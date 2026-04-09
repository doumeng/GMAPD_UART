#ifndef _RK_TDE_H___
#define _RK_TDE_H___

#include "rk_mpi_tde.h"

namespace tdeMod{


//tde初始化，只初始化一次
int tdeInit();

//tde去初始化，初始化成功才可去初始化
void tdeDeInit();


//tde快速拷贝, 可选择拷贝的图像大小以及拷贝位置
int tdeQuickCopy(VIDEO_FRAME_INFO_S *imgSrc, VIDEO_FRAME_INFO_S *imgDst, TDE_RECT_S *srcPoint, TDE_RECT_S *dstPoint);



};



#endif //!_RK_TDE_H___
