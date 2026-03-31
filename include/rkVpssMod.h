#ifndef _RK_VPSS_MOD_H_
#define _RK_VPSS_MOD_H_

#include "rkVpssConfig.h"
#include "commInfo.h"

//最终封装到一个类
namespace vpssFrameCtrl{

typedef struct imgInfo{

    RK_U32              u32Width; //输入图像宽
    RK_U32              u32height; //输入图像高
    RK_U32              size;      //输入图像大小
    PIXEL_FORMAT_E      enPixelFormat; //输入图像格式
    void*               data;   //输入图像数据地址

}VPSS_INPUT_IMG_INFO_S;


int inputFrame(VPSS_GRP VpssGrp, const VIDEO_FRAME_INFO_S *pstVideoFrame, RK_S32 s32MilliSec);

int inputFrame(VPSS_GRP VpssGrp, IMG_SRC_INFO_S *inputImg, RK_S32 s32MilliSec);

//直接输出RK图像帧结构体，此结构体拿到之后需要做几步操作才可以使用图像指针，所以先不适用，后续修改CV::Mat
int outputFrame(VPSS_GRP VpssGrp, VPSS_CHN vpssChn, VIDEO_FRAME_INFO_S *pstVideoFrame, RK_S32 s32MilliSec);

//带拷贝的输出
int outputFrame(VPSS_GRP VpssGrp, VPSS_CHN vpssChn, IMG_SRC_INFO_S *data, RK_S32 s32MilliSec);

int releaseFrame(VPSS_GRP VpssGrp, VPSS_CHN vpssChn, const VIDEO_FRAME_INFO_S *pstVideoFrame);

}

class vpss {
public:
    vpss();
    ~vpss();
    RK_S32 Init();
    RK_S32 Create();
    RK_S32 Destroy();
    RK_S32 Start();

private:

    MB_POOL __vpssGrpMbPool[VPSS_MAX_GRP_NUM];
    bool vpssCfgIsInit = false;

    //基本属性，创建时需配置，下面的属性都是创建后配置
    VPSS_GRP_ATTR_S vpssGrpAttrs[VPSS_MAX_GRP_NUM];
    VPSS_CHN_ATTR_S vpssGrpChnAttrs[VPSS_MAX_GRP_NUM][VPSS_MAX_CHN_NUM];

    //grp拓展属性
    VPSS_CROP_INFO_S              grpCropInfo[VPSS_MAX_GRP_NUM];
    VpssGrpRationAttr_S           grpRationInfo[VPSS_MAX_GRP_NUM];
    VIDEO_PROC_DEV_TYPE_E         procDev[VPSS_MAX_GRP_NUM];
    VpssGrpMirrorFilpAttr_S       mirrorFilpInfo[VPSS_MAX_GRP_NUM];

    //chn拓展属性
    VPSS_CROP_INFO_S    chnCropInfo[VPSS_MAX_GRP_NUM][VPSS_MAX_CHN_NUM];
    VpssGrpRationAttr_S chnRationInfo[VPSS_MAX_GRP_NUM][VPSS_MAX_CHN_NUM];
    VPSS_ROTATION_EX_ATTR_S rationEx[VPSS_MAX_GRP_NUM][VPSS_MAX_CHN_NUM];

    rkVpssConfig _vpssConfig;
};

#endif //!_RK_VPSS_MOD_H_
