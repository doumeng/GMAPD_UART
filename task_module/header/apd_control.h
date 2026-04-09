/*
 * @Author: doumeng 1159898567@qq.com
 * @Date: 2026-03-19 11:22:08
 * @LastEditors: doumeng 1159898567@qq.com
 * @LastEditTime: 2026-03-23 17:37:29
 * @FilePath: /GMAPD_UART/task_module/header/apd_control.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#pragma once


#include <unistd.h>

#include "xdmaVideo.h"
#include "apd_reg.h"
#include "pwProcess.h"
#include "img.h"

extern imgChnInfo_S depthImgChnAttr;
extern imgChnInfo_S tofImgChnAttr;
extern imgChnInfo_S outModAttr;

//  电源控制
int InitVoltageCtrl();                          // 电压控制初始化函数
int SecondVoltageCtrl();                        // 二级电压控制函数
int HighVoltageCtrl(uint8_t voltage, uint8_t level);              // 高压控制函数，voltage为电压值，单位为0.1V，例如50表示5.0V
int ShutdownVoltageCtrl();                      // 关机控制函数

// PCIE初始化
void Pcie_Init(int inputChn, int outPutVencChn);
void Mipi_Init(int inputChn);

// 时序控制
int ApdControl(int control, int sync_delay, int sync_pulse_width,
                          int reset_delay, int reset_pulse_width,
                          int en_delay, int en_pulse_width,
                          int rec_delay, int rec_pulse_width, int test_pulse_width);

int ApdGatherEn(int gather_en);                 // APD时序使能控制函数，gather_en为1表示使能，0表示禁用   
int PcieChlCtrl(int chl);                       // PCIE通道控制函数，chl为0表示原始数据通道，1表示点云数据通道
int EnDelayCtrl(int delay);                     // 延迟控制函数，delay为延迟开门的计数值
int RecDelayCtrl(int delay);                    // 延迟控制函数，delay为延迟接收的计数值   
int TriggerModeCtrl(int mode);                  // 触发模式控制函数，0为内触发，1为外触发
int CycleCtrl(int cycle);                       // 内触发周期控制函数，cycle为周期值
int ApdTestEn(int test_en);                     // 测试点控制函数，test_en为1表示使能，0表示禁用
int StrideLengthCtrl(int length);               // 步长控制函数，length为步长的对数值
int DiffThreholdCtrl(int threhold);             // 差分阈值控制函数，threhold为阈值
int ApdConstructFrameCtrl(int buffer_num);      // APD构建帧使能控制函数，buffer_num代表有多少个50帧数据被使用

// 读取状态函数
int GetFpgaVersion();
int ApdGatherEnStatus();
int GetEnDelay();
int GetRecDelay();
int GetDiffThrehold();
int GetStrideLength();
int GetApdConstructFrameNumber();
int GetRegValue(int reg_addr);
