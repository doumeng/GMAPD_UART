#include "apd_control.h"

imgChnInfo_S depthImgChnAttr;
imgChnInfo_S tofImgChnAttr;
imgChnInfo_S outModAttr;


void Pcie_Init(int inputChn, int outPutVencChn){
    // 初始化PCIE读取及写入通道
    // imgChnInfo_S depthImgChnAttr;
    depthImgChnAttr.modType = MOD_ID_XDMA_VIDEO;
    depthImgChnAttr.xdmaVideo.dev = 0;
    depthImgChnAttr.xdmaVideo.chn = inputChn;
    depthImgChnAttr.xdmaVideo.timeOut = 0;

    // imgChnInfo_S outModAttr;
    outModAttr.modType = MOD_ID_FPGA;
    outModAttr.img2Fpga.chn = outPutVencChn;
    outModAttr.img2Fpga.userSpace = true;
    outModAttr.img2Fpga.timeOut = -1;

    return ;
}

void Mipi_Init(int inputChn){
    // imgChnInfo_S tofImgChnAttr;
    tofImgChnAttr.modType = MOD_ID_VI;
    tofImgChnAttr.xdmaVideo.dev = 0;
    tofImgChnAttr.xdmaVideo.chn = inputChn;
    tofImgChnAttr.xdmaVideo.timeOut = -1;

    return ;
}

// APD变量偏移地址 10000
// 时序控制
int ApdControl(int control, int sync_delay, int sync_pulse_width,
               int reset_delay, int reset_pulse_width,
               int en_delay, int en_pulse_width,
               int rec_delay, int rec_pulse_width, int test_pulse_width)
{
    // xdmaVideoSpace::xdmaWriteReg(0, offset, data);
    if (
        xdmaVideoSpace::xdmaWriteReg(0, APD_CONTORL_ADDR, control) < 0 || 
        xdmaVideoSpace::xdmaWriteReg(0, APD_SYNC_DELAY_ADDR, sync_delay) < 0 ||
        xdmaVideoSpace::xdmaWriteReg(0, APD_SYNC_PULSE_WIDTH_ADDR, sync_pulse_width) < 0 ||
        xdmaVideoSpace::xdmaWriteReg(0, APD_RESET_DELAY_ADDR, reset_delay) < 0 ||
        xdmaVideoSpace::xdmaWriteReg(0, APD_RESET_PULSE_WIDTH_ADDR, reset_pulse_width) < 0 ||
        xdmaVideoSpace::xdmaWriteReg(0, APD_EN_DELAY_ADDR, en_delay) < 0 ||
        xdmaVideoSpace::xdmaWriteReg(0, APD_EN_PULSE_WIDTH_ADDR, en_pulse_width) < 0 ||
        xdmaVideoSpace::xdmaWriteReg(0, APD_REC_DELAY_ADDR, rec_delay) < 0 ||
        xdmaVideoSpace::xdmaWriteReg(0, APD_REC_PULSE_WIDTH_ADDR, rec_pulse_width) < 0 ||
        xdmaVideoSpace::xdmaWriteReg(0, APD_TEST_PULSE_WIDTH_ADDR, test_pulse_width) < 0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

 // 1.8v 5v 一级控制；10v 高压控制
int InitVoltageCtrl()
{
    uint8_t  apdVal = 164;   

    int status = boardPwCtrlInit();

    if (status != 0) {
        return -1; // 初始化失败
    }
              
    //set vcc out level
    if(apdLevelCtrl(0)<0){
        return -1; // 设置电压级别失败
    }

    usleep(1000); // 硬件稳定时间

    //enb apd 使能
    if(vcc5VFirstCtrl(1)<0){
        return -1; // 设置电压级别失败
    }
    
    usleep(1000); // 硬件稳定时间

    if(vcc1v8FirstCtrl(1)<0){
        return -1; // 设置电压级别失败
    }
    
    usleep(1000); // 硬件稳定时间
    
    if(apdEnCtrl(1)<0){
        return -1; // 设置电压级别失败
    }

    usleep(1000); // 硬件稳定时间

    ///10V 输出
    if(apdVccCtrl((uint8_t *)&apdVal, sizeof(apdVal))<0){
        return -1; // 设置电压级别失败
    }

    return 0; // 初始化成功
}

// 先上1.8v 再上5v
int SecondVoltageCtrl()
{   
    // 1.8v 二级电压控制
    if(apdEnSecCtrl(0x38)<0){
        return -1; // 5v二级电压控制失败
    }
    usleep(1000);

    // 1.8v 二级电压控制
    if(apdEnSecCtrl(0x3f)<0){
        return -1; // 1.8v二级电压控制失败
    }
    usleep(1000);

    return 0;
}

// 控制高压输出电压和级别，voltage为SPI值；level为电压级别，0-3分别对应不同的电压范围
int HighVoltageCtrl(uint8_t voltage, uint8_t level)
{   
    //set vcc out level
    if(apdLevelCtrl(level)<0){
        return -1; // 设置电压级别失败
    }

    usleep(1000); // 硬件稳定时间

    if(apdEnCtrl(1)<0){
        return -1; // 设置电压级别失败
    }
    
    // 关闭二级输出
    if(apdVccCtrl((uint8_t *)&voltage, sizeof(voltage))<0){
        return -1; // 设置电压级别失败
    }

    usleep(1000);

    return 0;
}

// 关闭二级输出
int ShutdownVoltageCtrl()
{   

    // 二级控制 先下5V
    if(apdEnSecCtrl(0x38)<0){
        return -1; // 禁能APD失败
    }
    usleep(1000); // 硬件稳定时间

    // 再下1.8 
    if(apdEnSecCtrl(0x0)<0){
        return -1; // 禁能APD失败
    }
    usleep(1000); // 硬件稳定时间

    return 0;
}

int ApdTestEn(int test_en)
{
    return xdmaVideoSpace::xdmaWriteReg(0, APD_TEST_EN_ADDR, test_en);
}

int ApdGatherEn(int gather_en)
{
    return xdmaVideoSpace::xdmaWriteReg(0, APD_GATHER_EN_ADDR, gather_en);
}


// 片内变量 00000
int PcieChlCtrl(int chl)
{
    // chl为0表示原始数据通道
    // chl为1表示点云数据通道
    if (chl < 0 || chl > 1)
    {
        return -1; // 无效通道
    }
    // 通过FPGA控制寄存器设置通道
    return xdmaVideoSpace::xdmaWriteReg(0, PCIE_CHL_CTRL_ADDR, chl);
}

int EnDelayCtrl(int delay)
{
    if (delay < 0)
    {
        return -1; // 无效延迟值
    }
    return xdmaVideoSpace::xdmaWriteReg(0, APD_EN_DELAY_ADDR, delay);
}

int RecDelayCtrl(int delay)
{
    if (delay < 0)
    {
        return -1; // 无效延迟值
    }
    return xdmaVideoSpace::xdmaWriteReg(0, APD_REC_DELAY_ADDR, delay);
}

int CycleCtrl(int cycle)
{
    if (cycle < 0)
    {
        return -1; // 无效延迟值
    }
    return xdmaVideoSpace::xdmaWriteReg(0, APD_CYCLE_CTRL_ADDR, cycle);
}

int TriggerModeCtrl(int mode)
{
    // 触发模式控制寄存器地址为0x1030，mode为0表示内触发，1表示外触发
    if (mode < 0 || mode > 1)
    {
        return -1; // 无效触发模式
    }
    return xdmaVideoSpace::xdmaWriteReg(0, APD_TRIGGER_MODE_ADDR, mode);
}

int DiffThreholdCtrl(int threhold)
{
    // 差分阈值控制接口，值在0-100
    if (threhold < 0 || threhold > 100)
    {
        return -1; // 无效触发模式
    }
    return xdmaVideoSpace::xdmaWriteReg(0, APD_DIFF_THREHOLD_ADDR, threhold);
}

int StrideLengthCtrl(int length)
{
    // 步长控制接口，值为8 16 32，输入为log2
    if (length < 0 || length > 5)
    {
        return -1; // 无效触发模式
    }
    return xdmaVideoSpace::xdmaWriteReg(0, APD_STRIDE_LENGTH_ADDR, length);
}

int ApdConstructFrameCtrl(int buffer_num)
{
    // APD构建帧使能控制接口，buffer_num为1表示使能，0表示禁用
    if (buffer_num < 4)
    {
        return -1; // 无效参数
    }
    return xdmaVideoSpace::xdmaWriteReg(0, APD_CONSTRUCT_FRAMES_ADDR, buffer_num);
}

int GetFpgaVersion(){
    return xdmaVideoSpace::xdmaReadReg(0, FPGA_VERSION_ADDR);
}

int ApdGatherEnStatus(){
    return xdmaVideoSpace::xdmaReadReg(0, APD_GATHER_EN_ADDR);
}

int GetEnDelay()
{
    return xdmaVideoSpace::xdmaReadReg(0, APD_EN_DELAY_ADDR);
}

int GetRecDelay()
{
    return xdmaVideoSpace::xdmaReadReg(0, APD_REC_DELAY_ADDR);
}

int GetDiffThrehold()
{
    return xdmaVideoSpace::xdmaReadReg(0, APD_DIFF_THREHOLD_ADDR);
}

int GetStrideLength()
{
    return xdmaVideoSpace::xdmaReadReg(0, APD_STRIDE_LENGTH_ADDR);
}

int GetApdConstructFrameNumber()
{
    return xdmaVideoSpace::xdmaReadReg(0, APD_CONSTRUCT_FRAMES_ADDR);
}

int GetRegValue(int reg_addr)
{
    int value = xdmaVideoSpace::xdmaReadReg(0, reg_addr);
    return value;
}