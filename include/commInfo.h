#ifndef _COMMINFO_H_
#define _COMMINFO_H_

#include "rk_comm_video.h"

typedef enum {
    MOD_ID_XDMA_VIDEO,
    MOD_ID_VI,
    MOD_ID_VPSS,
    MOD_ID_VENC,
    MOD_ID_VDEC,
    MOD_ID_VO,
    MOD_ID_FPGA
}RK_MODE_TYPE_E;

//公共图像帧  结构体   暂定的成员        所有模块发送接收，都应按照这个结构体来搞
typedef struct img_info{

    void *pkt; //数据地址

    size_t size; //图像大小

    int32_t timeOut; //超时时间

    size_t frameNum; //图像计数,帧号

    PIXEL_FORMAT_E format; //图像属性
    
    uint32_t width;//图像宽
    
    uint32_t height;//图像高
    
    uint32_t hor_stride;//图像宽跨距
    
    uint32_t ver_stride;//图像高跨距

}IMG_SRC_INFO_S;

typedef int (*inputFrameFunc)(int chn, IMG_SRC_INFO_S *inputImg, int milliSec);

typedef int (*pkt_callback_type)(const void *frame, const size_t size);

typedef enum COMMON_ENCODE_FRAME_TYPE {
    COMMON_ENCODE_FRAME_TYPE_IDR = 1,
    COMMON_ENCODE_FRAME_TYPE_I = 1,
    COMMON_ENCODE_FRAME_TYPE_P,
    COMMON_ENCODE_FRAME_TYPE_B,
    COMMON_ENCODE_FRAME_TYPE_INV
}COMMON_ENCODE_FRAME_TYPE_E;


// 若只使用数据和数据长度参数，则直接将chn置为 -1 即可

typedef int (*pktCallbackType)(int chn, void *srcPkt, size_t srcPktLen, int mill);

//先暂时这样加一个文件名，后续在修改为Bind-Function函数将成员函数作为回调传入，现在先不管了。
typedef int (*pktCallbackWriteFile)(const char *file, void *srcPkt, size_t srcPktLen);

typedef struct vencChnInfo{
    int chn;
    int timeOut;
}vencChnInfo_S;
typedef struct viChnInfo{
    int dev;
    int chn;
    int timeOut;
}viChnInfo_S;

typedef struct vpssChnInfo{
    int grp;
    int chn;
    int timeOut;
}vpssChnInfo_S;

typedef struct voChnInfo{
    int layer;
    int chn;
    int timeOut;
}voChnInfo_S;

typedef struct vdecChnInfo{
    int chn;
    int timeOut;
}vdecChnInfo_S;

typedef struct xdmaChnInfo{
    int dev;
    int chn;
    int timeOut;
}xdmaVideoChnInfo_S;

typedef struct fpgaChnInfo{
    int chn;
    bool userSpace; ///确认是MPP还是用户申请内存
    int timeOut; //无用配置
}fpgaChnInfo_S;

typedef struct imgChnInfo {
    RK_MODE_TYPE_E modType;
    union {
        viChnInfo_S   vi;
        vpssChnInfo_S vpss;
        vencChnInfo_S venc;
        vdecChnInfo_S vdec;
        voChnInfo_S   vo;
        xdmaVideoChnInfo_S xdmaVideo;
        fpgaChnInfo_S img2Fpga;
    };
}imgChnInfo_S;


#endif //!_COMMINFO_H_
