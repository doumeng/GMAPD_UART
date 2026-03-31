#include <stdio.h>
#include <thread>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <mntent.h>
#include <sys/time.h>
#include "uartClass.h"
#include "gpioClass.h"
#include "appExample.h"
#include "udpSer.h"
#include "xdmaVideo.h"

#define USE_LOOP_TEST_UART_NUM 6
#define DEFAULT_BAUDRATE 115200

static std::thread uartTid[USE_LOOP_TEST_UART_NUM];

static int uartNum[USE_LOOP_TEST_UART_NUM] = {0,3,4,5,6,7};

void printBuff(int ret, uint8_t *tempBuff)
{
    int i;
    for (i = 0; i < ret; i++)
        printf("%02x  ", tempBuff[i]);

    printf("\n");
}

void helloWorld()
{
    printf("hello world\n");
}

void userGetImgDemo(int inputChn, int outPutVencChn)
{
    bool getImgFlag(true);
    bool wrireflag(true);

    imgChnInfo_S ccdImgChnAttr;
    ccdImgChnAttr.modType = MOD_ID_XDMA_VIDEO;
    ccdImgChnAttr.xdmaVideo.dev = 0;
    ccdImgChnAttr.xdmaVideo.chn = inputChn;
    ccdImgChnAttr.xdmaVideo.timeOut = 0;

    imgChnInfo_S outModAttr;
    outModAttr.modType = MOD_ID_FPGA;
    outModAttr.img2Fpga.chn = outPutVencChn;
    outModAttr.img2Fpga.userSpace = true;
    outModAttr.img2Fpga.timeOut = -1;

    uint64_t count = 0;

    while (getImgFlag){
        img::ImgMod ccdImg = img::imgRead(ccdImgChnAttr);
        if(ccdImg.isEmptyFrame()){
            usleep(1000);
            continue;
        }

        ccdImg.ptr();

        printf("chn %d fpga %d, count %llu\n", inputChn, outPutVencChn, count);

        if(count == 50){
            wrireflag = false;
        }

        if(wrireflag){
            int fd = open("test.yuv", O_CREAT | O_APPEND | O_RDWR);
            write(fd, ccdImg.ptr(), ccdImg.size());
            close(fd);
        }


        img::imgWrite(outModAttr, ccdImg);

        // img::imgWrite(outModAttr, ccdImg.ptr(), ccdImg.size());
        
        count++;
    }
}

void createGetImgThd()
{
    std::thread tidCCD = std::thread(userGetImgDemo, 0, 0);
    tidCCD.detach();
}

int openSerial(int uartN)
{
    unsigned char tempBuff[64];
    int ret = 0;
    char path[16];
    sprintf(path, "/dev/ttyS%d", uartN);

    uartDev *serialc = nullptr;

    serialc = new uartDev(path, DEFAULT_BAUDRATE);
    serialc->uartDevCreate();

    printf("test %s, baudRate %d\n", path, DEFAULT_BAUDRATE);

    while (1){
        ret = serialc->uartRecv(tempBuff, sizeof(tempBuff), 10);
        if(ret < 0){
            printf("serial %s read failed\n", path);
            break;
        }else if(0 == ret){
            continue;
        }
        printf("uart %s recv:  ", serialc->path());
        printBuff(ret, tempBuff);
        serialc->uartSend(tempBuff, ret); //回环
    }

    return 0;
}

void createUartLoopTest()
{
    for (size_t i = 0; i < USE_LOOP_TEST_UART_NUM; i++)
    {
        uartTid[i] = std::thread(openSerial, uartNum[i]);
        uartTid[i].detach();
    }
}

/*  CLI 注册地， 示例代码，测试代码，提供给客户的示例代码 */
int printHello(int argc, char *argv[])
{
    helloWorld();
    return 0;
}

void printHelp()
{
    printf("\tinput uartOpen [uartNum] [boudRate]\n");
    printf("boudRate def is 115200\n");
}

int testWriteBarm(int argc, char *argv[])
{
    if (argc != 3)
    {
        return 0;
    }
    uint32_t offset;
    uint32_t data;

    sscanf(argv[1], "%x", &offset);
    sscanf(argv[2], "%x", &data);

    xdmaVideoSpace::xdmaWriteReg(0, offset, data);

    printf("write barm %#x data %#x\n", offset, data);

    return 0;
}

int testReadBarm(int argc, char *argv[])
{

    if (argc != 2)
    {
        return 0;
    }
    uint32_t offset;

    sscanf(argv[1], "%x", &offset);

    printf("read barm %#x data %#x\n", offset, xdmaVideoSpace::xdmaReadReg(0, offset));

    return 0;
}

extern struct cli_table_entry cli_cmd_table[];
extern int cli_cmd_table_size;
/**
 * 全局函数注册表
 * 第一个成员为想要定义的调用命令
 * 第二个成员为帮助信息
 * 第三个成员为实际调用函数,输入命令会执行该函数
 *
 */
struct cli_table_entry cli_cmd_table[] = {
        CLI_BUILT_IN_CMD, /* CLI 内置定义的命令，请勿删除 */
        {"hello", "hello", printHello},
        {"wBram", "wBram [addr] [data]", testWriteBarm},
        {"rBram", "rBram [addr]", testReadBarm}
};

/* 实际定义的总命令数 */
int cli_cmd_table_size = sizeof(cli_cmd_table) / sizeof(cli_cmd_table[0]);