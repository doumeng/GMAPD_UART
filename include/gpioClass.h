#ifndef _GPIO_CLASS__H__
#define _GPIO_CLASS__H__

#include <stddef.h>
#include <thread>
#include <poll.h>

#define USE_RK_GPIO
//计算RK gpio NUM的公式
#ifdef USE_RK_GPIO
#define GPIO_NUM_SET(x,y,z) (((x) * (32u)) + ((y) * (8u)) + (z))
#endif

//NUM gpio主号
//GRP A=0,B=1,C=2,D=3...........
//OffSet gpio1-2—3的3

typedef struct gpioGrp{
    int gpioNum;
    int gpioGrp;
    int gpioOffset;
}gpioGrp_S;

typedef enum{
    LEVEL_lOW,
    LEVEL_HIGH
}GPIO_LEVEL_E;

typedef enum{
    DIR_IN,
    DIR_OUT
}GPIO_DIR_E;

typedef enum{
    IRQ_CLOSE, // 不开启中断
    IRQ_BOTH, // 双沿触发
    IRQ_EDGE_FALLING, //下降沿
    IRQ_EDGE_RISING, //上升沿
    IRQ_BUTT //最大限制，超过报错
}GPIO_IRQ_E;

typedef struct gpioAttr{
    int gpioNumber;
    GPIO_DIR_E dir;
    GPIO_LEVEL_E level;
}gpioAttr_S;

//中断处理回调
typedef void *(*gpioIrqCallBack)(void* );

class gpioDev
{
private:
    gpioAttr_S attr;
    int valueFd;
private:
    std::shared_ptr<std::thread> irqThd;
    struct pollfd __irqFds[1];
    bool irqLoopEnd = true;
    gpioIrqCallBack __exIrqCallBack;
private:
    void *userArgs;

    int irqThdRun(void *args);

public:
    gpioDev(int num, int grp, int offset);
    gpioDev();
    ~gpioDev();

    int setGpioNum(int num, int grp, int offset);//可重复配置，配置之后调用 setStatus 输出或者配置输入

    //若方向为输出，配置此接口后，GPIo属性便固定下来。后续想要修改gpio方向以及值，则需再次调用此接口重新配置。
    int setStatus(GPIO_DIR_E dir, GPIO_IRQ_E irqMath);

    int out(GPIO_LEVEL_E level);

    int in();

    int irqRecvStart(); ///线程创建

    int regGpioIrqCallback(gpioIrqCallBack callBack, void *args);

    int stopIrq();

    int closeIo();
};


#endif // !_GPIO_CLASS_H_