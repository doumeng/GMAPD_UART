#ifndef __XDMA_CFG_H_
#define __XDMA_CFG_H_
#include "cJSON.h"
#include <map>

#define MAX_DEV_NAME_LEN (16)

#define MAX_XDMA_DEV_NUM (4)
#define MAX_XDMA_CHN_NUM (4)
#define MAX_XDMA_EVENT_DEV (16)

typedef struct xdmaAttr{
    unsigned int userBarSize;
    unsigned int ctrlBarSize;
    unsigned int bypassBarSize;
    unsigned int useEventNum;
    unsigned int useDevNum;

    bool enb;
}XDMA_ATTR_S;

typedef struct xdmaDevAttr{

    std::map<int , XDMA_ATTR_S> barSizeMap;
    std::map<int , int> bindCpuId;
    int devNum;
}xdmaDevAttr_S;


class xdmaCfg
{
private:
    int __getcfg(cJSON* item, int dev);
    int __getUserBarCfg(cJSON* item, int dev);
    int __getBypassBarCfg(cJSON* item, int dev);
    int __getCtrlBarCfg(cJSON* item, int dev);
    int __getCpuBindCfg(cJSON* item, int dev);

    int _GetConfig(cJSON* item);

    xdmaDevAttr_S __xdmaCfgMap;

public:
    xdmaCfg(/* args */);
    ~xdmaCfg();
    int Init();
    std::map<int , XDMA_ATTR_S> &getBarAttr();
    std::map<int , int> &getCpuAttr();
    int getDevNum();
};




#endif //__XDMA_CFG_H_