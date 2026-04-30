/**
 * @file    pc_sim_main.c
 * @brief   UART Shell PC 模拟版 - 脱离硬件直接在电脑上运行
 * @details 使用 stdin/stdout 替代真实串口，模拟 STM32 的 hal_get_tick()。
 *          可以直接编译运行来验证 Shell 的所有功能。
 *
 * 编译方法：
 *   Windows (MinGW/MSVC):
 *     gcc -o shell_sim pc_sim_main.c uart_shell.c cmd_system.c cmd_led.c cmd_sensor.c -DPC_SIM
 *
 *   Linux / macOS:
 *     gcc -o shell_sim pc_sim_main.c uart_shell.c cmd_system.c cmd_led.c cmd_sensor.c -DPC_SIM
 *
 * 运行：
 *   ./shell_sim
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>
#endif

#include "uart_shell.h"

/* ======================== PC 模拟的 HAL 适配 ======================== */

static uint32_t pc_tick_start = 0;

int hal_uart_init(void)
{
    pc_tick_start = (uint32_t)(clock() * 1000 / CLOCKS_PER_SEC);
    printf("[HAL] UART initialized (PC simulation mode)\r\n");
    return 0;
}

void hal_uart_putc(char ch)
{
    putchar(ch);
    fflush(stdout);
}

void hal_uart_write(const uint8_t *data, size_t len)
{
    if (data && len > 0) {
        fwrite(data, 1, len, stdout);
        fflush(stdout);
    }
}

int hal_uart_readable(void)
{
#ifdef _WIN32
    return _kbhit();
#else
    struct timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
#endif
}

int hal_uart_getc(void)
{
#ifdef _WIN32
    if (_kbhit()) {
        return _getch();
    }
    return -1;
#else
    return getchar();
#endif
}

uint32_t hal_get_tick(void)
{
    return (uint32_t)(clock() * 1000 / CLOCKS_PER_SEC) - pc_tick_start;
}

void hal_system_reset(void)
{
    printf("\r\n[System] Resetting (simulated - would NVIC_SystemReset on MCU)\r\n");
    exit(0);
}

/* ======================== PC 终端原始模式 ======================== */

#ifdef _WIN32
static DWORD orig_mode = 0;

static void pc_terminal_init(void)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(h, &orig_mode);
    SetConsoleMode(h, orig_mode & ~ENABLE_LINE_INPUT & ~ENABLE_ECHO_INPUT);
    /* 尝试启用 ANSI 转义码（Win10+） */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

static void pc_terminal_restore(void)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    SetConsoleMode(h, orig_mode);
}
#else
static struct termios orig_termios;

static void pc_terminal_init(void)
{
    struct termios t;
    tcgetattr(0, &orig_termios);
    t = orig_termios;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
}

static void pc_terminal_restore(void)
{
    tcsetattr(0, TCSANOW, &orig_termios);
}
#endif

/* ======================== main ======================== */

int main(void)
{
    shell_ctx_t shell;

    /* 初始化终端原始模式 */
    pc_terminal_init();

    /* 初始化 Shell */
    shell_init(&shell);

    /* 注册命令模块 */
    cmd_system_register();
    cmd_led_register();
    cmd_sensor_register();

    /* 初始化 UART（模拟） */
    hal_uart_init();

    /* 欢迎信息 */
    shell_printf("\r\n");
    shell_printf(ANSI_BOLD ANSI_CYAN);
    shell_printf("===================================\r\n");
    shell_printf("  UART Shell v1.0.0 (PC Simulator)\r\n");
    shell_printf("  Type 'help' for commands\r\n");
    shell_printf("  Press Ctrl+C to exit\r\n");
    shell_printf("===================================\r\n");
    shell_printf(ANSI_RESET);
    shell_printf("\r\n");

    /* 显示提示符 */
    shell_print_prompt(&shell);

    /* 主循环：轮询 stdin */
    while (shell.running) {
        if (hal_uart_readable()) {
            int ch = hal_uart_getc();
            if (ch < 0) continue;

            /* Ctrl+C -> 退出 */
            if (ch == 0x03) {
                shell_printf("^C\r\n");
                break;
            }

            shell_feed(&shell, (char)ch);
        }
#ifdef _WIN32
        Sleep(1);
#else
        usleep(1000);
#endif
    }

    /* 恢复终端 */
    pc_terminal_restore();
    shell_printf("\r\n[Shell] Goodbye!\r\n");

    return 0;
}
