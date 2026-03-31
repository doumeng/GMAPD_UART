#ifndef __XDMA_VIDEO_H_
#define __XDMA_VIDEO_H_

#include <map>
#include <semaphore.h>
#include "xdma.h"
#include "xdmaVideoCfg.h"
#include "doubleQue.h"

namespace xdmaVideoSpace{

#define DEPTH (4)

//图像输入全局初始化
int xdmaVideoInit();
bool xdmaInitFlag();
xdmaVideoDevAttr_S *__getVideoChnAttr(int devid, int chn);
int Start(int devid);
bool xdmaDevIsEnb(int devId);
uint32_t  xdmaWriteReg(int dev, uint32_t offset, uint32_t data);
uint32_t  xdmaReadReg(int dev, uint32_t offset);
uint32_t  xdmaDmaWrite(int dev, int chn, const void* buff, uint64_t size, uint64_t base);
uint32_t  xdmaDmaRead(int dev, int chn, void* buff, uint64_t size, uint64_t base);
int getXdmaUseDevNum();

/// 图像video输入，一个变量对应一路图像
class xdmaVideoDev
{
private:
    xdmaVideoDevAttr_S __videoDevAttr;
    bool userWantExchangeThisFrame = false;
    bool __recvFlag = false;
    size_t __frameNum = 0;

    ThreadSafeDoubleQueue<IMG_SRC_INFO_S> *videoDQue = nullptr;

private:
    int imgBindThd();
    int __RunBindFunc(IMG_SRC_INFO_S *src);
    size_t callImgSize(PIXEL_FORMAT_E format, uint32_t w, uint32_t h);

private:
    std::thread __bindThd;
    std::thread __recvThd;
    sem_t __recvSem;

public:
    xdmaVideoDev(/* args */);
    ~xdmaVideoDev();

    int create(int devid, int chn);

    int Init();

    int Start();

    int startRecvThd();

    int startBindThd();

    int imgRecvThd();

    void stopRecvThd();

public:
    uint32_t   writeReg(uint32_t offset, uint32_t data);

    uint32_t   readReg(uint32_t offset);

    int  regCallback(xdmaEventFunc func, void *arg);

    int  unRegCallback(xdmaEventFunc func, void *arg);

    int  getFrame(char *buffer, uint64_t size, uint64_t base);

    int  sendFrame(char *buffer, uint64_t size, uint64_t base);

    void postSem();

    int clearIrq();

    void popImg(IMG_SRC_INFO_S &src);

    bool getImg(IMG_SRC_INFO_S &src);

public:

    size_t w();

    size_t h();

    PIXEL_FORMAT_E fmt();

    int eventId();

    int chnId();

    bool isInit();

    int devId();

    bool bindF();

};


} // namespace name

#endif // __XDMA_VIDEO_H_
