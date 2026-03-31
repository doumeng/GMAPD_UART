#ifndef _BIND_MAP_CFG_H__
#define _BIND_MAP_CFG_H__

#include "rk_type.h"
#include "rk_common.h"
#include "cJSON.h"
#include "rk_mpi_sys.h"
#include "commInfo.h"
#include <map>
#include <list>

typedef struct {
    MPP_CHN_S in;
    MPP_CHN_S out;
}bindMapCfg_S;

typedef struct bindMap
{
    //图像输入到图像输出模块或者模块通道，比如vpss-vo-venc，这里暂不设计线程bind
    std::list<bindMapCfg_S> bindMapList;
    int count;
}bindMap_S;

class bindMapCfg
{
private:
    bindMap_S __bindAttr;
    bool bindEnb;
    int _GetConfig(cJSON* item);

public:
    bindMapCfg(/* args */);
    ~bindMapCfg();
    int Init();
    bindMap_S &getBindAttr();
};






#endif // !_BIND_MAP_CFG_H__