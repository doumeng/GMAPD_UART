#ifndef _RK__VENCCONFIG_H_
#define _RK__VENCCONFIG_H_

#include <map>
#include <list>
#include "rk_debug.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_cal.h"
#include "rk_mpi_venc.h"
#include "rk_comm_venc.h"
#include "rk_comm_rc.h"
#include "cJSON.h"

typedef struct bindRtspAttr{
    int serverNum; // rtsp服务器的编号
    int sessionNum; //RTSP以上服务器的会话号
}VENC_BIND_RTSP_S;

typedef struct rkVencConfigS
{
    std::map<int, VENC_CHN_ATTR_S> chnAttrs;
    std::map<int, VENC_RECV_PIC_PARAM_S> recvPicParams;
    std::map<int, VENC_CHN_PARAM_S> chnParams;
    std::map<int, VENC_RC_PARAM_S> rcParams;
    std::map<int, VENC_ROI_ATTR_S> roiAttrs;
    std::map<int, VENC_BIND_RTSP_S> bindRtspAttrs;

}G_RK_VENC_CONFIG_S;

class rkVencConfig
{
private:
    G_RK_VENC_CONFIG_S __vencConfig;
    int __getVencChnAttrCfg(cJSON* item, int chn);
    int __getVencChnParamCfg(cJSON* item, int chn);
    int __getVencRecvParamCfg(cJSON* item, int chn);
    int __getVencChnRcCfg(cJSON* item, int chn);
    int __getVencChnRoiCfg(cJSON* item, int chn);
    int __getVencBindCfg(cJSON* item, int chn);
    int _getConfigVencChnAllAttr(cJSON* item, int chn);
    int _GetConfig(cJSON* item);

public:
    rkVencConfig(/* args */);
    ~rkVencConfig();
    std::map<int, VENC_CHN_ATTR_S>& getVencChnAttr();
    std::map<int, VENC_RECV_PIC_PARAM_S>& getVencRecvParamAttr();
    std::map<int, VENC_CHN_PARAM_S>& getVencChnParam();
    std::map<int, VENC_RC_PARAM_S>& getVencRcParam();
    std::map<int, VENC_ROI_ATTR_S>& getVencRoiAttr();
    std::map<int, VENC_BIND_RTSP_S>& getVencBindAttr();


    RK_S32 Init();
};

#endif //!_RK__VENCCONFIG_H_
