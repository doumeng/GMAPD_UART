#ifndef _RKSYS_H_
#define _RKSYS_H_

#include "rkSysConfig.h"

class Mb {
public:
    Mb();
    ~Mb();
    RK_S32 Init();
    RK_S32 Create();
    RK_S32 Destroy();
private:
    MB_CONFIG_S *_stMbConf;
    rkSysConfig _config;
};

class Sys {
public:
    Sys();
    ~Sys();
    RK_S32 Init();
    RK_S32 Create();
    RK_S32 Destroy();
private:
    Mb sMb;

};

#endif //!_RKSYS_H_
