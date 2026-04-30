/**
 * @file    hal_uart.c
 * @brief   STM32 HAL UART 适配层实现
 * @details 提供 UART 初始化、发送、接收的 HAL 封装。
 *          支持两种工作模式：
 *            1. 轮询模式 (POLLING)：简单，适合入门
 *            2. DMA 中断模式 (DMA_IT)：高效，推荐生产使用
 *
 *          本文件默认使用轮询模式，DMA 模式的代码已注释保留。
 *
 * @note    使用前请确保：
 *          1. STM32CubeMX 已配置 USART1 (115200-8-N-1)
 *          2. 已使能对应 GPIO (PA9-TX, PA10-RX)
 *          3. 如使用 DMA，需在 CubeMX 中启用 USART1_RX DMA
 */

#include "uart_shell.h"
#include "stm32f1xx_hal.h"

/* ======================== 外设句柄 ======================== */

extern UART_HandleTypeDef huart1;  /* CubeMX 生成的 UART 句柄 */

/* ======================== 轮询模式 ======================== */

int hal_uart_init(void)
{
    /* UART 已在 CubeMX 生成的 MX_USART1_UART_Init() 中完成初始化
     * 这里只需确保 UART 已就绪 */
    if (HAL_UART_GetState(&huart1) != HAL_UART_STATE_READY) {
        return -1;
    }

    /* 开启接收中断（用于 shell_feed 实时处理） */
    uint8_t dummy;
    HAL_UART_Receive_IT(&huart1, &dummy, 1);

    return 0;
}

void hal_uart_putc(char ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
}

void hal_uart_write(const uint8_t *data, size_t len)
{
    if (data && len > 0) {
        HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);
    }
}

int hal_uart_readable(void)
{
    /* 轮询模式：不推荐使用，应使用中断/DMA */
    return 0;
}

int hal_uart_getc(void)
{
    uint8_t ch;
    if (HAL_UART_Receive(&huart1, &ch, 1, 0) == HAL_OK) {
        return ch;
    }
    return -1;
}

uint32_t hal_get_tick(void)
{
    return HAL_GetTick();
}

void hal_system_reset(void)
{
    HAL_Delay(100);
    NVIC_SystemReset();
}

/* ======================== UART 接收中断回调 ======================== */

/**
 * @brief UART 接收完成回调
 * @note  在中断上下文中调用，尽量简短
 *
 * 每收到一个字节就调用 shell_feed()，实现实时命令行编辑。
 * 对于 ESC 序列（方向键等），需要额外积累字节后处理。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    extern shell_ctx_t g_shell_ctx;

    if (huart->Instance == USART1) {
        /* 读取接收到的字节 */
        uint8_t ch;

        /* 从 DR 寄存器读取数据 */
        ch = (uint8_t)(huart1.Instance->DR & 0xFF);

        /* 送入 Shell 处理 */
        shell_feed(&g_shell_ctx, (char)ch);

        /* 重新开启接收中断 */
        HAL_UART_Receive_IT(&huart1, &ch, 1);
    }
}

/* ======================== DMA 模式（可选，需 CubeMX 配置 DMA）======================== */
/*
 * 如果使用 DMA + IDLE 中断接收（推荐），取消以下注释并注释掉上面的轮询接收。
 *
 * #define USE_UART_DMA 1
 *
 * #if USE_UART_DMA
 *
 * #define UART_RX_BUF_SIZE  256
 * static uint8_t uart_rx_dma_buf[UART_RX_BUF_SIZE];
 *
 * // 在 hal_uart_init() 中添加：
 * //   __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
 * //   HAL_UART_Receive_DMA(&huart1, uart_rx_dma_buf, UART_RX_BUF_SIZE);
 *
 * // 在 HAL_UART_IdleCpltCallback 或自定义 IDLE ISR 中：
 * void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
 * {
 *     if (huart->Instance != USART1) return;
 *
 *     static uint16_t last_pos = 0;
 *     uint16_t count;
 *
 *     if (size >= last_pos) {
 *         count = size - last_pos;
 *     } else {
 *         count = UART_RX_BUF_SIZE - last_pos + size;
 *     }
 *
 *     for (uint16_t i = 0; i < count; i++) {
 *         uint16_t idx = (last_pos + i) % UART_RX_BUF_SIZE;
 *         shell_feed(&g_shell_ctx, (char)uart_rx_dma_buf[idx]);
 *     }
 *
 *     last_pos = size;
 *     HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buf, UART_RX_BUF_SIZE);
 *     __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
 * }
 * #endif
 */
