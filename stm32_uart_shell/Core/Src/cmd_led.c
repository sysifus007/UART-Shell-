/**
 * @file    cmd_led.c
 * @brief   LED 控制命令实现
 * @details 提供对板载 LED 的开关、翻转、状态查询、闪烁等控制。
 *          演示了 Shell 命令如何与硬件外设交互。
 *
 *          硬件映射（以 STM32F103C8T6 "Blue Pill" 为例）：
 *            LED1 = PC13（板载蓝色 LED，低电平点亮）
 *            LED2 = PA5  （可选外接 LED，高电平点亮）
 *
 *          使用时需根据实际硬件修改 LED_PORT/LED_PIN 宏。
 */

#include "uart_shell.h"
#include <string.h>
#include <stdio.h>

/* ======================== 硬件抽象（模拟层） ======================== */
/*
 * 说明：在实际 STM32 项目中，LED 的 GPIO 操作通过 HAL 库完成。
 * 本文件中的 LED 操作函数提供了两种模式：
 *   1. 真实硬件模式：通过 HAL_GPIO_WritePin 操作
 *   2. 模拟模式：使用软件变量模拟（用于无硬件时的测试）
 *
 * 宏 LED_SIMULATE 用于选择模式：
 *   - 定义为 1：使用模拟模式（默认，方便测试）
 *   - 定义为 0：使用真实 HAL GPIO 操作
 */

#ifndef LED_SIMULATE
#define LED_SIMULATE 1
#endif

/* ---- LED 定义 ---- */
#define LED_MAX_COUNT  2

typedef struct {
    uint8_t  id;         /* LED 编号 (0, 1, ...) */
    char     name[8];    /* LED 名称 */
    uint8_t  active_low; /* 低电平点亮 */
    uint8_t  state;      /* 当前状态：0=灭, 1=亮 */
} led_dev_t;

static led_dev_t led_table[LED_MAX_COUNT] = {
    { 0, "LED1", 1, 0 },  /* PC13, 板载蓝灯, 低电平亮 */
    { 1, "LED2", 0, 0 },  /* PA5,  外接LED, 高电平亮 */
};

/* ---- 模拟 LED 操作 ---- */
#if LED_SIMULATE
static void led_gpio_write(led_dev_t *led, uint8_t state)
{
    /* 模拟模式：仅更新软件状态 */
    led->state = state ? 1 : 0;
}
#else
#include "stm32f1xx_hal.h"

/* 实际硬件的 GPIO 端口/引脚映射 */
static const struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} led_gpio_map[LED_MAX_COUNT] = {
    { GPIOC, GPIO_PIN_13 },  /* LED1 -> PC13 */
    { GPIOA, GPIO_PIN_5 },   /* LED2 -> PA5  */
};

static void led_gpio_write(led_dev_t *led, uint8_t state)
{
    int idx = led->id;
    GPIO_PinState pin_state;

    if (led->active_low) {
        pin_state = state ? GPIO_PIN_RESET : GPIO_PIN_SET;
    } else {
        pin_state = state ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }

    HAL_GPIO_WritePin(led_gpio_map[idx].port, led_gpio_map[idx].pin, pin_state);
    led->state = state ? 1 : 0;
}
#endif /* LED_SIMULATE */

/* ======================== 命令实现 ======================== */

/**
 * @brief led - LED 控制主命令
 *
 * 用法：
 *   led on [id]      点亮 LED (默认 LED1)
 *   led off [id]     熄灭 LED
 *   led toggle [id]  翻转 LED 状态
 *   led status [id]  查看 LED 状态
 *   led blink <id> <count> <interval_ms>  LED 闪烁
 */
static int cmd_led(int argc, char *argv[])
{
    if (argc < 2) {
        shell_printf("Usage: led <on|off|toggle|status|blink> [id] [args...]\r\n");
        return 1;
    }

    int led_id = 0;  /* 默认 LED1 */

    /* ---- status ---- */
    if (strcmp(argv[1], "status") == 0) {
        shell_printf(ANSI_BOLD "  ID   NAME   STATE" ANSI_RESET "\r\n");
        shell_printf("  --   ----   -----\r\n");
        for (int i = 0; i < LED_MAX_COUNT; i++) {
            shell_printf("  %d    %-5s  %s\r\n",
                         led_table[i].id,
                         led_table[i].name,
                         led_table[i].state ? ANSI_GREEN "ON " ANSI_RESET
                                            : ANSI_RED "OFF" ANSI_RESET);
        }
        return 0;
    }

    /* ---- on / off / toggle ---- */
    uint8_t new_state = 0;

    if (strcmp(argv[1], "on") == 0) {
        new_state = 1;
    } else if (strcmp(argv[1], "off") == 0) {
        new_state = 0;
    } else if (strcmp(argv[1], "toggle") == 0) {
        /* do nothing, handled below */
    } else if (strcmp(argv[1], "blink") == 0) {
        /* blink [id] [count] [interval_ms] */
        if (argc < 5) {
            shell_printf("Usage: led blink <id> <count> <interval_ms>\r\n");
            return 1;
        }
        led_id = atoi(argv[2]);
        int count = atoi(argv[3]);
        int interval = atoi(argv[4]);

        if (led_id < 0 || led_id >= LED_MAX_COUNT) {
            shell_printf(ANSI_RED "Error:" ANSI_RESET " invalid LED id %d (0-%d)\r\n",
                         led_id, LED_MAX_COUNT - 1);
            return 1;
        }

        led_dev_t *led = &led_table[led_id];
        shell_printf("Blinking %s %d times, interval=%dms...\r\n",
                     led->name, count, interval);

        for (int i = 0; i < count; i++) {
            led_gpio_write(led, 1);
            shell_printf("  [%d] " ANSI_GREEN "ON" ANSI_RESET "\r\n", i + 1);
            /* 实际项目中使用 HAL_Delay() 或 RTOS 延时 */
            {
                volatile uint32_t t = hal_get_tick();
                while (hal_get_tick() - t < (uint32_t)interval);
            }

            led_gpio_write(led, 0);
            shell_printf("  [%d] " ANSI_RED "OFF" ANSI_RESET "\r\n", i + 1);
            {
                volatile uint32_t t = hal_get_tick();
                while (hal_get_tick() - t < (uint32_t)interval);
            }
        }
        return 0;
    } else {
        shell_printf(ANSI_RED "Error:" ANSI_RESET " unknown action '%s'\r\n", argv[1]);
        shell_printf("Valid actions: on, off, toggle, status, blink\r\n");
        return 1;
    }

    /* 解析可选的 LED id 参数 */
    if (argc >= 3) {
        led_id = atoi(argv[2]);
    }
    if (led_id < 0 || led_id >= LED_MAX_COUNT) {
        shell_printf(ANSI_RED "Error:" ANSI_RESET " invalid LED id %d (0-%d)\r\n",
                     led_id, LED_MAX_COUNT - 1);
        return 1;
    }

    led_dev_t *led = &led_table[led_id];

    if (strcmp(argv[1], "toggle") == 0) {
        new_state = led->state ? 0 : 1;
    }

    led_gpio_write(led, new_state);

    shell_printf("%s -> %s\r\n", led->name,
                 new_state ? ANSI_GREEN "ON" ANSI_RESET : ANSI_RED "OFF" ANSI_RESET);

    return 0;
}

/* ---- 命令注册 ---- */
static const shell_cmd_t led_cmd = {
    .name     = "led",
    .help     = "Control on-board LEDs",
    .usage    = "led <on|off|toggle|status|blink> [id] [count] [interval_ms]",
    .handler  = cmd_led,
    .completer = NULL,
};

int cmd_led_register(void)
{
    extern shell_ctx_t *g_shell;
    return shell_register(g_shell, &led_cmd);
}
