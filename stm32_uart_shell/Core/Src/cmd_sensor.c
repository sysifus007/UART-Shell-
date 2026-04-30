/**
 * @file    cmd_sensor.c
 * @brief   传感器数据读取命令实现
 * @details 提供模拟的温度、ADC、GPIO 等数据读取命令。
 *          实际项目中，替换底层读取函数即可对接真实传感器。
 *
 *          演示的传感器类型：
 *            - DS18B20 温度传感器（模拟）
 *            - ADC 通道读取（模拟）
 *            - GPIO 电平读取
 *            - DHT11 温湿度（模拟）
 */

#include "uart_shell.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ======================== 模拟传感器数据 ======================== */
/*
 * 在实际硬件中，这些函数会被替换为真实的传感器驱动调用。
 * 例如 DS18B20 的 one-wire 读取、ADC 的 HAL_ADC_Read 等。
 *
 * 这里使用模拟数据来展示完整的命令框架和输出格式。
 */

/** 模拟温度读取（返回摄氏度，精确到 0.1） */
static float simulate_read_temperature(void)
{
    /* 模拟 20~30 度范围的温度波动 */
    uint32_t tick = hal_get_tick();
    return 25.0f + 5.0f * ((float)(tick % 10000) / 10000.0f) - 2.5f;
}

/** 模拟 ADC 读取（返回 0~4095 的 12 位值） */
static uint16_t simulate_read_adc(int channel)
{
    uint32_t tick = hal_get_tick();
    /* 不同通道模拟不同基准值 */
    uint16_t bases[] = { 2048, 1024, 3072, 512, 3584 };
    if (channel < 0 || channel > 4) channel = 0;

    uint32_t noise = (tick * (channel + 1) * 7) % 100;
    int16_t result = (int16_t)(bases[channel] + noise - 50);
    if (result < 0) result = 0;
    if (result > 4095) result = 4095;
    return (uint16_t)result;
}

/** 模拟 DHT11 读取（温湿度） */
static void simulate_read_dht11(float *temp, float *humi)
{
    uint32_t tick = hal_get_tick();
    *temp = 22.0f + 3.0f * ((float)(tick % 20000) / 20000.0f);
    *humi = 55.0f + 10.0f * ((float)((tick + 5000) % 15000) / 15000.0f);
}

/* ======================== 命令实现 ======================== */

/**
 * @brief read_temp - 读取温度传感器
 *
 * 用法：
 *   read_temp              单次读取
 *   read_temp -c <count>   连续读取 count 次（间隔 500ms）
 *   read_temp -v           详细模式（显示原始数据）
 */
static int cmd_read_temp(int argc, char *argv[])
{
    int count = 1;
    int verbose = 0;

    /* 解析参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            count = atoi(argv[++i]);
            if (count < 1) count = 1;
            if (count > 100) count = 100;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        }
    }

    for (int i = 0; i < count; i++) {
        float temp = simulate_read_temperature();

        if (verbose) {
            shell_printf("[%d] Raw: 0x%04X  Temp: %.1f C (%.1f F)\r\n",
                         i + 1, (uint16_t)(temp * 10), temp, temp * 1.8f + 32.0f);
        } else {
            shell_printf("[%d] Temperature: " ANSI_YELLOW "%.1f C" ANSI_RESET
                         " (%.1f F)\r\n",
                         i + 1, temp, temp * 1.8f + 32.0f);
        }

        /* 连续读取时延时 */
        if (i < count - 1) {
            volatile uint32_t t = hal_get_tick();
            while (hal_get_tick() - t < 500);
        }
    }

    return 0;
}

/**
 * @brief read_adc - 读取 ADC 通道值
 *
 * 用法：
 *   read_adc                读取所有通道
 *   read_adc <channel>      读取指定通道 (0-4)
 *   read_adc <ch> -c <n>    连续读取 n 次
 */
static int cmd_read_adc(int argc, char *argv[])
{
    int channel = -1;  /* -1 表示读取所有通道 */
    int count = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            count = atoi(argv[++i]);
            if (count < 1) count = 1;
        } else {
            channel = atoi(argv[i]);
        }
    }

    int ch_start = (channel >= 0) ? channel : 0;
    int ch_end   = (channel >= 0) ? channel : 4;

    for (int i = 0; i < count; i++) {
        if (count > 1) {
            shell_printf("--- Round %d/%d ---\r\n", i + 1, count);
        }

        shell_printf(ANSI_BOLD "  CH   RAW      VOLTAGE   PERCENT" ANSI_RESET "\r\n");
        shell_printf("  --   ----     -------   -------\r\n");

        for (int ch = ch_start; ch <= ch_end; ch++) {
            uint16_t raw = simulate_read_adc(ch);
            float voltage = (float)raw * 3.3f / 4095.0f;
            float percent = (float)raw / 4095.0f * 100.0f;

            shell_printf("  %d    %4u     %5.3fV   %5.1f%%\r\n",
                         ch, raw, voltage, percent);
        }
    }

    return 0;
}

/**
 * @brief read_gpio - 读取 GPIO 引脚状态
 *
 * 用法：
 *   read_gpio <port> <pin>   例如：read_gpio A 5
 */
static int cmd_read_gpio(int argc, char *argv[])
{
    if (argc < 3) {
        shell_printf("Usage: read_gpio <port> <pin>\r\n");
        shell_printf("  port: A, B, C, ...\r\n");
        shell_printf("  pin:  0 - 15\r\n");
        shell_printf("Example: read_gpio A 5\r\n");
        return 1;
    }

    char port = toupper((unsigned char)argv[1][0]);
    int pin = atoi(argv[2]);

    if (pin < 0 || pin > 15) {
        shell_printf(ANSI_RED "Error:" ANSI_RESET " pin must be 0-15\r\n");
        return 1;
    }

    if (port < 'A' || port > 'G') {
        shell_printf(ANSI_RED "Error:" ANSI_RESET " port must be A-G\r\n");
        return 1;
    }

    /* 模拟：根据 tick 和引脚号生成伪随机电平 */
    uint32_t t = hal_get_tick() + pin * 1000;
    int level = (t >> 8) & 1;

    shell_printf("GPIO%c Pin %d: %s\r\n",
                 port, pin,
                 level ? ANSI_GREEN "HIGH (1)" ANSI_RESET
                       : ANSI_RED "LOW (0)" ANSI_RESET);

    return 0;
}

/**
 * @brief read_dht - 读取 DHT11/22 温湿度传感器
 */
static int cmd_read_dht(int argc, char *argv[])
{
    int count = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            count = atoi(argv[++i]);
            if (count < 1) count = 1;
            if (count > 50) count = 50;
        }
    }

    for (int i = 0; i < count; i++) {
        float temp, humi;
        simulate_read_dht11(&temp, &humi);

        shell_printf("[%d] " ANSI_YELLOW "Temp: %.1f C" ANSI_RESET
                     "  |  " ANSI_CYAN "Humidity: %.1f %%\r\n" ANSI_RESET,
                     i + 1, temp, humi);

        if (i < count - 1) {
            volatile uint32_t t = hal_get_tick();
            while (hal_get_tick() - t < 2000);  /* DHT11 最小间隔 2s */
        }
    }

    return 0;
}

/* ---- 命令注册表 ---- */
static const shell_cmd_t cmds[] = {
    { "read_temp",  "Read temperature sensor (DS18B20)",
      "read_temp [-c count] [-v]",             cmd_read_temp,  NULL },
    { "read_adc",   "Read ADC channel values",
      "read_adc [channel] [-c count]",         cmd_read_adc,   NULL },
    { "read_gpio",  "Read GPIO pin level",
      "read_gpio <port> <pin>",                cmd_read_gpio,  NULL },
    { "read_dht",   "Read DHT11 temperature & humidity",
      "read_dht [-c count]",                   cmd_read_dht,   NULL },
};

int cmd_sensor_register(void)
{
    extern shell_ctx_t *g_shell;
    for (int i = 0; i < (int)(sizeof(cmds) / sizeof(cmds[0])); i++) {
        shell_register(g_shell, &cmds[i]);
    }
    return 0;
}
