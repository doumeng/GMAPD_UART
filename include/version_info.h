// version_info.h
#ifndef __VERSION_INFO_H__
#define __VERSION_INFO_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

typedef struct
{
    // 版本号
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint8_t build;

    // Git信息
    char git_hash[41];   // SHA-1 hash (40 chars + null)
    char git_branch[32]; // 分支名称
    char git_tag[32];    // 标签（如果有）
    uint8_t git_dirty;   // 工作区是否有未提交更改

    // 编译信息
    char build_date[11];      // YYYY-MM-DD
    char build_time[9];       // HH:MM:SS
    uint32_t build_timestamp; // Unix时间戳

    // 编译选项
    char compiler[32];     // 编译器版本
    char build_type[16];   // Debug/Release
    char optimization[16]; // -O0, -O1, -O2, -O3, -Os等

    // 校验和（可选）
    uint32_t checksum; // 结构体校验和
} version_info_t;

const version_info_t* get_version_info(void);
void print_version_info(void);


#ifdef __cplusplus
}
#endif


#endif // VERSION_INFO_H
