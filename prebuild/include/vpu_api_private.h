#ifndef VPU_API_PRIVATE_H_
#define VPU_API_PRIVATE_H_

#include "vpu_global.h"
#include "vpu_api.h"
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_STREAM_LENGHT  1024*1024  //for encoder out put stream
#define DEFAULT_FRAME_BUFFER_DEPTH 4

/* interlace filed mode support */
typedef enum {
    DEINTER_INPUT_EOS_REACHED   = 0x01,
}DEINTER_FLAG_MAP;

typedef enum DEINTER_CMD {
    DEINT_GET_PRE_PROCESS_COUNT,
    DEINT_GET_FRAME_COUNT,
    DEINT_SET_INPUT_EOS_REACH,
} DEINTER_CMD;

typedef enum DEINTER_STATUS_TYPE {
    DEINTER_NOT_INIT            = -1,
    DEINTER_OK                  = 0,
    DEINTER_INIT_FAIL           = 1,
    DEINTER_ERR_FATAL_THREAD    = 2,
    DEINTER_ERR_OPEN_DEV        = 3,
    DEINTER_ERR_TEST_DEV        = 4,
    DEINTER_ERR_LIST_PRE_DEINT  = 5,
    DEINTER_ERR_ERR_LIST_STREAM = 6,

    DEINTER_ERR_UNKNOW          = 100,
    DEINTER_BUTT,
}DEINTER_STATUS_TYPE;

typedef struct DeinterContext {
    VPU_FRAME  deintBuf;
    VPU_FRAME  originBuf;
    int32_t    poll_flag;
}DeinterContext_t;

typedef enum {
    NEED_FLUSH_DEC_DPB          = 0x01,
    ENABLE_DEINTERLACE_SUPPORT  = 0x02,
    INTERLACE_FIELD_MODE        = 0x04,

}VPU_API_PRIVATE_FLAG_MAP;

// trick: put vpu_mem at head of node and make stream destroy simple
typedef struct {
    uint8_t* buf;
    uint32_t size;
    int64_t timestamp;
    int32_t usePts;
} stream_packet;

typedef enum VPU_CODEC_CMD {
    VPU_GET_STREAM_COUNT,
    VPU_GET_FRAME_COUNT,
} VPU_CODEC_CMD;

typedef struct Stream_describe
{
    uint8_t                 flag[4];
    uint32_t                size;
    uint32_t                timel;
    uint32_t                timeh;
    uint32_t                type;
    uint32_t                slices;
    uint32_t                retFlag;
    uint32_t                res[1];
    uint32_t                slicesize[128];
}Stream_describe;

/* Dec decoder Input structure */
typedef struct DecDecInput
{
    uint8_t *pStream;               /* Pointer to the input */
    uint32_t streamBusAddress;      /* DMA bus address of the input stream */
    uint32_t dataLen;               /* Number of bytes to be decoded         */
    uint32_t picId;                 /* Identifier for the picture to be decoded */
    uint32_t skipNonReference;      /* Flag to enable decoder skip non-reference
                                    * frames to reduce processor load */
    int64_t  timeUs;
    int32_t usePts;
}DecDecInput_t;

/* Dec decoder Output structure */
typedef struct DecDecOutput
{
    uint32_t dataLeft;              /* how many bytes left undecoded */
}DecDecOutput_t;

typedef struct DecM2vTimeAmend {

    int32_t  count;
    int64_t  frameDelayUs;
    int64_t  originTimeUs;
    int64_t  lastTimeUs;
    bool     needReset;
}DecM2vTimeAmend_t;

typedef struct Mpeg2HwStatus {
    int32_t  errFrmNum;
    int32_t  tolerance;
}Mpeg2HwStatus_t;

typedef struct H264HwStatus {
    int32_t  errFrmNum;
    int32_t  tolerance;
}H264HwStatus_t;


typedef struct DecDecExt
{
    int32_t headSend;
    Mpeg2HwStatus_t m2vHwStatus;
    H264HwStatus_t  h264HwStatus;
}DecDecExt_t;

typedef enum CODEC_CFG_VALUE {
    CFG_VALUE_UNKNOW,
    CFG_VALUE_SUPPORT,
    CFG_VALUE_NOT_SUPPORT
} CODEC_CFG_VALUE;

typedef struct DecCodecConfigure {
    CODEC_TYPE codecType;
    OMX_DEC_VIDEO_CODINGTYPE videoCoding;
    uint8_t old_cfg[10];
    const char* cfg;
    CODEC_CFG_VALUE value;
}DecCodecConfigure_t;

typedef struct CfgFile {
    FILE*       file;
    uint32_t    file_size;
    uint32_t    read_pos;
}CfgFile_t;

typedef struct VpuApiCodecCfg {
    CfgFile_t   cfg_file;
    uint8_t*    buf;
}VpuApiCodecCfg_t;

typedef struct VpuApiTimeStampTrace {
    int64_t     pre_timeUs;
    uint32_t    same_cnt;
    uint32_t    init;
}VpuApiTimeStampTrace_t;

#ifdef __cplusplus
extern "C"
{
#endif

extern void*         get_class_RkHevcDecoder(void);
extern int          init_class_RkHevcDecoder(void * HevcDecoder, uint8_t* extra_data, uint32_t extra_size);
extern void      destroy_class_RkHevcDecoder(void * HevcDecoder);
extern int        deinit_class_RkHevcDecoder(void * HevcDecoder);
extern int  dec_oneframe_class_RkHevcDecoder(void * HevcDecoder, uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize, int64_t *InputTimestamp);
extern int reset_class_RkHevcDecoder(void * HevcDecoder);
extern int flush_oneframe_in_dpb_class_RkHevcDecoder(void *HevcDecoder, uint8_t* aOutBuffer, uint32_t *aOutputLength);
extern int perform_seting_class_RkHevcDecoder(void * HevcDecoder,VPU_API_CMD cmd,void *param);

extern void*         get_class_DecAvcDecoder(void);
extern int          init_class_DecAvcDecoder(void * AvcDecoder, int32_t tsFlag);
extern void      destroy_class_DecAvcDecoder(void * AvcDecoder);
extern int        deinit_class_DecAvcDecoder(void * AvcDecoder);
extern int  dec_oneframe_class_DecAvcDecoder(void * AvcDecoder, uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize, int64_t *InputTimestamp);
extern int reset_class_DecAvcDecoder(void * AvcDecoder);
extern int flush_oneframe_in_dpb_class_DecAvcDecoder(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength);
extern int  perform_seting_class_DecAvcDecoder(void * AvcDecoder,VPU_API_CMD cmd,void *param);

extern void*         get_class_DecM4vDecoder(void);
extern void      destroy_class_DecM4vDecoder(void * M4vDecoder);
extern int          init_class_DecM4vDecoder(void * M4vDecoder, VPU_GENERIC *vpug);
extern int        deinit_class_DecM4vDecoder(void * M4vDecoder);
extern int         reset_class_DecM4vDecoder(void * M4vDecoder);
extern int  dec_oneframe_class_DecM4vDecoder(void * M4vDecoder,uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize);
extern int  flush_oneframe_in_dpb_class_DecM4vDecoder(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength);
extern int  perform_seting_class_DecM4vDecoder(void * M4vDecoder,VPU_API_CMD cmd,void *param);

extern void*        get_class_DecH263Decoder(void);
extern void     destroy_class_DecH263Decoder(void * H263Decoder);
extern int         init_class_DecH263Decoder(void * H263Decoder, VPU_GENERIC *vpug);
extern int       deinit_class_DecH263Decoder(void * H263Decoder);
extern int        reset_class_DecH263Decoder(void * H263Decoder);
extern int dec_oneframe_class_DecH263Decoder(void * H263Decoder,uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize);
extern int  flush_oneframe_in_dpb_class_DecH263Decoder(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength);
extern int   perform_seting_class_DecH263Decoder(void * H263Decoder,VPU_API_CMD cmd,void *param);

extern void*         get_class_DecM2vDecoder(void);
extern void      destroy_class_DecM2vDecoder(void * M2vDecoder);
extern int          init_class_DecM2vDecoder(void * M2vDecoder);
extern int        deinit_class_DecM2vDecoder(void * M2vDecoder);
extern int         reset_class_DecM2vDecoder(void * M2vDecoder);
extern int  dec_oneframe_class_DecM2vDecoder(void * M2vDecoder,uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize);
extern int  get_oneframe_class_DecM2vDecoder(void * M2vDecoder, uint8_t* aOutBuffer, uint32_t* aOutputLength);
extern int  flush_oneframe_in_dpb_class_DecM2vDecoder(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength);
extern int   perform_seting_class_DecM2vDecoder(void * M2vDecoder,VPU_API_CMD cmd,void *param);

extern void*          get_class_DecRvDecoder(void);
extern void       destroy_class_DecRvDecoder(void * RvDecoder);
extern int           init_class_DecRvDecoder(void * RvDecoder);
extern int         deinit_class_DecRvDecoder(void * RvDecoder);
extern int          reset_class_DecRvDecoder(void * RvDecoder);
extern int   dec_oneframe_class_DecRvDecoder(void * RvDecoder,uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize);
extern void         set_width_Height_class_DecRvDecoder(void * RvDecoder, uint32_t* width, uint32_t* height);
extern int   perform_seting_class_DecRvDecoder(void * RvDecoder,VPU_API_CMD cmd,void *param);

extern void*         get_class_DecVp8Decoder(void);
extern void      destroy_class_DecVp8Decoder(void * Vp8Decoder);
extern int          init_class_DecVp8Decoder(void * Vp8Decoder);
extern int        deinit_class_DecVp8Decoder(void * Vp8Decoder);
extern int         reset_class_DecVp8Decoder(void * Vp8Decoder);
extern int  dec_oneframe_class_DecVp8Decoder(void * Vp8Decoder,uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize);
extern int   perform_seting_class_DecVp8Decoder(void * Vp8Decoder,VPU_API_CMD cmd,void *param);

extern void*         get_class_DecVc1Decoder(void);
extern void      destroy_class_DecVc1Decoder(void * Vc1Decoder);
extern int          init_class_DecVc1Decoder(void * Vc1Decoder, uint8_t* tmpStrm, uint32_t size,uint32_t extraDataSize);
extern int        deinit_class_DecVc1Decoder(void * Vc1Decoder);
extern int         reset_class_DecVc1Decoder(void * Vc1Decoder);
extern int  dec_oneframe_class_DecVc1Decoder(void * Vc1Decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize, int64_t *InputTimestamp);
extern int   perform_seting_class_DecVc1Decoder(void * Vc1Decoder,VPU_API_CMD cmd,void *param);

extern void*         get_class_DecVp6Decoder(void);
extern void      destroy_class_DecVp6Decoder(void * Vp6Decoder);
extern int          init_class_DecVp6Decoder(void * Vp6Decoder, int codecid);
extern int        deinit_class_DecVp6Decoder(void * Vp6Decoder);
extern int         reset_class_DecVp6Decoder(void * Vp6Decoder);
extern int  dec_oneframe_class_DecVp6Decoder(void * Vp6Decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize);
extern int   perform_seting_class_DecVp6Decoder(void * Vp6Decoder,VPU_API_CMD cmd,void *param);

extern void*         get_class_DecJpegDecoder(void);
extern void      destroy_class_DecJpegDecoder(void * JpegDecoder);
extern int          init_class_DecJpegDecoder(void * JpegDecoder);
extern int        deinit_class_DecJpegDecoder(void * JpegDecoder);
extern int         reset_class_DecJpegDecoder(void * JpegDecoder);
extern int  dec_oneframe_class_DecJpegDecoder(void * JpegDecoder, unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize);
extern int   perform_seting_class_DecJpegDecoder(void * JpegDecoder,VPU_API_CMD cmd,void *param);

extern void* get_class_DecAvcEncoder(void);
extern void  destroy_class_DecAvcEncoder(void * AvcEncoder);
extern int init_class_DecAvcEncoder(void * AvcEncoder, EncParameter_t *aEncOption, uint8_t* aOutBuffer,uint32_t * aOutputLength);
extern int deinit_class_DecAvcEncoder(void * AvcEncoder);
extern int enc_oneframe_class_DecAvcEncoder(void * AvcEncoder, uint8_t* aOutBuffer, uint32_t * aOutputLength, uint8_t* aInputBuf,uint32_t  aInBuffPhy,uint32_t *aInBufSize,uint32_t * aOutTimeStamp, int* aSyncFlag);
extern void set_config_class_DecAvcEncoder(void * AvcEncoder,EncParameter_t *vpug);
extern void get_config_class_DecAvcEncoder(void * AvcEncoder,EncParameter_t *vpug);
extern int set_idrframe_class_DecAvcEncoder(void * AvcEncoder);
extern int set_inputformat_class_DecAvcEncoder(void * AvcEncoder,H264EncPictureType inputFormat);

extern void *  get_class_DecVp8Encoder(void);
extern void  destroy_class_DecVp8Encoder(void * Vp8Encoder);
extern int init_class_DecVp8Encoder(void * Vp8Encoder, EncParameter_t *aEncOption, uint8_t* aOutBuffer,uint32_t* aOutputLength);
extern int deinit_class_DecVp8Encoder(void * Vp8Encoder);
extern int enc_oneframe_class_DecVp8Encoder(void * Vp8Encoder, uint8_t* aOutBuffer, uint32_t* aOutputLength,
                                     uint8_t *aInBuffer,uint32_t aInBuffPhy,uint32_t* aInBufSize,uint32_t* aOutTimeStamp, int *aSyncFlag);
extern void *  get_class_DecMjpegEncoder(void);
extern void  destroy_class_DecMjpegEncoder(void * MjpegEncoder);
extern int init_class_DecMjpegEncoder(void * MjpegEncoder, EncParameter_t *aEncOption, unsigned char* aOutBuffer,unsigned int* aOutputLength);
extern int deinit_class_DecMjpegEncoder(void * MjpegEncoder);
extern int enc_oneframe_class_DecMjpegEncoder(void *MjpegEncoder, unsigned char* aOutBuffer, unsigned int* aOutputLength,
                                     unsigned char *aInBuffer,unsigned int aInBuffPhy,unsigned int* aInBufSize,unsigned int* aOutTimeStamp, int *aSyncFlag);
#ifdef __cplusplus
}
#endif

typedef struct tag_VPU_API {
    void* (*         get_class_DecDecoder)(void);
    void  (*     destroy_class_DecDecoder)(void *decoder);
    int   (*      deinit_class_DecDecoder)(void *decoder);
    int   (*        init_class_DecDecoder)(void *decoder);
    int   (*        init_class_DecDecoder_with_extra)(void *decoder, uint8_t* extra_data, uint32_t extra_size);
    int   (*        init_class_DecDecoder_M4VH263)(void *decoder, VPU_GENERIC *vpug);
    int   (*        init_class_DecDecoder_VC1)(void *decoder, uint8_t *tmpStrm, uint32_t size,uint32_t extraDataSize);
    int   (*        init_class_DecDecoder_VP6)(void *decoder, int codecid);
    int   (*        init_class_DecDecoder_AVC)(void *decoder,int tsFlag);
    int   (*       reset_class_DecDecoder)(void *decoder);
    int   (*dec_oneframe_class_DecDecoder)(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize);
    int   (*dec_oneframe_class_DecDecoder_WithTimeStamp)(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize, int64_t* InputTimestamp);
    int   (*get_oneframe_class_DecDecoder)(void *decoder, uint8_t* aOutBuffer, uint32_t* aOutputLength);
    void  (*set_width_Height_class_DecDecoder_RV)(void *decoder, uint32_t* width, uint32_t* height);
    int   (*flush_oneframe_in_dpb_class_DecDecoder)(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength);
    int   (*perform_seting_class_DecDecoder)(void *decoder,VPU_API_CMD cmd,void *param);

	void* (*         get_class_DecEncoder)(void);
    void  (*     destroy_class_DecEncoder)(void *encoder);
    int   (*      deinit_class_DecEncoder)(void *encoder);
    int   (*        init_class_DecEncoder)(void *encoder, EncParameter_t *aEncOption, uint8_t *aOutBuffer, uint32_t* aOutputLength);
    int   (*enc_oneframe_class_DecEncoder)(void *encoder, uint8_t* aOutBuffer, uint32_t * aOutputLength,uint8_t* aInputBuf,uint32_t  aInBuffPhy,uint32_t *aInBufSize,uint32_t * aOutTimeStamp, int* aSyncFlag);
    void  (*enc_getconfig_class_DecEncoder)(void * AvcEncoder,EncParameter_t* vpug);
    void  (*enc_setconfig_class_DecEncoder)(void * AvcEncoder,EncParameter_t* vpug);
    int   (*enc_setInputFormat_class_DecEncoder)(void * AvcEncoder,H264EncPictureType inputFormat);
    int   (*enc_setIdrframe_class_DecEncoder)(void * AvcEncoder);
} VPU_API;

#endif

