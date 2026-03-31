#ifndef _IMG_USER_H_
#define _IMG_USER_H_

#include <map>
#include "rkVencMod.h"
#include "commInfo.h"

#pragma pack(push,1)
typedef struct
{
    uint8_t camare0Status;
    uint8_t camare1Status;
    uint8_t rk0PcieStatus;
    uint8_t rk1PcieStatus;
    uint8_t aiPcieStatus;

}IMG_DEV_INTERFACE_STATUS_S;
#pragma pack(pop)

namespace img{

    //整个工程初始化，各个模块初始化。
    int imgInit();

    //工程去初始化
    int imgStopInput();

    //用户注册编码器回调函数
    int userRegVencCallBack(VENC_CHN chn, vencStreamCallback func);

    class RkPrivData;
    class ImgMod
    {
    private:
        RkPrivData *priv = nullptr;
        bool imgFlag;
    public:
        ImgMod();
        ~ImgMod();
        int create(imgChnInfo_S &info);
        bool isEmptyFrame();
        void flush();
        size_t w();
        size_t h();
        void* ptr();
        size_t size();
        size_t frameNum();
        PIXEL_FORMAT_E fmt();
        RK_MODE_TYPE_E mod();
    public:
        const void* Element();
        const void* blk();
        const void* pri();
    };

    ImgMod imgRead(imgChnInfo_S &info);

    int imgWrite(imgChnInfo_S &info, ImgMod &img);

    int imgWrite(imgChnInfo_S &info, VIDEO_FRAME_INFO_S *frame);

    int imgWrite(imgChnInfo_S &info, const void*ptr, const size_t size);

    size_t tdeAllocMmz(PIXEL_FORMAT_E fmt, uint32_t w, uint32_t h, MB_BLK *blk);

    void checkXdmaVideoAttr(uint32_t dev, uint32_t chn, uint32_t w, uint32_t h, PIXEL_FORMAT_E fmt);

}

//读FPGA版本号
int readVersion(int dev);

///读取接口状态
void getImgInterfaceStatus(IMG_DEV_INTERFACE_STATUS_S&intStatus);

#endif // !