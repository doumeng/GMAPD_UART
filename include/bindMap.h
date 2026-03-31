#ifndef _BIND_MAP__H__
#define _BIND_MAP__H__

#include "bindMapCfg.h"

namespace useSys {
    int bind(MPP_CHN_S *src, MPP_CHN_S *dst);

    int unBind(MPP_CHN_S *src, MPP_CHN_S *dst);
}

class bindMapC {
private:
    bindMapCfg bindMaps;
public:
    bindMapC();
    ~bindMapC();
    int Init();
    int Start();
};



#endif // !_BIND_MAP__H__