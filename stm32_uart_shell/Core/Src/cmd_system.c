/**
 * @file    cmd_system.c
 * @brief   系统信息命令实现
 * @details 提供 version、reboot、uptime、free、ps 等系统级命令。
 *          这些命令展示了 Shell 与 HAL 适配层的交互方式。
 */

#include "uart_shell.h"
#include <string.h>
#include <stdio.h>

/* ---- 项目版本信息（编译时由构建系统或手动定义）---- */
#define FW_VERSION      "1.0.0"
#define FW_BUILD_DATE   __DATE__
#define FW_BUILD_TIME   __TIME__
#define FW_AUTHOR       "IoT Developer"

/**
 * @brief version - 显示固件版本信息
 */
static int cmd_version(int argc, char *argv[])
{
    shell_printf(ANSI_BOLD "=== System Info ===" ANSI_RESET "\r\n");
    shell_printf("  Firmware : v%s\r\n", FW_VERSION);
    shell_printf("  Build    : %s %s\r\n", FW_BUILD_DATE, FW_BUILD_TIME);
    shell_printf("  Author   : %s\r\n", FW_AUTHOR);
    shell_printf("  Compiler : GCC %d.%d.%d\r\n",
                 __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    return 0;
}

/**
 * @brief reboot - 软件复位
 */
static int cmd_reboot(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "-f") == 0) {
        /* 强制复位（无确认） */
        shell_printf("Force rebooting...\r\n");
        hal_system_reset();
        while (1);  /* 永不返回 */
    }

    shell_printf("Reboot system? (y/N): ");
    /* 简化：直接执行。实际项目中应等待用户输入确认 */
    shell_printf("Rebooting...\r\n");
    hal_system_reset();
    while (1);
    return 0;
}

/**
 * @brief uptime - 显示系统运行时间
 */
static int cmd_uptime(int argc, char *argv[])
{
    uint32_t ms = hal_get_tick();
    uint32_t sec  = ms / 1000;
    uint32_t min  = sec / 60;
    uint32_t hour = min / 60;
    uint32_t day  = hour / 24;

    shell_printf("Uptime: ");
    if (day > 0)  shell_printf("%lu day%s ", day, day > 1 ? "s" : "");
    if (hour > 0) shell_printf("%luh ", hour % 24);
    if (min > 0)  shell_printf("%lum ", min % 60);
    shell_printf("%lus\r\n", sec % 60);

    return 0;
}

/**
 * @brief free - 显示内存使用信息（STM32 栈/堆的简化版）
 */
static int cmd_free(int argc, char *argv[])
{
    extern int _estack;    /* 栈顶（链接脚本定义） */
    extern int _Min_Stack; /* 最小剩余栈空间（需启用栈水印） */

    /* 注意：实际的精确栈/堆统计需要链接脚本符号或 FreeRTOS 统计 */
    shell_printf("Memory Usage (approximate):\r\n");
    shell_printf("  SHELL_RX_BUF    : %d bytes\r\n", SHELL_RX_BUF_SIZE);
    shell_printf("  SHELL_HISTORY   : %d x %d = %d bytes\r\n",
                 SHELL_HISTORY_SIZE, SHELL_RX_BUF_SIZE,
                 SHELL_HISTORY_SIZE * SHELL_RX_BUF_SIZE);
    shell_printf("  shell_ctx_t     : %u bytes\r\n", (unsigned)sizeof(shell_ctx_t));
    shell_printf("  SHELL_MAX_CMDS  : %d\r\n", SHELL_MAX_CMDS);
    return 0;
}

/**
 * @brief ps - 显示任务/线程列表（配合 RTOS 使用）
 */
static int cmd_ps(int argc, char *argv[])
{
    shell_printf(ANSI_BOLD "  PID  STATE   STACK   NAME" ANSI_RESET "\r\n");
    shell_printf("  ---  ------  ------  ----\r\n");

#ifdef USE_FREERTOS
    /* 如果使用 FreeRTOS，可调用 vTaskList() */
    shell_printf("  (Enable FreeRTOS vTaskList for full output)\r\n");
#else
    /* 裸机模式：只显示主循环 */
    shell_printf("    0  RUN       -     main\r\n");
    shell_printf("    1  RUN       -     shell_rx\r\n");
#endif

    return 0;
}

/**
 * @brief date - 显示系统滴答时间（秒）
 */
static int cmd_date(int argc, char *argv[])
{
    uint32_t ms = hal_get_tick();
    uint32_t sec = ms / 1000;

    shell_printf("Tick: %lu ms (%lu sec since boot)\r\n",
                 (unsigned long)ms, (unsigned long)sec);
    return 0;
}

/* ---- 命令注册表 ---- */
static const shell_cmd_t cmds[] = {
    { "version", "Show firmware version info",     "version",          cmd_version, NULL },
    { "reboot",  "Reboot the system",              "reboot [-f]",      cmd_reboot,  NULL },
    { "uptime",  "Show system uptime",             "uptime",           cmd_uptime,  NULL },
    { "free",    "Show memory usage info",         "free",             cmd_free,    NULL },
    { "ps",      "Show task list (RTOS mode)",     "ps",               cmd_ps,      NULL },
    { "date",    "Show tick time since boot",      "date",             cmd_date,    NULL },
};

int cmd_system_register(void)
{
    extern shell_ctx_t *g_shell;
    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(cmds[0])); i++) {
        shell_register(g_shell, &cmds[i]);
    }
    return 0;
}
