#ifndef _PW_PROCESS_H_
#define _PW_PROCESS_H_

#include <stdint.h>

int boardPwCtrlInit();

int apdLevelCtrl(uint8_t cmd);

int apdEnCtrl(uint8_t enb);

int apdEnSecCtrl(uint8_t cmd); // 000 1.8V 000 5V

int vcc1v8FirstCtrl(uint8_t enb);

int vcc5VFirstCtrl(uint8_t enb);

int apdVccCtrl(const void *txbuff, size_t size);

int ms5541VccCtrl(const void *txbuff, size_t size);

/* adc采集芯片初始化 */
int tempSensorInit();

int getSensor1ADCData(int16_t *adcData);

int getSensor0ADCData(int16_t *adcData);


///一秒采集一次adc 测试使用
void createMs5292tReadThd();

void getSensor1Temp(int8_t *temp);

void getSensor0Temp(int8_t *temp);

#endif