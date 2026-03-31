//
// Created by admin on 2019/8/18.
//

#ifndef SIMRTSPSERVER_H
#define SIMRTSPSERVER_H

#include <pthread.h>
#include <string> 
#include "commInfo.h"
#include "rtspServerConfig.h"

#define MAX_NAME_SIZE				100
#define DEFAULT_THREAD_PRIORITY		5
#define DEFAULT_TRHEAD_STACKSIZE	(64*1024)

typedef int(*THREAD_FUNC)(void *pParam);

typedef struct tagTGLThread
{
	pthread_t		hThread;
	THREAD_FUNC		func;
	void			*pParam;
	char			szName[MAX_NAME_SIZE];
}TGL_thread_t;

class rtspServerApi {
public:
    rtspServerApi();
    ~rtspServerApi();

private:
    static int ThreadRtspServer(void* pParam);
    static int clientCnt;
    int serverNum;
private:

    pthread_mutex_t m_mutexThis;
    TGL_thread_t *m_pThreadServer;
public:
	void *v_pEvtLoop;
    void *v_pServer;
    void *v_pSession;
    uint32_t v_sessionID;
	
    int m_nClients;
    std::string m_strIP;
    int m_nPort;
    RK_CODEC_ID_E m_enPayload;
    int StartServer(unsigned    short port, RK_CODEC_ID_E enPayload);
    int StartServer(unsigned short port, std::string loaclIp, RK_CODEC_ID_E enPayload);
    int StartServer(int serNum, unsigned short port, std::string loaclIp, RK_CODEC_ID_E enPayload);
    int StopServer();
    int PushData(COMMON_ENCODE_FRAME_TYPE_E type, void* pData, unsigned int len);
    std::string m_strURL;
};


#endif //SIMRTSPSERVER_H
