#ifndef __XDMA_VIDEO_DEFINE_H_
#define __XDMA_VIDEO_DEFINE_H_

/*图像偏移寄存器基地址*/
#define IMG_CTRL_BASE_REG_OFFSET (0x0000)
/*控制寄存器偏移*/
#define CTL_REG_OFFSET          (0x1000)
/*图像控制寄存器*/
#define IMAGE_CTL_BASE_OFFSET   (0x1100)

/*用户自定义寄存访问基地址*/
#define USER_CTL_BASE_OFFSET    (0x4000)
/*控制寄存器定义*/
#define CTL_REG_LED                 (CTL_REG_OFFSET + 0x00)
#define CTL_REG_VERSION_DATE        (CTL_REG_OFFSET + 0x04)
#define CTL_REG_VERSION_TIME        (CTL_REG_OFFSET + 0x08)
#define CTL_REG_TEMP_DSP_FPGA       (CTL_REG_OFFSET + 0x0C)
#define CTL_REG_TEMP_3399_BOARD     (CTL_REG_OFFSET + 0x10)

/*srio,pcie, camareLink 链接状态*/
#define CTL_REG_LINK_STATE_BASE (CTL_REG_OFFSET + 0x400)

#define ALIGN_TO(a,b)   (a+(b-1))&(~(b-1)) 

#define CTL_REG_IRQ                         (CTL_REG_OFFSET + 0x14)             // 使用
#define CTL_REG_IMATGE_CHL_INFO(chl)        (CTL_REG_OFFSET + 0x30 + (chl * 0x20)) // 使用
#define CTL_REG_IMATGE_CHL_ADDR(chl)        (CTL_REG_OFFSET + 0x34 + (chl * 0x20)) // 使用
#define CTL_REG_IMATGE_CHL_CUR(chl)         (CTL_REG_OFFSET + 0x38 + (chl * 0x20)) // 使用



/* 使能图像处理 */
#define CTL_REG_CHL_ENABLE(chl) (IMAGE_CTL_BASE_OFFSET + 0x40 + (chl * 4))
/* 码流发送的ddr地址寄存器 */
#define CTL_REG_BITSTEAM_SEND_ADDR(chl) (IMAGE_CTL_BASE_OFFSET + 0x80 + (chl * 4))
/* 本次码流的长度和状态寄存器 */
#define CTL_REG_BITSTEAM_SEND_LEN(chl) (IMAGE_CTL_BASE_OFFSET + 0x20 + (chl * 4))
#define CTL_REG_BITSTEAM_SEND_ADDR_BACK(chl) (IMAGE_CTL_BASE_OFFSET + 0x300 + (chl * 4))



/* 拼接图像 point check */

#define CTR_IMG_POINT_REG(chl) (IMG_CTRL_BASE_REG_OFFSET + (chl * 4))

/* 图像矫正使能控制寄存器 */
#define CTRL_CORRECT_ENB_REG (IMG_CTRL_BASE_REG_OFFSET + (0xc))

typedef enum {
    IMG0_BASE_ADDR,
    IMG1_BASE_ADDR = 0x140000,
    IMG2_BASE_ADDR = 0x280000,
    IMG3_BASE_ADDR
}IMG_CTRL_BASE_ADDR_E;


#endif // __XDMA_VIDEO_DEFINE_H_