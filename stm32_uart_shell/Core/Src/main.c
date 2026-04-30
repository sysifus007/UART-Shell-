/**
 * @file    main.c
 * @brief   STM32 UART Shell 入口文件
 * @details 演示如何初始化和使用 UART Shell 框架。
 *
 *          功能概览：
 *          - 串口 115200-8-N-1 连接
 *          - Linux 风格命令行（Tab 补全、上下历史）
 *          - 模块化命令注册（led / read_temp / read_adc 等）
 *          - ANSI 颜色输出
 *
 * @note    使用步骤：
 *          1. 用 STM32CubeMX 创建 F103C8 工程，启用 USART1
 *          2. 将 Core/Src 和 Core/Inc 中的文件加入工程
 *          3. 在 stm32f1xx_it.c 中确保 USART1_IRQHandler 调用 HAL_UART_IRQHandler
 *          4. 编译烧录
 *          5. 用串口助手（如 MobaXterm、Tera Term）连接 115200 波特率
 */

/* ---- STM32 HAL 头文件 ---- */
#include "stm32f1xx_hal.h"

/* ---- 系统配置 ---- */
#include "main.h"

/* ---- Shell 框架 ---- */
#include "uart_shell.h"

/* ---- 命令模块 ---- */
#include "cmd_system.h"
#include "cmd_led.h"
#include "cmd_sensor.h"

/* ======================== 全局变量 ======================== */

/* UART 句柄（CubeMX 生成） */
UART_HandleTypeDef huart1;

/* Shell 上下文实例 */
shell_ctx_t g_shell_ctx;

/* ======================== 函数声明 ======================== */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

/* ======================== main ======================== */

int main(void)
{
    /* ---- HAL 初始化 ---- */
    HAL_Init();
    SystemClock_Config();

    /* ---- 外设初始化 ---- */
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    /* ---- Shell 初始化 ---- */
    shell_init(&g_shell_ctx);

    /* ---- 注册命令模块 ---- */
    cmd_system_register();   /* version, reboot, uptime, free, ps, date */
    cmd_led_register();      /* led on/off/toggle/status/blink */
    cmd_sensor_register();   /* read_temp, read_adc, read_gpio, read_dht */

    /* ---- UART 适配层初始化 ---- */
    hal_uart_init();

    /* ---- 欢迎信息 ---- */
    shell_printf("\r\n");
    shell_printf(ANSI_BOLD ANSI_CYAN);
    shell_printf("=================================\r\n");
    shell_printf("  STM32 UART Shell v1.0.0\r\n");
    shell_printf("  Type 'help' for commands\r\n");
    shell_printf("=================================\r\n");
    shell_printf(ANSI_RESET);
    shell_printf("\r\n");

    /* ---- 提示符 ---- */
    shell_print_prompt(&g_shell_ctx);

    /* ---- 主循环 ---- */
    while (1) {
        /* Shell 使用中断驱动模式（HAL_UART_RxCpltCallback + shell_feed），
         * 主循环空闲，可添加其他任务。
         *
         * 如果使用轮询模式，取消注释下方代码：
         *   if (hal_uart_readable()) {
         *       int ch = hal_uart_getc();
         *       if (ch >= 0) shell_feed(&g_shell_ctx, (char)ch);
         *   }
         */

        /* 低功耗模式示例（可选） */
        __WFI();  /* Wait For Interrupt */
    }
}

/* ======================== 时钟配置（CubeMX 生成，此处为参考）======================== */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* 配置 HSE 和 PLL */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;  /* 8MHz * 9 = 72MHz */

    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* 配置系统时钟 */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;   /* APB1 36MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;   /* APB2 72MHz */

    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

/* ======================== GPIO 初始化 ======================== */

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* PC13 - 板载 LED（推挽输出） */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);  /* 默认熄灭 */

    /* PA5 - 外接 LED（可选） */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

/* ======================== USART1 初始化 ======================== */

static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&huart1);

    /* 配置 NVIC 中断优先级 */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* ======================== 中断服务函数 ======================== */

/**
 * @brief USART1 全局中断服务函数
 * @note  放在 stm32f1xx_it.c 中，或在此处定义
 */
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

/* ======================== SysTick ======================== */

/**
 * @brief SysTick 中断回调（HAL 库使用）
 */
void HAL_IncTick(void)
{
    uwTick += uwTickFreq;
}
