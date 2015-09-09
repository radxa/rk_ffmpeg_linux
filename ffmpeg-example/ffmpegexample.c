/*    gcc -o videoframe videoframe.c -lavformat -lavcodec -lavutil -lz -lm -lpthread -lswscale  */  
#include <libavformat/avformat.h>  
#include <libavcodec/avcodec.h>  
#include <libavutil/avutil.h>  
#include <libswscale/swscale.h>  
#include <stdio.h>  
#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>  
#include <stdlib.h>  
#include <sys/mman.h> 
#include <drm_fourcc.h>
#include "dev.h"
#include "bo.h"
#include "modeset.h"
#include <sys/time.h> 
#include <time.h>
#include "xf86drm.h"                                                            
#include "xf86drmMode.h"

int g_fps = 0;

double g_cl;
double g_last_cl;

void GetFPS()
{

	struct timeval tv;
	gettimeofday(&tv, NULL);
	g_cl = tv.tv_sec + (double)tv.tv_usec / 1000000;
	if (!g_last_cl)
		g_last_cl = g_cl;
	if(g_cl - g_last_cl > 1.0f) {
		g_last_cl = g_cl;
		printf("\n>>>>>fps=%d\n", g_fps);
		g_fps = 1;
	}
	else {
		g_fps++;
	}
}

static void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame)  
{  
	FILE *pFile;  
	char szFilename[32];  
	int y;  
	sprintf(szFilename, "frame%d.ppm", iFrame);  
	pFile = fopen(szFilename, "wb");  
	if(!pFile)  
		return;  
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);  //ppm 文件头  
	for(y=0; y<height; y++)  
		fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);  
	fclose(pFile);  
}  

int main(int argc, const char *argv[])  
{  
	AVFormatContext *pFormatCtx = NULL;  
	int i, videoStream;  
	AVCodecContext *pCodecCtx;  
	AVCodec *pCodec;  
	AVFrame *pFrame;  
	AVPacket packet;  
	int frameFinished;  
	int numBytes;  
	uint8_t *buffer;  
	int fbfd = -1;
	struct fb_var_screeninfo vinfo;  
	unsigned long screensize = 0;
	char *fbp = 0;
	int ret;
	struct sp_dev *dev;
	struct sp_plane **plane = NULL;
	struct sp_crtc *test_crtc;
	struct sp_plane *test_plane;
	int num_test_planes;

	dev = create_sp_dev();
	if (!dev) {
		printf("Failed to create sp_dev\n");
		return -1;
	}

	ret = initialize_screens(dev);
	if (ret) {
		printf("Failed to initialize screens\n");
		return -1;
	}
	plane = calloc(dev->num_planes, sizeof(*plane));
	if (!plane) {
		printf("Failed to allocate plane array\n");
		return -1;
	}

	test_crtc = &dev->crtcs[0];
	num_test_planes = test_crtc->num_planes;
	for (i = 0; i < test_crtc->num_planes; i++) {
		plane[i] = get_sp_plane(dev, test_crtc);
		if (is_supported_format(plane[i], DRM_FORMAT_NV12))
			test_plane = plane[i];
	}
	if (!test_plane)
		return -1;

	av_register_all();  
	if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {  
		return -1;  
	}  
	if(avformat_find_stream_info(pFormatCtx, NULL) < 0) {  
		return -1;  
	}  
	av_dump_format(pFormatCtx, -1, argv[1], 0);  
	videoStream = -1;  
	for(i=0; i<pFormatCtx->nb_streams; i++)  
		if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {  
			videoStream = i;  
			break;  
		}  
	if(videoStream == -1) {  
		return -1;  
	}  
	pCodecCtx = pFormatCtx->streams[videoStream]->codec;  
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);  
	if(pCodec == NULL) {  
		return -1;  
	}  
	if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {  
		return -1;  
	}  
	pFrame = avcodec_alloc_frame();  
	if(pFrame == NULL) {  
		return -1;  
	}  
	numBytes = avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);  
	buffer = av_malloc(numBytes);  
	while(av_read_frame(pFormatCtx, &packet) >=0) {  
		if(packet.stream_index == videoStream) {  
			void* map_addr;
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);  
			if(frameFinished) {  
				int ret;
				struct drm_mode_create_dumb cd;
				struct sp_bo *bo;
				int wAlign16 = ((pCodecCtx->width + 15) & (~15));
				int hAlign16 = ((pCodecCtx->height + 15) & (~15));
				int frameSize = wAlign16 * hAlign16 * 3 / 2;
				int dmafd = pFrame->data[3];
				uint32_t handles[4], pitches[4], offsets[4];

				bo = calloc(1, sizeof(*bo));
				if (!bo)
					return -1;

				ret = drmPrimeFDToHandle(dev->fd, dmafd, &bo->handle);
				bo->dev = dev;
				bo->width = wAlign16;
				bo->height = pCodecCtx->height;
				bo->depth = 16;
				bo->bpp = 32;
				bo->format = DRM_FORMAT_NV12;
				bo->flags = 0;

				handles[0] = bo->handle;
				pitches[0] = wAlign16;
				offsets[0] = 0;
				handles[1] = bo->handle;
				pitches[1] = wAlign16;
				offsets[1] = wAlign16 * hAlign16;
				ret = drmModeAddFB2(bo->dev->fd, bo->width, bo->height,
						bo->format, handles, pitches, offsets,
						&bo->fb_id, bo->flags);
				if (ret) {
					printf("failed to create fb ret=%d\n", ret);
					return ret;
				}

				ret = drmModeSetPlane(dev->fd, test_plane->plane->plane_id,
					test_crtc->crtc->crtc_id, bo->fb_id, 0, 0, 0,
					test_crtc->crtc->mode.hdisplay,
					test_crtc->crtc->mode.vdisplay,
					0, 0, bo->width << 16, bo->height << 16);
				if (ret) {
					printf("failed to set plane to crtc ret=%d\n", ret);
					return ret;
				}
				if (test_plane->bo) {
					if (test_plane->bo->fb_id) {
						ret = drmModeRmFB(dev->fd, test_plane->bo->fb_id);
						if (ret)
							printf("Failed to rmfb ret=%d!\n", ret);
					}
					if (test_plane->bo->handle) {
						struct drm_gem_close req = {
							.handle = test_plane->bo->handle,
						};

						drmIoctl(bo->dev->fd, DRM_IOCTL_GEM_CLOSE, &req);
					}
					free(test_plane->bo);
				}
				test_plane->bo = bo;

				GetFPS();
			}
		}  
		av_free_packet(&packet);  
	}  
	av_free(buffer);  
	av_free(pFrame);  
	avcodec_close(pCodecCtx);  
	avformat_close_input(&pFormatCtx);  
	return 0;  
}  
