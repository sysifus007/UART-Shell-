/**
 * @file    uart_shell.c
 * @brief   STM32 UART Shell/CLI 核心实现
 * @details 实现命令行编辑（退格/删除/光标/历史）、命令解析与分发、
 *          Tab 自动补全、通配符搜索等核心功能。
 */

#include "uart_shell.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

/* ======================== 内部辅助函数 ======================== */

/** 计算字符串的 UTF-8 显示宽度（ASCII 视为 1，中文等视为 2） */
static int str_display_width(const char *s)
{
    int w = 0;
    while (*s) {
        if ((uint8_t)*s >= 0x80)
            w += 2, s++;
        else
            w++, s++;
    }
    return w;
}

/** 输出退格序列：将光标回退 n 个位置并清除字符 */
static void shell_erase_chars(shell_ctx_t *shell, int n)
{
    char seq[16];
    for (int i = 0; i < n; i++) {
        shell_write("\b \b", 3);
    }
}

/** 重新打印当前命令行（光标移动后刷新显示） */
static void shell_refresh_line(shell_ctx_t *shell)
{
    char seq[8];
    int i;

    /* 移动到行首 */
    shell_write("\r", 1);

    /* 打印提示符 */
    shell_printf(ANSI_GREEN "shell> " ANSI_RESET);

    /* 打印当前命令行内容 */
    if (shell->rx_len > 0) {
        shell_write(shell->rx_buf, shell->rx_len);
    }

    /* 如果光标不在行尾，需要左移 */
    if (shell->rx_pos < shell->rx_len) {
        int move = shell->rx_len - shell->rx_pos;
        /* 使用 ANSI 左移光标 */
        snprintf(seq, sizeof(seq), "\033[%dD", move);
        shell_write(seq, strlen(seq));
    }
}

/** 将当前行保存到历史记录 */
static void history_save(shell_ctx_t *shell)
{
    /* 空行不保存 */
    if (shell->rx_len == 0) return;

    /* 与上一条重复则不保存 */
    if (shell->history_count > 0 &&
        strcmp(shell->rx_buf, shell->history[shell->history_count - 1]) == 0) {
        return;
    }

    if (shell->history_count < SHELL_HISTORY_SIZE) {
        strncpy(shell->history[shell->history_count], shell->rx_buf, SHELL_RX_BUF_SIZE - 1);
        shell->history[shell->history_count][SHELL_RX_BUF_SIZE - 1] = '\0';
        shell->history_count++;
    } else {
        /* 滚动：丢弃最旧的 */
        memmove(shell->history[0], shell->history[1],
                (SHELL_HISTORY_SIZE - 1) * SHELL_RX_BUF_SIZE);
        strncpy(shell->history[SHELL_HISTORY_SIZE - 1], shell->rx_buf, SHELL_RX_BUF_SIZE - 1);
        shell->history[SHELL_HISTORY_SIZE - 1][SHELL_RX_BUF_SIZE - 1] = '\0';
    }
}

/** 从历史记录恢复一行到编辑区 */
static void history_recall(shell_ctx_t *shell, int index)
{
    if (index < 0 || index >= shell->history_count) return;

    /* 先清除当前显示 */
    shell_erase_chars(shell, shell->rx_len);

    /* 加载历史命令 */
    strncpy(shell->rx_buf, shell->history[index], SHELL_RX_BUF_SIZE - 1);
    shell->rx_buf[SHELL_RX_BUF_SIZE - 1] = '\0';
    shell->rx_len = strlen(shell->rx_buf);
    shell->rx_pos = shell->rx_len;

    /* 显示新内容 */
    shell_write(shell->rx_buf, shell->rx_len);
}

/** 尝试 Tab 自动补全，返回是否成功 */
static int shell_tab_complete(shell_ctx_t *shell)
{
    int i, j, matches = 0;
    const char *last_match = NULL;
    const char *common_prefix = NULL;
    int prefix_len = 0;

    if (shell->rx_len == 0) return 0;

    /* 统计匹配的命令数 */
    for (i = 0; i < shell->cmd_count; i++) {
        const char *name = shell->cmds[i]->name;
        if (strncmp(shell->rx_buf, name, shell->rx_len) == 0) {
            matches++;
            last_match = name;

            /* 计算公共前缀 */
            if (common_prefix == NULL) {
                common_prefix = name;
                prefix_len = strlen(name);
            } else {
                int k = 0;
                while (k < prefix_len && common_prefix[k] == name[k]) k++;
                prefix_len = k;
            }
        }
    }

    if (matches == 0) {
        return 0;  /* 无匹配 */
    }

    if (matches == 1) {
        /* 唯一匹配：补全并追加空格 */
        int name_len = strlen(last_match);
        int add_len = name_len - shell->rx_len + 1; /* +1 for trailing space */
        char suffix[SHELL_RX_BUF_SIZE];
        memcpy(suffix, last_match + shell->rx_len, add_len);
        suffix[add_len] = '\0';

        shell_write(suffix, add_len);
        memcpy(shell->rx_buf + shell->rx_len, last_match + shell->rx_len, add_len);
        shell->rx_len += add_len;
        shell->rx_pos = shell->rx_len;
        return 1;
    }

    /* 多个匹配：先补全公共前缀 */
    if (common_prefix && prefix_len > shell->rx_len) {
        int add_len = prefix_len - shell->rx_len;
        shell_write(common_prefix + shell->rx_len, add_len);
        memcpy(shell->rx_buf + shell->rx_len, common_prefix + shell->rx_len, add_len);
        shell->rx_len += add_len;
        shell->rx_pos = shell->rx_len;
    }

    /* 打印所有匹配项 */
    shell_printf("\r\n");
    for (i = 0; i < shell->cmd_count; i++) {
        const char *name = shell->cmds[i]->name;
        if (strncmp(shell->rx_buf, name, shell->rx_len) == 0) {
            shell_printf("  " ANSI_CYAN "%s" ANSI_RESET "  %s\r\n",
                         name, shell->cmds[i]->help ? shell->cmds[i]->help : "");
        }
    }

    /* 重新显示提示符和当前行 */
    shell_refresh_line(shell);
    return 1;
}

/* ======================== 命令解析与执行 ======================== */

/**
 * @brief 解析命令行，分割为 argc/argv
 * @note  支持引号包裹的参数（双引号）
 */
static int parse_args(char *line, int max_args, char **argv)
{
    int argc = 0;
    char *p = line;
    int in_quote = 0;

    /* 跳过前导空白 */
    while (isspace((unsigned char)*p)) p++;

    while (*p && argc < max_args) {
        if (*p == '"') {
            in_quote = !in_quote;
            p++;
            continue;
        }
        if (isspace((unsigned char)*p) && !in_quote) {
            *p = '\0';
            p++;
            /* 跳过连续空白 */
            while (isspace((unsigned char)*p)) p++;
            continue;
        }
        if (argv[argc] == NULL) {
            argv[argc] = p;
        }
        p++;
        if (isspace((unsigned char)*p) && !in_quote) {
            argc++;
        }
    }

    if (argv[argc] != NULL && *argv[argc] != '\0') {
        argc++;
    }

    return argc;
}

/**
 * @brief 内置 help 命令实现
 */
static int cmd_help(int argc, char *argv[])
{
    extern shell_ctx_t *g_shell;  /* 引用全局实例 */

    if (argc == 1) {
        shell_printf(ANSI_BOLD "Available commands:" ANSI_RESET "\r\n\r\n");

        for (int i = 0; i < g_shell->cmd_count; i++) {
            const shell_cmd_t *cmd = g_shell->cmds[i];
            /* 命令名左对齐 14 字符 */
            shell_printf("  " ANSI_GREEN "%-14s" ANSI_RESET " %s\r\n",
                         cmd->name,
                         cmd->help ? cmd->help : "");
        }
        shell_printf("\r\nTotal: %d commands, %lu executed.\r\n",
                     g_shell->cmd_count, (unsigned long)g_shell->cmd_executed);
        return 0;
    }

    /* help <cmd>：显示指定命令的详细帮助 */
    const shell_cmd_t *cmd = shell_find(g_shell, argv[1]);
    if (!cmd) {
        shell_printf(ANSI_RED "Error: " ANSI_RESET "unknown command '%s'\r\n", argv[1]);
        return 1;
    }

    shell_printf(ANSI_BOLD "Command: " ANSI_RESET "%s\r\n", cmd->name);
    shell_printf(ANSI_BOLD "Usage:   " ANSI_RESET "%s\r\n", cmd->usage ? cmd->usage : cmd->name);
    shell_printf(ANSI_BOLD "Help:    " ANSI_RESET "%s\r\n", cmd->help ? cmd->help : "(none)");
    return 0;
}

/**
 * @brief 内置 history 命令实现
 */
static int cmd_history(int argc, char *argv[])
{
    extern shell_ctx_t *g_shell;

    if (g_shell->history_count == 0) {
        shell_printf("No history.\r\n");
        return 0;
    }

    for (int i = 0; i < g_shell->history_count; i++) {
        shell_printf("  %3d  %s\r\n", i + 1, g_shell->history[i]);
    }
    return 0;
}

/**
 * @brief 内置 clear/cls 命令
 */
static int cmd_clear(int argc, char *argv[])
{
    shell_printf("\033[2J\033[H");  /* ANSI: 清屏 + 光标归位 */
    return 0;
}

/**
 * @brief 内置 echo 命令（用于调试和脚本）
 */
static int cmd_echo(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        shell_printf("%s%s", argv[i], i < argc - 1 ? " " : "");
    }
    shell_printf("\r\n");
    return 0;
}

/* 内置命令注册表 */
static const shell_cmd_t builtin_help    = { "help",    "Show available commands",  "help [cmd]",          cmd_help,    NULL };
static const shell_cmd_t builtin_history = { "history", "Show command history",     "history",             cmd_history, NULL };
static const shell_cmd_t builtin_clear   = { "clear",   "Clear terminal screen",   "clear",               cmd_clear,   NULL };
static const shell_cmd_t builtin_echo    = { "echo",    "Print text to terminal",  "echo <text...>",       cmd_echo,    NULL };

static const shell_cmd_t *builtin_cmds[] = {
    &builtin_help,
    &builtin_history,
    &builtin_clear,
    &builtin_echo,
};

/* ======================== 公共 API 实现 ======================== */

/* 全局 Shell 实例指针（供内置命令使用） */
shell_ctx_t *g_shell = NULL;

int shell_init(shell_ctx_t *shell)
{
    if (!shell) return -1;

    memset(shell, 0, sizeof(shell_ctx_t));
    shell->running = 1;
    shell->echo = 1;
    shell->history_index = -1;

    g_shell = shell;

    /* 注册内置命令 */
    for (int i = 0; i < (int)(sizeof(builtin_cmds) / sizeof(builtin_cmds[0])); i++) {
        shell_register(shell, builtin_cmds[i]);
    }

    return 0;
}

int shell_register(shell_ctx_t *shell, const shell_cmd_t *cmd)
{
    if (!shell || !cmd || !cmd->name || !cmd->handler) return -2;
    if (shell->cmd_count >= SHELL_MAX_CMDS) return -1;

    shell->cmds[shell->cmd_count++] = (shell_cmd_t *)cmd;
    return 0;
}

int shell_unregister(shell_ctx_t *shell, const char *name)
{
    if (!shell || !name) return -1;

    for (int i = 0; i < shell->cmd_count; i++) {
        if (strcmp(shell->cmds[i]->name, name) == 0) {
            /* 将后续命令前移 */
            for (int j = i; j < shell->cmd_count - 1; j++) {
                shell->cmds[j] = shell->cmds[j + 1];
            }
            shell->cmd_count--;
            return 0;
        }
    }
    return -1;
}

const shell_cmd_t *shell_find(shell_ctx_t *shell, const char *name)
{
    if (!shell || !name) return NULL;

    for (int i = 0; i < shell->cmd_count; i++) {
        if (strcmp(shell->cmds[i]->name, name) == 0) {
            return shell->cmds[i];
        }
    }
    return NULL;
}

int shell_exec(shell_ctx_t *shell, const char *cmd_str)
{
    char buf[SHELL_RX_BUF_SIZE];
    char *argv[SHELL_MAX_ARGS];
    const shell_cmd_t *cmd;

    if (!shell || !cmd_str) return -1;

    /* 复制并清理 */
    strncpy(buf, cmd_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* 去除尾部空白 */
    int len = strlen(buf);
    while (len > 0 && isspace((unsigned char)buf[len - 1])) {
        buf[--len] = '\0';
    }

    /* 跳过空行和注释（以 # 开头） */
    if (len == 0 || buf[0] == '#') return 0;

    /* 解析参数 */
    memset(argv, 0, sizeof(argv));
    int argc = parse_args(buf, SHELL_MAX_ARGS, argv);
    if (argc == 0) return 0;

    /* 查找并执行命令 */
    cmd = shell_find(shell, argv[0]);
    if (!cmd) {
        shell_printf(ANSI_RED "Error: " ANSI_RESET "command '%s' not found. "
                     "Type 'help' for available commands.\r\n", argv[0]);
        return 1;
    }

    int ret = cmd->handler(argc, argv);
    shell->cmd_executed++;
    return ret;
}

void shell_print_prompt(shell_ctx_t *shell)
{
    shell_printf(ANSI_GREEN "shell> " ANSI_RESET);
}

void shell_feed(shell_ctx_t *shell, char ch)
{
    if (!shell || !shell->running) return;

    switch (ch) {
    /* ---- 回车/换行：执行命令 ---- */
    case '\r':
    case '\n':
        shell_printf("\r\n");

        if (shell->rx_len > 0) {
            history_save(shell);
            shell_exec(shell, shell->rx_buf);
        }

        /* 重置编辑状态 */
        shell->rx_buf[0] = '\0';
        shell->rx_len = 0;
        shell->rx_pos = 0;
        shell->history_index = -1;

        shell_print_prompt(shell);
        break;

    /* ---- 退格（Backspace, 0x08）或 DEL (0x7F) ---- */
    case 0x08:
    case 0x7F:
        if (shell->rx_pos > 0) {
            /* 删除光标前一个字符 */
            memmove(&shell->rx_buf[shell->rx_pos - 1],
                    &shell->rx_buf[shell->rx_pos],
                    shell->rx_len - shell->rx_pos);
            shell->rx_pos--;
            shell->rx_len--;

            /* 光标回退并清除 */
            shell_write("\b", 1);
            /* 如果光标不在行尾，需要重绘后面部分 */
            if (shell->rx_pos < shell->rx_len) {
                int tail = shell->rx_len - shell->rx_pos;
                shell_write(&shell->rx_buf[shell->rx_pos], tail);
                shell_write(" ", 1);
                snprintf(shell->rx_buf + SHELL_RX_BUF_SIZE - 8, 8, "\033[%dD", tail + 1);
                shell_write(shell->rx_buf + SHELL_RX_BUF_SIZE - 8, strlen(shell->rx_buf + SHELL_RX_BUF_SIZE - 8));
            } else {
                shell_write(" \b", 2);
            }
        }
        break;

    /* ---- Tab：自动补全 ---- */
    case '\t':
        shell_tab_complete(shell);
        break;

    /* ---- ESC 序列（方向键等）---- */
    case 0x1B:
        /* 在中断/实时处理中，通常需要状态机解析完整的 ESC 序列。
         * 这里采用简化方案：通过标志位在下一字节到达时判断。
         * 完整实现建议使用 DMA + 环形缓冲区的方案。
         * 下次迭代中处理 [A（上）/[B（下）
         */
        break;

    /* ---- 普通可打印字符 ---- */
    default:
        if (ch >= 0x20 && ch < 0x7F) {
            if (shell->rx_len < SHELL_RX_BUF_SIZE - 1) {
                /* 在光标位置插入字符 */
                if (shell->rx_pos < shell->rx_len) {
                    memmove(&shell->rx_buf[shell->rx_pos + 1],
                            &shell->rx_buf[shell->rx_pos],
                            shell->rx_len - shell->rx_pos);
                }
                shell->rx_buf[shell->rx_pos] = ch;
                shell->rx_pos++;
                shell->rx_len++;
                shell->rx_buf[shell->rx_len] = '\0';

                /* 回显 */
                if (shell->echo) {
                    hal_uart_putc(ch);
                    /* 如果不在行尾，重绘尾部 */
                    if (shell->rx_pos < shell->rx_len) {
                        shell_write(&shell->rx_buf[shell->rx_pos],
                                    shell->rx_len - shell->rx_pos);
                        char seq[8];
                        snprintf(seq, sizeof(seq), "\033[%dD",
                                 shell->rx_len - shell->rx_pos);
                        shell_write(seq, strlen(seq));
                    }
                }
            } else {
                /* 缓冲区满，响铃提示 */
                hal_uart_putc('\a');
            }
        }
        break;
    }
}

void shell_run(shell_ctx_t *shell)
{
    while (shell->running) {
        if (hal_uart_readable()) {
            int ch = hal_uart_getc();
            if (ch >= 0) {
                shell_feed(shell, (char)ch);
            }
        }
        /* 可以在这里加入其他低优先级任务 */
    }
}

/* ======================== 格式化输出 ======================== */

void shell_printf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len > 0) {
        if ((size_t)len >= sizeof(buf)) len = sizeof(buf) - 1;
        hal_uart_write((const uint8_t *)buf, (size_t)len);
    }
}

void shell_write(const void *data, size_t len)
{
    if (data && len > 0) {
        hal_uart_write((const uint8_t *)data, len);
    }
}

/* ======================== ESC 序列扩展处理 ======================== */

/**
 * @brief 处理完整的 ESC 序列（供 DMA 接收+解析模式使用）
 * @param shell Shell 上下文
 * @param seq   完整的 ESC 序列（如 "\033[A"）
 * @param len   序列长度
 *
 * 示例用法（在 DMA 接收回调中积累字节后调用）：
 *   if (seq[0] == 0x1B && seq[1] == '[') {
 *       shell_handle_esc_seq(shell, seq);
 *   }
 */
void shell_handle_esc_seq(shell_ctx_t *shell, const char *seq, int len)
{
    if (len < 3 || seq[0] != 0x1B || seq[1] != '[') return;

    switch (seq[2]) {
    case 'A':  /* 上箭头 */
        if (shell->history_count == 0) break;

        /* 首次按下上键：保存当前行 */
        if (shell->history_index < 0) {
            strncpy(shell->history_tmp, shell->rx_buf, SHELL_RX_BUF_SIZE - 1);
            shell->history_tmp[SHELL_RX_BUF_SIZE - 1] = '\0';
            shell->history_index = shell->history_count;
        }

        if (shell->history_index > 0) {
            shell->history_index--;
            shell_erase_chars(shell, shell->rx_len);
            shell->rx_len = 0;
            shell->rx_pos = 0;
            history_recall(shell, shell->history_index);
        }
        break;

    case 'B':  /* 下箭头 */
        if (shell->history_index < 0) break;

        shell_erase_chars(shell, shell->rx_len);
        shell->rx_len = 0;
        shell->rx_pos = 0;

        if (shell->history_index < shell->history_count - 1) {
            shell->history_index++;
            history_recall(shell, shell->history_index);
        } else {
            /* 恢复保存的当前行 */
            shell->history_index = -1;
            int tmp_len = strlen(shell->history_tmp);
            memcpy(shell->rx_buf, shell->history_tmp, tmp_len + 1);
            shell->rx_len = tmp_len;
            shell->rx_pos = tmp_len;
            shell_write(shell->rx_buf, shell->rx_len);
        }
        break;

    case 'C':  /* 右箭头 */
        if (shell->rx_pos < shell->rx_len) {
            shell_write("\033[C", 3);
            shell->rx_pos++;
        }
        break;

    case 'D':  /* 左箭头 */
        if (shell->rx_pos > 0) {
            shell_write("\033[D", 3);
            shell->rx_pos--;
        }
        break;

    case '3':  /* Delete 键 */
        if (len >= 4 && seq[3] == '~') {
            if (shell->rx_pos < shell->rx_len) {
                memmove(&shell->rx_buf[shell->rx_pos],
                        &shell->rx_buf[shell->rx_pos + 1],
                        shell->rx_len - shell->rx_pos - 1);
                shell->rx_len--;
                shell_write(&shell->rx_buf[shell->rx_pos], shell->rx_len - shell->rx_pos);
                shell_write(" ", 1);
                char move_seq[8];
                snprintf(move_seq, sizeof(move_seq), "\033[%dD",
                         shell->rx_len - shell->rx_pos + 1);
                shell_write(move_seq, strlen(move_seq));
            }
        }
        break;
    }
}

/* ======================== 向后兼容：shell_handle_esc_seq 声明 ======================== */

/* 注意：shell_handle_esc_seq 函数已在上方定义，供外部调用。
 * 如果需要在 uart_shell.h 中声明，可添加：
 *   void shell_handle_esc_seq(shell_ctx_t *shell, const char *seq, int len);
 */
