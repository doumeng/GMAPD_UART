#ifndef __XDMA_VIDEO_CFG_H_
#define __XDMA_VIDEO_CFG_H_

#include <map>
#include <list>
#include "commInfo.h"
#include "rk_comm_video.h"
#include "cJSON.h"

#define MAX_XDMA_VIDEO_NUM (4)
#define MAX_XDMA_VIDEO_CHN_NUM (4)

typedef struct bindVideoAttr{
    // MOD_ID_E bindMode;
    RK_MODE_TYPE_E bindMode;
    int chn;
}bindVideoAttr_S1;

typedef struct xdmaVideoDevAttr
{
    uint32_t width;
    uint32_t height;
    PIXEL_FORMAT_E format;
    uint32_t xdmaDevId;
    uint32_t xdmaChnId;
    uint32_t xdmaH2cId;
    uint32_t xdmaC2hId;
    uint32_t xdmaEvenId;
    uint32_t depth;
    int cpu;
    bool enb;
    bool bindFlag;
    std::list<bindVideoAttr_S1> bindModeChnList;
}xdmaVideoDevAttr_S;

typedef struct xdmaVideoCfgS
{
    xdmaVideoDevAttr_S videoCfg[MAX_XDMA_VIDEO_NUM][MAX_XDMA_VIDEO_CHN_NUM];
    // __count;
}xdmaVideoCfg_S;

class xdmaVideoCfg
{
private:
    xdmaVideoCfg_S __videoConfig;
    int __GetConfig(cJSON* item, int devid);
    int _GetConfig(cJSON* item);

public:
    xdmaVideoCfg(/* args */);
    ~xdmaVideoCfg();
    xdmaVideoDevAttr_S *getVideoChnAttr(int devid, int chn);

    RK_S32 Init();
};



#endif //__XDMA_VIDEO_CFG_H_