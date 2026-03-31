#ifndef _PERIPHERY_API__H___
#define _PERIPHERY_API__H___

#include <stddef.h>
#include <mutex>

#define MAX_DEV_NAME (128)

namespace periDev{

//RK芯片自身接口、目前只使用了 2 3 4 ADC通道，所以只能传参 2 3 4其中
class adcDev
{
private:
    int adcChn;
    int __devFd;
    char devName[MAX_DEV_NAME];
    std::string *_path;
    std::mutex *rdMutex;
    friend class Ltc2991Dev;
public:
    virtual adcDev(std::string &path);
    virtual adcDev();
    virtual ~adcDev();
public:
    virtual int openDev();
    virtual int getAdc();
    virtual int closeDev();
};

///目前Chn只用了 1 2 3 为电压, 7 8为ADC 
class Ltc2991Dev : public adcDev{
public:
    Ltc2991Dev(std::string &path);

};

}



#endif // !_PERIPHERY_API__H___