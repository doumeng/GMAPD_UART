/******************************************************************************************
 * @copyright   : Copyright (c) 2021~2021, 四川赛狄信息技术股份公司
 * @author      : 袁登超
 * @version     :
 * @LastEditTime: 2021-11-25 13:16:34
 * @brief       :
 * @history     :
 ******************************************************************************************/
#ifndef __CLI_H__
#define __CLI_H__

#ifdef __cplusplus
extern "C"
{
#endif


#define VERSION_MAIN 1
#define VERSION_MAJOR 0
#define VERSION_MINOR 0

/***************/
/* CLI support */
/***************/

#define RC_SUCCESS (0)
#define RC_FAIL (-1)

/* Fra can implement new CLI functions using the cli_cmd() macro. */
typedef int (*cli_handle_t)(int argc, char *argv[]);

struct cli_table_entry
{
    const char *cmd;
    const char *explain;
    const cli_handle_t handle;
    const int resave;
};

int cli_cmd_start(void);

void cli_cmd_end(void);

void *cli_cmd_thread_fun(void *arg);
int print_help(int argc, char *argv[]);

/* 配置开关 */
/* 是否使用 POSIX ,0不使用，1使用 */
#define CLI_POSIX 1

/* 内置命令 */
#define CLI_BUILT_IN_CMD                          \
    {                                             \
        "help", "print CLI all cmd", print_help}, \
    {                                             \
        "q", "quit", NULL                         \
    }

#ifdef __cplusplus
}
#endif

#endif /* __CLI_H__ */