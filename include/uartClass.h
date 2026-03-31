#ifndef _UART_CLASS__H___
#define _UART_CLASS__H___

#include <stddef.h>
#include <mutex>

#define MAX_UART_PATH_LEN (16)

class uartDev{
private:
    void *serial;
    int uartBaudRate;
    std::mutex *serLock = nullptr;
    char pathName[MAX_UART_PATH_LEN];
public:
    uartDev(const char *uartPath, int baudRate);
    uartDev();
    ~uartDev();

    int uartSetPath(const char *path);

    int uartDevCreate();

    int uartDevDestory();

    int uartSetBaudRate(int baudRate);

    const char * path();

    int baud();

    int uartRecv(const void *buf, size_t len, int timeoutMs);

    int uartSend(const void *buf, size_t len);

};

#endif // !_UART_CLASS__H___