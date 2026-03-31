#ifndef _RKVPSSCONFIG_H_
#define _RKVPSSCONFIG_H_

#include "rk_debug.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_cal.h"
#include "rk_mpi_vpss.h"
#include "rk_comm_vpss.h"

typedef struct vpssGrpRationAttr
{
    ROTATION_E ration;
    RK_BOOL enb;
}VpssGrpRationAttr_S;

typedef struct vpssGrpMirrorFilpAttr
{
    RK_BOOL mirror;
    RK_BOOL filp;
}VpssGrpMirrorFilpAttr_S;

typedef struct g_rkVpssGrpAttr{
    SIZE_S                        inputSize;
    PIXEL_FORMAT_E                intputEnPixelFormat;   /* RW; Pixel format of target image. */
    FRAME_RATE_CTRL_S             stFrameRate;        /* Frame rate control info */
    COMPRESS_MODE_E               enCompressMode;      /* RW; Reference frame compress mode */
    VPSS_CROP_INFO_S              cropInfo;
    VpssGrpRationAttr_S           rationInfo;
    VIDEO_PROC_DEV_TYPE_E         procDev;
    VpssGrpMirrorFilpAttr_S       mirrorFilpInfo;
}G_VPSS_GRP_ATTR;

typedef struct g_rkVpssChnAttr{
    SIZE_S              size;
    VPSS_CHN_MODE_E     enChnMode;          /* RW; Vpss channel's work mode. */
    VIDEO_FORMAT_E      enVideoFormat;      /* RW; Video format of target image. */
    PIXEL_FORMAT_E      enPixelFormat;      /* RW; Pixel format of target image. */
    COMPRESS_MODE_E     enCompressMode;     /* RW; Compression mode of the output. */
    FRAME_RATE_CTRL_S   stFrameRate;        /* Frame rate control info */
    RK_BOOL             bMirror;            /* RW; Mirror enable. */
    RK_BOOL             bFlip;              /* RW; Flip enable. */
    RK_U32              u32Depth;           /* RW; Range: [0, 8]; User get list depth. */
    ASPECT_RATIO_S      stAspectRatio;      /* Aspect Ratio info. */
    RK_U32              u32FrameBufCnt;     /* RW; frame buffer cnt only used by MB_SOURCE_PRIVATE */
    VPSS_CROP_INFO_S    cropInfo;
    VpssGrpRationAttr_S rationInfo;
    VPSS_ROTATION_EX_ATTR_S rationEx;

}G_VPSS_GRP_CHN_ATTR;

typedef struct vpssAttrS{
    VPSS_MOD_PARAM_S modParm;
	RK_BOOL grpEnableHash[VPSS_MAX_GRP_NUM];
	RK_BOOL chnEnableHash[VPSS_MAX_GRP_NUM][VPSS_MAX_CHN_NUM];
	G_VPSS_GRP_ATTR stVpssGrpAttr[VPSS_MAX_GRP_NUM];
	G_VPSS_GRP_CHN_ATTR stVpssChnAttr[VPSS_MAX_GRP_NUM][VPSS_MAX_CHN_NUM];  
}GLOBAL_VPSS_ATTR_S;

class rkVpssConfig
{
private:
public:
    rkVpssConfig(/* args */);
    ~rkVpssConfig();
    RK_S32 Init();
};


G_VPSS_GRP_ATTR *getVpssGrpAttr();

G_VPSS_GRP_CHN_ATTR *getVpssGrpChnAttr(int grp);

RK_BOOL *getGrpEnbList();

RK_BOOL *getGrpChnEnbList(int grp);

VPSS_MOD_PARAM_S *getModParm();


#endif //!_RKVPSSCONFIG_H_
