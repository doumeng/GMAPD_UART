#ifndef _RKVENCMOD_H_
#define _RKVENCMOD_H_

#include "rkVencConfig.h"
#include "commInfo.h"
#include <thread>


typedef struct VENC_GETSTREAM_PARA {
    bool bThreadStart;
    VENC_CHN chn;
    int RtspSerNum;
    size_t dataLen;
    COMMON_ENCODE_FRAME_TYPE_E type;
    void* data;
}VENC_GETSTREAM_PARA_S;

typedef struct VENC_SEND_FRAME_INFO{
    RK_U32 u32Width;
    RK_U32 u32Height;
    RK_U64 u64MBSize;
    MB_POOL pool;
    PIXEL_FORMAT_E format;
}VENC_SEND_FRAME_INFO_S;

typedef int (*vencStreamCallback)(VENC_CHN, VENC_GETSTREAM_PARA_S*);

namespace vencFrame{
int inputFrame(int chn, VIDEO_FRAME_INFO_S *frame, RK_S32 milliSec=1000);
int inputFrame(int chn, IMG_SRC_INFO_S *inputImg, RK_S32 milliSec=1000);
int inputFrame(int chn, void *buff, RK_U32 size, RK_S32 milliSec=1000);

RK_S32 usrInserSeiData(VENC_CHN VeChn, const void *pu8Data, RK_U32 u32Len);

/// @brief 编码器动态修改码率
/// @param VeChn 编码通道
/// @param bitrate 码率
/// @return 
RK_S32 checkVencBitRate(VENC_CHN VeChn, RK_U32 bitrate);

}

class rkVencMod
{
private:
    rkVencConfig __config;
    bool vencIsCfg;

    VENC_GETSTREAM_PARA_S _stVencGetStreamFlag[VENC_MAX_CHN_NUM];

    //编译器回调函数
    std::map<int, std::shared_ptr<std::thread>> _pkt_thread_map;
    std::multimap<int, vencStreamCallback> _pkt_func_callback_multimap;

    //编码线程数据输出函数
    void* _GetVencStreamProc(void* p);

    RK_S32 _excCallback(VENC_CHN chn, VENC_GETSTREAM_PARA_S* param);

public:
    rkVencMod(/* args */);
    ~rkVencMod();

    RK_S32 Create();

    RK_S32 start();

    RK_S32 stop();

    RK_S32 Destroy();

    RK_S32 Init();

    RK_S32 RegisterRxCallback(VENC_CHN chn, vencStreamCallback func);

    void UnRegisterRxCallback(VENC_CHN chn);

    RK_CODEC_ID_E getChnCodecId(VENC_CHN chn);

    bool vencChnIsRun(VENC_CHN chn);

    std::map<int, VENC_BIND_RTSP_S>& getVencBindAttr();

};









#endif //!_RKVENCMOD_H_
