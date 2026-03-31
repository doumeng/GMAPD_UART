#ifndef _UDP_SER_H__
#define _UDP_SER_H__

#include <string>
#include <mutex>
#include <list>
#include <sys/socket.h>
#include <arpa/inet.h>

typedef int (*callBackFuns)(const void*ptr, size_t size);

class udpSer
{
private:
    int __sockFd = -1;
    int __port = -1;
    bool __ipCheck = false;
    std::string __ip;
    std::mutex *__sendLock;
    struct sockaddr_in __remoteAddr;
    struct sockaddr_in __recvAddr;
    socklen_t __remoteLen;
    bool bindFlag;
    bool __recvStart = false;
    std::list<callBackFuns> __funs;

private:
    int __recvThd();
    void __runCallBack(const void* buff, size_t size);
    void __start();

public:
    //创建udp对象，配置的ip和端口为自己的
    udpSer(std::string ip, int port);

    //创建udp对象，配置的自己的端口，ip自动识别
    udpSer(int port);

    //不bind
    udpSer();
    
    //析构
    ~udpSer();

public:
    //初始化-bind
    int Init();

    //去初始化
    void deInit();

    //初始化成功，启动udp
    void start();

    //设置端口号
    void setMySelfPort(int port);

    ////初始化组播属性
    void initMulticast(std::string ip);

    void initMulticastCli(std::string ip);

    void setRemote(std::string &ip, int port);

    void setRemote(const char *ip, int port);

    int send(const void *buff, const size_t size);
    
    int send(std::string &ip, int port, const void *buff, const size_t size);

    //注册回调
    void regCallBack(callBackFuns fun);
    
    //去除注册的回调
    void unregCallBack(callBackFuns fun);

//属性配置接口,缓存大小,阻塞与否
    void setCacheSize(size_t size);
};



#endif //_UDP_SER_H__
