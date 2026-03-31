#ifndef _RKVDECCONFIG_H_
#define _RKVDECCONFIG_H_

#include <map>
#include <list>
#include "rk_debug.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_cal.h"
#include "rk_mpi_vdec.h"
#include "rk_comm_vdec.h"

#define MAX_URL_NAME_LEN (64)

typedef struct GLOBAL_rkVdec {

    int chn;
    VDEC_CHN_ATTR_S chnAttr;
    VDEC_CHN_PARAM_S modParm;
    VIDEO_DISPLAY_MODE_E disPlayMode;

}GLOBAL_rkVdec_CONFIG_S;

typedef struct G_MB_ATTRS{

    RK_BOOL vdecChnEnb[VDEC_MAX_CHN_NUM];
    bool bindFfmpegEnb[VDEC_MAX_CHN_NUM];
    char url[VDEC_MAX_CHN_NUM][MAX_URL_NAME_LEN];
    GLOBAL_rkVdec_CONFIG_S Attr[VDEC_MAX_CHN_NUM];
    VDEC_MOD_PARAM_S modParam;

}GLOBAL_VDEC_ATTR_S;

class rkVdecConfig
{
private:
public:
    rkVdecConfig(/* args */);
    ~rkVdecConfig();
    RK_S32 Init();
};

RK_BOOL *getVdecChnEnb();
bool *getVdecBindEnb();
VDEC_MOD_PARAM_S *getVdecMbMod();
GLOBAL_rkVdec_CONFIG_S *getVdecChnAttr(int chnId);
char *getVdecChnUrl(int chnId);

#endif //!_RKVDECCONFIG_H_
