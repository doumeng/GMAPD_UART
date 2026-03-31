#ifndef _TCP_SER_H__
#define _TCP_SER_H__

#include <string>
#include <mutex>
#include <list>
#include <sys/socket.h>
#include <arpa/inet.h>

/// 客户端设备描述符， 数据地址，数据长度。
typedef int (*tcpCallBackFuns)(int fd, const void*, size_t size);


class tcpSer
{
private:
    int __socketFd = -1;
    int __port = -1;
    std::string __ip;
    bool __acceptStart = false;
    std::list<tcpCallBackFuns> __funs;

private:

    int __startRecv(int client);

    int __startAccept();

    void __runCallBack(int fd, const void *buff, size_t size);

public:
    tcpSer(std::string ip, int port);
    ~tcpSer();

    int Init();
    void start();
    int sendTo(int client, const void *buff, size_t size);

    void regCallBack(tcpCallBackFuns fun);
    void unregCallBack(tcpCallBackFuns fun);
};







#endif //_TCP_SER_H__
