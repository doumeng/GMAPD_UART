#ifndef _RKVDEC_H_
#define _RKVDEC_H_

#include "rkVdecConfig.h"
#include "commInfo.h"

namespace vdecFrame{
    int inputFrame(int chn, void *data, size_t len, RK_S32 milliSec);
    int inputFrame(int chn, IMG_SRC_INFO_S *src, RK_S32 milliSec);
    int outputFrame(VDEC_CHN VdChn, VIDEO_FRAME_INFO_S *pstFrameInfo, RK_S32 s32MilliSec);
    int outputFrame(VDEC_CHN VdChn, IMG_SRC_INFO_S *data, RK_S32 s32MilliSec);
    int releaseFrame(VDEC_CHN VdChn, const VIDEO_FRAME_INFO_S *pstFrameInfo);
}

class vdec {
public:
    vdec();
    ~vdec();
    RK_S32 Init();
    RK_S32 Create();
    RK_S32 Destroy();
    RK_S32 vdecStart();
    RK_S32 vdecSetFFmpegCallback(RK_S32 chnId, pktCallbackType callback);
    RK_S32 vdecDelFFmpegCallback(RK_S32 chnId, pktCallbackType callback);


private:
    bool __vdecIsCfg;
    rkVdecConfig _config;
    MB_POOL __vdecChnMbPool[VDEC_MAX_CHN_NUM];
    // ffmpegStream *ffmpegClient[VDEC_MAX_CHN_NUM];

};


#endif //!_RKVDEC_H_
