#ifndef __XDMA_LIB_H_
#define __XDMA_LIB_H_
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <thread>
#include <mutex>
#include "xdmaCfg.h"
/* return state*/
#define XDMA_RET_ERR    (-1)
#define XDMA_RET_OK     (0)

/**/
#define MAX_DMA_CHL (16)
#define MAX_CTL_CHL     (1)
#define MAX_EVENT_CHL   (6)


#define MAX_EPOLL_FD    16

#define RW_MAX_SIZE 0x7ffff000

#define dsb(scope) asm volatile("dsb " #scope : : : "memory")

typedef int (*xdmaEventFunc)(void*arg);

typedef struct eventCallback {
    xdmaEventFunc func;
    void *args;
}eventCallback_t;

class xdma
{
private:
    xdmaCfg __config;
    std::map<int, int> __userBarFdmap;
    std::map<int, int> __ctrlBarFdmap;
    std::map<int, int> __bypassBarFdmap;

    std::map<int , int> __c2hFdmap[MAX_XDMA_DEV_NUM];
    std::map<int , int> __h2cFdmap[MAX_XDMA_DEV_NUM];
    std::map<int , int> __eventFdmap[MAX_XDMA_DEV_NUM];
    bool devEnb[MAX_XDMA_DEV_NUM] = {false, false,false,false};

    std::map<int , std::shared_ptr<eventCallback_t>> _eventCallBackMap[MAX_XDMA_DEV_NUM];
    std::thread _epollPid[MAX_XDMA_DEV_NUM];

    void *__userBarBaseAddr[MAX_XDMA_DEV_NUM];
    int __epollFd[MAX_XDMA_DEV_NUM];
    int __eventCpuId[MAX_XDMA_DEV_NUM] = {0,0,0,0};
    bool _eventIsRun[MAX_XDMA_DEV_NUM] = {false, false, false,false};
    std::mutex eventCreatLock[MAX_XDMA_DEV_NUM];
    std::mutex c2hLock[MAX_XDMA_DEV_NUM];
    std::mutex h2cLock[MAX_XDMA_DEV_NUM];

    int devNum = 0;

    int eventReadTask(int devid);

    ssize_t write_from_buffer(int fd, char *buffer, uint64_t size,uint64_t base);
    ssize_t read_to_buffer(int fd, char *buffer, uint64_t size, uint64_t base);

public:
    xdma(/* args */);
    ~xdma();
    int Init();

    int setCpuId(int dev, int cpuid);

    //获取userbar地址
    void*getBarBase(int devid);

    ///写寄存器
    uint32_t writeUserBar(int devid, uint32_t offset, uint32_t data);
    
    ///读寄存器
    uint32_t readUserBar(int devid, uint32_t offset);

    ///写DMA数据
    ssize_t outPutData(int devid, int chn, char *buffer, uint64_t size,uint64_t base);
    
    ///读DMA数据
    ssize_t inputData(int devid, int chn, char *buffer, uint64_t size, uint64_t base);

    //注册对应中断号的回调函数
    int regEventFuncCallBack(int devid, int eventNum, xdmaEventFunc func, void* arg);
    
    //取消注册对应中断号的回调函数
    int unRegEventFuncCallBack(int devid, int eventNum);
    
    int runEventFunc(int devid);
    
    int stopEventFunc();

    bool devIsEnb(int devId);

    int getUseDevNum();
};

#endif // __XDMA_LIB_H_