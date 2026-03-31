#ifndef SIMRTSPSERVER_CONFIG_H
#define SIMRTSPSERVER_CONFIG_H

#include "cJSON.h"
#include "rk_common.h"
#include "rk_comm_venc.h"
#include <string>
#include <map>

typedef struct rtspServerConfig{

    std::string url;
    int port;
    RK_CODEC_ID_E codeId;
    bool enb;
}rtspServerConfig_S;

//随便写一个配置，简单配置一下，后续rtsp将多会话实现之后在进行修改此处。

rtspServerConfig_S *getRtspServerAttr(int rtspSerNum);

RK_S32 rtspCfgInit();

#endif //SIMRTSPSERVER_CONFIG_H