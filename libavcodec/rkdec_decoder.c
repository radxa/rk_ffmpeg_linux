#include <dlfcn.h>  // for dlopen/dlclose
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>

#include "rga.h"
#include "vpu.h"
#include "vpu_global.h"
#include "vpu_api_private.h"

#include "avcodec.h"
#include "internal.h"
#include "mpegvideo.h"

#include "log.h"

#define DIV3_TAG 861292868

const char *RK_DEC_LIB = "/usr/lib/librk_dec.so";
const char *RK_VPU_LIB = "/usr/lib/libvpu.so";

//#define FAKE_DECODE

typedef RK_S32 (*VpuMemFunc)(VPUMemLinear_t *p);
typedef RK_S32 (*VpuContextFunc)(VpuCodecContext_t **ctx);

typedef struct _RkVpuDecContext {
    OMX_DEC_VIDEO_CODINGTYPE    video_type;
    VPU_API                     dec_api;
    VpuCodecContext_t*		ctx;
    DecoderOut_t		decOut;
    VideoPacket_t		demoPkt;
    VpuMemFunc			vpu_mem_link;
    VpuMemFunc			vpu_free_linear;
    VpuMemFunc			vpu_mem_invalidate;
    VpuMemFunc			vpu_mem_getfd;

    RK_S32 (*vpu_mem_dup)(VPUMemLinear_t *dst, VPUMemLinear_t *src);

    VpuContextFunc		vpu_open;
    VpuContextFunc		vpu_close;
    void*			dec_handle;
    void*			vpu_handle;
    int                         rga_fd;
    int                         nal_length_size;
    int                         mpeg2_err_frm_num;
    bool                        header_send_flag;
    VPUMemLinear_t  front_vpumem;
} RkVpuDecContext;

static bool rkdec_hw_support(enum AVCodecID codec_id, unsigned int codec_tag) {
    switch(codec_id){
        case AV_CODEC_ID_MPEG1VIDEO:
        case AV_CODEC_ID_MPEG2VIDEO:
        case AV_CODEC_ID_MPEG4:
        case AV_CODEC_ID_FLV1:
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_RV30:
        case AV_CODEC_ID_RV40:
        case AV_CODEC_ID_VC1:
        case AV_CODEC_ID_WMV3:
        case AV_CODEC_ID_VP8:
        case AV_CODEC_ID_HEVC:
            return true;
        case AV_CODEC_ID_MSMPEG4V3:
            {
                if(DIV3_TAG == codec_tag){
                    return true;
                }
            }
        default:
            return false;
    }
    return false;
}

static bool init_declib(RkVpuDecContext* rkdec_ctx) {

    if (rkdec_ctx->dec_handle == NULL) {
        rkdec_ctx->dec_handle = dlopen(RK_DEC_LIB, RTLD_NOW | RTLD_DEEPBIND);
    }

    if (rkdec_ctx->dec_handle == NULL) {
        ALOGI("Failed to open RK_DEC_LIB");
    }

    if (rkdec_ctx->vpu_handle == NULL) {
        rkdec_ctx->vpu_handle = dlopen(RK_VPU_LIB, RTLD_NOW | RTLD_DEEPBIND);
    }
    if (rkdec_ctx->vpu_handle == NULL) {
        ALOGI("Failed to open RK_VPU_LIB");
    }

    return rkdec_ctx->dec_handle != NULL && rkdec_ctx->vpu_handle != NULL;
}

static int rkdec_prepare(AVCodecContext* avctx)
{
    RkVpuDecContext* rkdec_ctx = avctx->priv_data;

    if (!init_declib(rkdec_ctx)) {
        ALOGE("init_decdeclib failed");
        return -1;
    }

    rkdec_ctx->vpu_open = (VpuContextFunc) dlsym(rkdec_ctx->dec_handle, "vpu_open_context");
    rkdec_ctx->vpu_close = (VpuContextFunc) dlsym(rkdec_ctx->dec_handle, "vpu_close_context");
    rkdec_ctx->vpu_mem_link = (VpuMemFunc) dlsym(rkdec_ctx->vpu_handle, "VPUMemLink");
    rkdec_ctx->vpu_free_linear = (VpuMemFunc) dlsym(rkdec_ctx->vpu_handle, "VPUFreeLinear");
    rkdec_ctx->vpu_mem_invalidate = (VpuMemFunc) dlsym(rkdec_ctx->vpu_handle, "VPUMemInvalidate");
    rkdec_ctx->vpu_mem_getfd = (VpuMemFunc) dlsym(rkdec_ctx->vpu_handle, "VPUMemGetFD");
    rkdec_ctx->vpu_mem_dup = (VpuMemFunc) dlsym(rkdec_ctx->vpu_handle, "VPUMemDuplicate");

    if (rkdec_ctx->vpu_open(&rkdec_ctx->ctx) || rkdec_ctx->ctx == NULL) {
        ALOGE("vpu_open_context failed");
        return -1;
    }

    rkdec_ctx->ctx->codecType = CODEC_DECODER;
    rkdec_ctx->ctx->videoCoding = rkdec_ctx->video_type;
    rkdec_ctx->ctx->width = avctx->width;
    rkdec_ctx->ctx->height = avctx->height;
    rkdec_ctx->ctx->no_thread = 1;
    rkdec_ctx->ctx->enableparsing = 1;

    if (rkdec_ctx->ctx->init(rkdec_ctx->ctx, avctx->extradata, avctx->extradata_size) != 0) {
        ALOGE("ctx init failed");
        return -1;
    }
#if 0
    rkdec_ctx->rga_fd = open("/dev/rga",O_RDWR,0);
    if (rkdec_ctx->rga_fd < 0) {
        ALOGE("failed to open rga.");
        return false;
    }
#endif
    return 0;
}

static int rkdec_init(AVCodecContext *avctx) {
    ALOGD("rkdec_init");
#ifndef FAKE_DECODE
    RkVpuDecContext* rkdec_ctx = avctx->priv_data;
    if (!rkdec_hw_support(avctx->codec_id, avctx->codec_tag)) {
        return -1;
    }
    memset(rkdec_ctx, 0, sizeof(RkVpuDecContext));
    rkdec_ctx->header_send_flag = false;
    rkdec_ctx->video_type = OMX_DEC_VIDEO_CodingUnused;
    switch(avctx->codec_id){
        case AV_CODEC_ID_MPEG1VIDEO:
        case AV_CODEC_ID_MPEG2VIDEO:
            rkdec_ctx->video_type = OMX_DEC_VIDEO_CodingMPEG2;
            break;
        case AV_CODEC_ID_MJPEG:
            rkdec_ctx->video_type = OMX_DEC_VIDEO_CodingMJPEG;
            break;
        case AV_CODEC_ID_MPEG4:
        case AV_CODEC_ID_MSMPEG4V3:
            rkdec_ctx->video_type = OMX_DEC_VIDEO_CodingMPEG4;
            break;
        case AV_CODEC_ID_FLV1:
            rkdec_ctx->video_type = OMX_DEC_VIDEO_CodingFLV1;
            break;
        case AV_CODEC_ID_H264:
            rkdec_ctx->video_type = OMX_DEC_VIDEO_CodingAVC;
            break;
        case AV_CODEC_ID_RV30:
        case AV_CODEC_ID_RV40:
            rkdec_ctx->video_type = OMX_DEC_VIDEO_CodingRV;
            break;
        case AV_CODEC_ID_VC1:
        case AV_CODEC_ID_WMV3:
            rkdec_ctx->video_type = OMX_DEC_VIDEO_CodingWMV;
            break;
        case AV_CODEC_ID_VP6:
        case AV_CODEC_ID_VP6F:
        case AV_CODEC_ID_VP6A:
            rkdec_ctx->video_type = OMX_DEC_VIDEO_CodingVP6;
            break;
        case AV_CODEC_ID_VP8:
            rkdec_ctx->video_type = OMX_DEC_VIDEO_CodingVP8;
            break;
        case AV_CODEC_ID_HEVC:
            rkdec_ctx->video_type = OMX_RK_VIDEO_CodingHEVC;
            break;
        default:
            ALOGE("no support");
            break;
    }

    avctx->pix_fmt = AV_PIX_FMT_YUV420P;

    return rkdec_prepare(avctx);
#else
    avctx->pix_fmt = AV_PIX_FMT_YUV420P;
    return 0;
#endif
}

static int rkdec_deinit(AVCodecContext *avctx) {
#ifndef FAKE_DECODE
    RkVpuDecContext* rkdec_ctx = avctx->priv_data;
    rkdec_ctx->vpu_close(&rkdec_ctx->ctx);
    if (rkdec_ctx->front_vpumem.vir_addr)
        rkdec_ctx->vpu_free_linear(&rkdec_ctx->front_vpumem);
#if 0
    if (rkdec_ctx->rga_fd > 0) {
        close(rkdec_ctx->rga_fd);
        rkdec_ctx->rga_fd = 0;
    }
#endif
#endif
    ALOGD("rkdec_deinit");
    return 0;
}

static void yuv420sp_2_420p(char* y_src, char* y_dst, char* uv_src,
        void* u_dst, void* v_dst, int pixels) {
    const int uv_size = pixels >> 1;
    const int u_size = uv_size >> 1;
    char* buf = (char*) av_malloc(uv_size);
    char* u_buf = buf;
    char* v_buf = buf + u_size;

    int i = 0;
    for (i = 0; i < u_size; i++) {
        u_buf[i] = uv_src[2 * i];
        v_buf[i] = uv_src[2 * i + 1];
    }
    if (y_src != y_dst)
        memcpy(y_dst, y_src, pixels);
    memcpy(u_dst, u_buf, u_size);
    memcpy(v_dst, v_buf, u_size);
    av_free(buf);
}

static int rkdec_decode_frame(AVCodecContext *avctx/*ctx*/, void *data/*AVFrame*/,
        int *got_frame/*frame count*/, AVPacket *packet/*src*/) {
    int ret = 0;
#ifndef FAKE_DECODE
    RkVpuDecContext* rkdec_ctx = avctx->priv_data;
    VpuCodecContext_t* ctx = rkdec_ctx->ctx;
    VideoPacket_t* pDemoPkt = &rkdec_ctx->demoPkt;
    DecoderOut_t* pDecOut = &rkdec_ctx->decOut;

    pDemoPkt->data = packet->data;
    pDemoPkt->size = packet->size;

    if (packet->pts != AV_NOPTS_VALUE) {
        pDemoPkt->pts = av_q2d(avctx->time_base)*(packet->pts)*1000000ll;
    } else {
        pDemoPkt->pts = av_q2d(avctx->time_base)*(packet->dts)*1000000ll;
    }

    memset(pDecOut, 0, sizeof(DecoderOut_t));
    pDecOut->data = (RK_U8 *)(av_malloc)(sizeof(VPU_FRAME));
    if (pDecOut->data == NULL) {
        ALOGE("malloc VPU_FRAME failed");
        goto out;
    }

    memset(pDecOut->data, 0, sizeof(VPU_FRAME));
    pDecOut->size = 0; 

#if 1
    if (ctx->decode_sendstream(ctx, pDemoPkt) != 0) {
        ALOGE("send packet failed");
    }

    if ((ret = ctx->decode_getframe(ctx, pDecOut)) == 0) {
#else
        if ((ret = ctx->decode(ctx, pDemoPkt, pDecOut)) == 0) {
#endif
            if (pDecOut->size && pDecOut->data) {
                AVFrame *frame = data;
                if ((ret = ff_get_buffer(avctx, frame, 0)) < 0) {
                    ALOGE("Failed to get buffer!!!:%d", ret);
                    goto out;
                }
                if (rkdec_ctx->front_vpumem.phy_addr) {
                    rkdec_ctx->vpu_free_linear(&rkdec_ctx->front_vpumem);
                }

                /* modify by zhaojun for load decoded data from physical mem to virtual mem */
                VPU_FRAME *pframe = (VPU_FRAME *) (pDecOut->data);
                rkdec_ctx->vpu_mem_link(&pframe->vpumem);
                rkdec_ctx->vpu_mem_invalidate(&pframe->vpumem);
                //memcpy(&rkdec_ctx->front_vpumem, &pframe->vpumem, sizeof(pframe->vpumem));
                rkdec_ctx->vpu_mem_dup(&rkdec_ctx->front_vpumem, &pframe->vpumem);
                RK_U32 wAlign16 = ((pframe->DisplayWidth + 15) & (~15));
                RK_U32 hAlign16 = ((pframe->DisplayHeight + 15) & (~15));
                RK_U32 frameSize = wAlign16 * hAlign16 * 3 / 2;
                const int u_size = frameSize >> 2;
                uint8_t *buffer = pframe->vpumem.vir_addr;//(uint8_t *)av_malloc(frameSize);
                int dma_fd = rkdec_ctx->vpu_mem_getfd(&rkdec_ctx->front_vpumem);
                uint8_t *tmpBuffer = buffer + wAlign16 * hAlign16;
                rkdec_ctx->vpu_free_linear(&pframe->vpumem);
#if 0
                pframe->vpumem.vir_addr = mmap(NULL, frameSize, PROT_READ | PROT_WRITE, MAP_SHARED, dma_fd, 0);

                frame->data[0] = pframe->vpumem.vir_addr;
                frame->data[1] = tmpBuffer;
                frame->data[2] = tmpBuffer;
#endif
                frame->data[3] = dma_fd;
		frame->linesize[3] = pframe->FrameWidth;
		frame->linesize[4] = pframe->FrameHeight;
#if 0
                memcpy(buffer, pframe->vpumem.vir_addr, frameSize);
                for (int i = 0;i < hAlign16;i++) {
                    memcpy((frame->data[0] + i * frame->linesize[0]), (buffer + i * wAlign16), wAlign16);
                }
                for (int i = 0;i < (hAlign16 >> 1);i++) {
                    for (int j = 0;j < (wAlign16 >> 1);j++) {
                        *(frame->data[1] + j + i * frame->linesize[1]) = *(tmpBuffer + 2 * j + i * wAlign16);
                        *(frame->data[2] + j + i * frame->linesize[2]) = *(tmpBuffer + 2 * j + 1 + i * wAlign16);
                    }
                }
#endif
                //rkdec_ctx->vpu_free_linear(&pframe->vpumem);
                /* end modify 2015/04/21 */

                *got_frame = 1;
            }
        } else {
            ALOGE("get frame failed ret %d", ret);
            goto out;
        }

        ret = 0;
out:
        if (pDecOut->data != NULL) {
            av_free(pDecOut->data);
            pDecOut->data = NULL;
        }
        if (ret < 0) {
            ALOGE("Something wrong during decode!!!");
        }

        return (ret < 0) ? ret : packet->size;

#else

        AVFrame *frame     = data;
        if ((ff_get_buffer(avctx, frame, 0)) < 0) {
            ALOGE("Failed to get buffer!!!");
            return -1;
        }

        frame->key_frame = 1;
        frame->pict_type = AV_PICTURE_TYPE_I;

        *got_frame = 1;
        return packet->size;
#endif

    }

    static void rkdec_decode_flush(AVCodecContext *avctx) {
        ALOGD("rkdec_decode_reset");
#ifndef FAKE_DECODE
        RkVpuDecContext* rkdec_ctx = avctx->priv_data;
        rkdec_ctx->ctx->flush(rkdec_ctx->ctx);
#endif
    }

#define DECLARE_RKDEC_VIDEO_DECODER(TYPE, CODEC_ID)                     \
    AVCodec ff_##TYPE##_decoder = {                                         \
        .name           = #TYPE,                                            \
        .long_name      = NULL_IF_CONFIG_SMALL(#TYPE " hw decoder"),        \
        .type           = AVMEDIA_TYPE_VIDEO,                               \
        .id             = CODEC_ID,                                         \
        .priv_data_size = sizeof(RkVpuDecContext),                          \
        .init           = rkdec_init,                                       \
        .close          = rkdec_deinit,                                     \
        .decode         = rkdec_decode_frame,                               \
        .capabilities   = CODEC_CAP_DELAY,                                  \
        .flush          = rkdec_decode_flush,                               \
        .pix_fmts       = (const enum AVPixelFormat[]) {                    \
            AV_PIX_FMT_YUV420P,						\
            AV_PIX_FMT_NONE							\
        },									\
    };                                                                  

DECLARE_RKDEC_VIDEO_DECODER(mpegvideo_rkvpu, AV_CODEC_ID_MPEG2VIDEO)
DECLARE_RKDEC_VIDEO_DECODER(mpeg1video_rkvpu, AV_CODEC_ID_MPEG1VIDEO)
DECLARE_RKDEC_VIDEO_DECODER(mpeg4_rkvpu, AV_CODEC_ID_MPEG4)
DECLARE_RKDEC_VIDEO_DECODER(flv_rkvpu, AV_CODEC_ID_FLV1)
DECLARE_RKDEC_VIDEO_DECODER(h264_rkvpu, AV_CODEC_ID_H264)
DECLARE_RKDEC_VIDEO_DECODER(hevc_rkvpu, AV_CODEC_ID_HEVC)
DECLARE_RKDEC_VIDEO_DECODER(rv30_rkvpu, AV_CODEC_ID_RV30)
DECLARE_RKDEC_VIDEO_DECODER(rv40_rkvpu, AV_CODEC_ID_RV40)
DECLARE_RKDEC_VIDEO_DECODER(vc1_rkvpu, AV_CODEC_ID_VC1)
DECLARE_RKDEC_VIDEO_DECODER(wmv3_rkvpu, AV_CODEC_ID_WMV3)
DECLARE_RKDEC_VIDEO_DECODER(vp8_rkvpu, AV_CODEC_ID_VP8)
DECLARE_RKDEC_VIDEO_DECODER(msmpeg4v3_rkvpu, AV_CODEC_ID_MSMPEG4V3)
