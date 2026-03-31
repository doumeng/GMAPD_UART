#ifndef _FTOK_PROCESS_H_
#define _FTOK_PROCESS_H_

#include <stdint.h>

typedef enum{
    RK0_NUM,
    RK1_NUM
}RK_NUM_E;

typedef enum{
    VCC28V_OUT_HY = 1,
    VCC28V_OUT_CJ,
    VCC12V_OUT,
    BORAD_REBOOT
}CTRL_POWER_CMD_E;


#pragma pack(push,1)
typedef struct
{
    int8_t rkNum;
    int8_t fpgaTemp0;
    int8_t rk0Temp;
    int8_t rk1Temp;
    int8_t sf2507Temp;
}userGetSelfData_S;
#pragma pack(pop)


int processToProcess();

void getBoardTemp(userGetSelfData_S& selfdata);

int ctrl28vOutHy(bool enb);

int ctrl28vOutCj(bool enb);

int ctrl12vOut(bool enb);

int ctrlBoardReboot(bool enb);

#endif