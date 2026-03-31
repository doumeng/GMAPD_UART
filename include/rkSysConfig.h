#ifndef _RKSYSCONFIG_H_
#define _RKSYSCONFIG_H_

#include <map>
#include <list>
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_cal.h"

#define MAX_POOL_NUM (20)

typedef struct GLOBAL_POOL_ATTR{
	SIZE_S stBlkSize;
	RK_U32 u32BlkCnt;
	PIXEL_FORMAT_E enPixelFormat;
    MB_UID_E mods;
}GLOBAL_POOL_ATTR_S;

typedef struct MB_ATTR{

    RK_BOOL poolEnb[MB_MAX_COMM_POOLS];
    GLOBAL_POOL_ATTR_S mbAttr[MB_MAX_COMM_POOLS];
}MB_ATTR_S;

class rkSysConfig
{
private:
public:
    rkSysConfig(/* args */);
    ~rkSysConfig();
    RK_S32 Init();
};

RK_BOOL *getMbEnbList();
GLOBAL_POOL_ATTR_S* getMbPollAttr(int poolId);
bool getSysInitFlag();

#endif //!_RKSYSCONFIG_H_
