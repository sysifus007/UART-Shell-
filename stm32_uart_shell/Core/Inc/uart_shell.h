/**
 * @file    uart_shell.h
 * @brief   STM32 UART Shell/CLI - 轻量级串口命令行框架
 * @details 提供 Linux 风格的命令行界面，支持命令注册、自动补全、历史记录、
 *          通配符匹配等功能。设计为可移植的模块，通过 HAL 适配层与具体硬件解耦。
 * @version v1.0.0
 * @date    2026-04-30
 */

#ifndef __UART_SHELL_H
#define __UART_SHELL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* ======================== 配置宏 ======================== */

/** Shell 接收缓冲区大小（字节），含 '\0' */
#ifndef SHELL_RX_BUF_SIZE
#define SHELL_RX_BUF_SIZE        256
#endif

/** 最大命令行显示宽度（用于换行计算） */
#ifndef SHELL_LINE_WIDTH
#define SHELL_LINE_WIDTH         80
#endif

/** 最大支持的已注册命令数量 */
#ifndef SHELL_MAX_CMDS
#define SHELL_MAX_CMDS           32
#endif

/** 历史记录保存条数 */
#ifndef SHELL_HISTORY_SIZE
#define SHELL_HISTORY_SIZE       16
#endif

/** 最大单条命令的参数个数 */
#ifndef SHELL_MAX_ARGS
#define SHELL_MAX_ARGS           16
#endif

/** 是否启用 ANSI 转义码（颜色、光标控制等） */
#ifndef SHELL_USE_ANSI
#define SHELL_USE_ANSI           1
#endif

/* ======================== ANSI 颜色码 ======================== */

#if SHELL_USE_ANSI
#define ANSI_RESET       "\033[0m"
#define ANSI_RED         "\033[31m"
#define ANSI_GREEN       "\033[32m"
#define ANSI_YELLOW      "\033[33m"
#define ANSI_BLUE        "\033[34m"
#define ANSI_CYAN        "\033[36m"
#define ANSI_WHITE       "\033[37m"
#define ANSI_BOLD        "\033[1m"
#define ANSI_CURSOR_UP   "\033[A"
#define ANSI_CLEAR_LINE  "\033[2K"
#else
#define ANSI_RESET       ""
#define ANSI_RED         ""
#define ANSI_GREEN       ""
#define ANSI_YELLOW      ""
#define ANSI_BLUE        ""
#define ANSI_CYAN        ""
#define ANSI_WHITE       ""
#define ANSI_BOLD        ""
#define ANSI_CURSOR_UP   ""
#define ANSI_CLEAR_LINE  ""
#endif

/* ======================== 回调函数指针 ======================== */

/**
 * @brief 命令处理回调函数类型
 * @param argc 参数个数
 * @param argv 参数字符串数组（argv[0] 为命令名本身）
 * @return 0 成功，非 0 错误码
 */
typedef int (*shell_cmd_handler_t)(int argc, char *argv[]);

/**
 * @brief 自动补全回调函数类型
 * @param index  匹配到的候选索引（从 0 开始）
 * @param buf    当前输入缓冲区
 * @param buflen 缓冲区当前长度
 * @return 完整的命令字符串（静态存储），或 NULL 表示无更多候选
 */
typedef const char *(*shell_complete_t)(int index, const char *buf, size_t buflen);

/* ======================== 命令结构体 ======================== */

/**
 * @brief Shell 命令描述结构体
 */
typedef struct {
    const char *name;       /**< 命令名称 */
    const char *help;       /**< 帮助信息（一行描述） */
    const char *usage;      /**< 详细用法（可选，可为 NULL） */
    shell_cmd_handler_t handler;  /**< 命令处理函数 */
    shell_complete_t  completer;  /**< 自动补全函数（可选，可为 NULL） */
} shell_cmd_t;

/* ======================== Shell 上下文 ======================== */

/**
 * @brief Shell 运行时上下文
 */
typedef struct {
    /* ---- 接收状态 ---- */
    char  rx_buf[SHELL_RX_BUF_SIZE]; /**< 当前正在编辑的命令行 */
    int   rx_pos;                     /**< 当前光标位置 */
    int   rx_len;                     /**< 当前命令行有效长度 */

    /* ---- 历史记录 ---- */
    char  history[SHELL_HISTORY_SIZE][SHELL_RX_BUF_SIZE]; /**< 历史命令缓冲 */
    int   history_count;      /**< 已存储的历史条数 */
    int   history_index;      /**< 浏览历史时的当前索引（-1 表示当前行） */
    char  history_tmp[SHELL_RX_BUF_SIZE]; /**< 保存当前行（进入历史浏览前） */

    /* ---- 命令表 ---- */
    shell_cmd_t *cmds[SHELL_MAX_CMDS]; /**< 已注册命令指针数组 */
    int   cmd_count;         /**< 已注册命令数 */

    /* ---- 标志位 ---- */
    uint8_t running : 1;     /**< Shell 是否在运行 */
    uint8_t echo    : 1;     /**< 是否回显输入字符 */

    /* ---- 统计信息 ---- */
    uint32_t cmd_executed;   /**< 已执行的命令总数 */
} shell_ctx_t;

/* ======================== 公共 API ======================== */

/**
 * @brief  初始化 Shell 上下文
 * @param  shell 指向 shell_ctx_t 实例的指针（通常使用全局变量）
 * @retval 0 成功，-1 参数无效
 */
int shell_init(shell_ctx_t *shell);

/**
 * @brief  注册一个命令
 * @param  shell  Shell 上下文
 * @param  cmd    命令结构体指针（指向静态/全局存储，不会被复制）
 * @retval 0 成功，-1 表满，-2 参数无效
 */
int shell_register(shell_ctx_t *shell, const shell_cmd_t *cmd);

/**
 * @brief  注销一个命令
 * @param  shell  Shell 上下文
 * @param  name   要注销的命令名
 * @retval 0 成功，-1 未找到
 */
int shell_unregister(shell_ctx_t *shell, const char *name);

/**
 * @brief  查找命令
 * @param  shell Shell 上下文
 * @param  name  命令名
 * @return 命令结构体指针，或 NULL
 */
const shell_cmd_t *shell_find(shell_ctx_t *shell, const char *name);

/**
 * @brief  向 Shell 喂入一个接收到的字节（在中断中调用）
 * @param  shell Shell 上下文
 * @param  ch    接收到的字节
 * @note   此函数处理所有行编辑逻辑（退格、Tab 补全、上下历史等），
 *         完整命令收到 '\r' 或 '\n' 后自动执行。
 */
void shell_feed(shell_ctx_t *shell, char ch);

/**
 * @brief  Shell 主循环（可选，也可完全靠 shell_feed 中断驱动）
 * @param  shell Shell 上下文
 * @note   使用 DMA 空闲中断 + shell_feed 的方式更高效，
 *         本函数仅为轮询模式备用。
 */
void shell_run(shell_ctx_t *shell);

/**
 * @brief  直接执行一条命令字符串（供程序内部调用）
 * @param  shell    Shell 上下文
 * @param  cmd_str  命令字符串
 * @retval 命令返回值
 */
int shell_exec(shell_ctx_t *shell, const char *cmd_str);

/**
 * @brief  打印 Shell 提示符
 * @param  shell Shell 上下文
 */
void shell_print_prompt(shell_ctx_t *shell);

/**
 * @brief  获取命令数量
 * @param  shell Shell 上下文
 * @return 已注册的命令数
 */
int shell_cmd_count(shell_ctx_t *shell);

/**
 * @brief  获取第 i 个已注册命令
 * @param  shell Shell 上下文
 * @param  index 索引
 * @return 命令指针，或 NULL
 */
const shell_cmd_t *shell_cmd_at(shell_ctx_t *shell, int index);

/**
 * @brief  Shell 格式化输出（类似 printf，发送到 UART）
 * @param  fmt 格式字符串
 * @param  ... 可变参数
 */
void shell_printf(const char *fmt, ...);

/**
 * @brief  Shell 原始字节输出
 * @param  data 数据指针
 * @param  len  数据长度
 */
void shell_write(const void *data, size_t len);

/**
 * @brief  处理完整的 ESC 序列（供 DMA 接收+解析模式使用）
 * @param  shell Shell 上下文
 * @param  seq   完整的 ESC 序列（如 "\033[A"）
 * @param  len   序列长度
 * @note   支持方向键（上下左右）和 Delete 键
 */
void shell_handle_esc_seq(shell_ctx_t *shell, const char *seq, int len);

/* ======================== HAL 适配层（需用户实现）======================== */

/**
 * @brief  HAL 适配层：初始化 UART
 * @retval 0 成功，-1 失败
 */
int  hal_uart_init(void);

/**
 * @brief  HAL 适配层：发送一个字节
 * @param  ch 字符
 */
void hal_uart_putc(char ch);

/**
 * @brief  HAL 适配层：发送缓冲区
 * @param  data 数据
 * @param  len  长度
 */
void hal_uart_write(const uint8_t *data, size_t len);

/**
 * @brief  HAL 适配层：查询接收 FIFO 中是否有数据（轮询模式用）
 * @retval 1 有数据，0 无数据
 */
int  hal_uart_readable(void);

/**
 * @brief  HAL 适配层：读取一个字节（轮询模式用）
 * @retval 读取到的字节，或 -1 无数据
 */
int  hal_uart_getc(void);

/**
 * @brief  HAL 适配层：获取系统运行节拍数（用于 uptime 等命令）
 * @retval 毫秒数
 */
uint32_t hal_get_tick(void);

/**
 * @brief  HAL 适配层：软件复位
 * @note   永不返回
 */
void hal_system_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __UART_SHELL_H */
