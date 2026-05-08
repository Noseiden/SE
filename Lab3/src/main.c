#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"

// Pines motor
#define A_mas    23
#define A_menos  22
#define B_mas    21
#define B_menos  19

#define EN_A     18
#define EN_B      5

// Salidas
#define Cal 17
#define led 16

// UART
#define UART_PORT UART_NUM_0

// Tiempo
#define SAMPLE_PERIOD_US 100000
#define REPORT_PERIOD_US 1000000

// Variables
float T_medida = 0;
float L_medida = 0;

uint8_t T_deseada = 25;

int T_sen = 0;
int L_sen = 0;

int motor_speed = 0;
int direccion = 1;
int step_index = 0;

int duty_actual = -1;

// Secuencia half-step
int pasos[8][4] = {
    {1, 0, 1, 0},
    {1, 0, 0, 0},
    {1, 0, 0, 1},
    {0, 0, 0, 1},
    {0, 1, 0, 1},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0}
};

void step_motor(int dir) {
    gpio_set_level(EN_A, 1);
    gpio_set_level(EN_B, 1);

    if (dir == 1) {
        step_index = (step_index + 1) % 8;
    } else {
        step_index = (step_index - 1 + 8) % 8;
    }

    gpio_set_level(A_mas,   pasos[step_index][0]);
    gpio_set_level(A_menos, pasos[step_index][1]);
    gpio_set_level(B_mas,   pasos[step_index][2]);
    gpio_set_level(B_menos, pasos[step_index][3]);
}

void motor_off(void) {
    gpio_set_level(EN_A, 0);
    gpio_set_level(EN_B, 0);

    gpio_set_level(A_mas, 0);
    gpio_set_level(A_menos, 0);
    gpio_set_level(B_mas, 0);
    gpio_set_level(B_menos, 0);
}

void set_pwm(int porcentaje) {
    int duty = (porcentaje * 4095 / 100);

    if (duty != duty_actual) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        duty_actual = duty;
    }
}

void app_main(void) {

    // GPIO
    gpio_config_t salidas = {
        .pin_bit_mask = (
            (1ULL << A_mas) |
            (1ULL << A_menos) |
            (1ULL << B_mas) |
            (1ULL << B_menos) |
            (1ULL << EN_A) |
            (1ULL << EN_B) |
            (1ULL << Cal) |
            (1ULL << led)
        ),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&salidas);

    motor_off();
    gpio_set_level(Cal, 0);

    // UART
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    uart_param_config(UART_PORT, &uart_config);
    uart_driver_install(UART_PORT, 1024, 1024, 0, NULL, 0);

    char *inicio = "\nIngrese la temperatura deseada:\n";
    uart_write_bytes(UART_PORT, inicio, strlen(inicio));

    // ADC
    adc_oneshot_unit_handle_t adc_handle;

    adc_oneshot_unit_init_cfg_t adc_init = {
        .unit_id = ADC_UNIT_2,
    };

    adc_oneshot_new_unit(&adc_init, &adc_handle);

    adc_oneshot_chan_cfg_t adc_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_3, &adc_config);
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_2, &adc_config);

    // PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = led,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 4095,
        .hpoint = 0,
        .intr_type = LEDC_INTR_DISABLE,
    };

    ledc_channel_config(&ledc_channel);

    // UART buffer
    uint8_t data[32];
    char buffer[32];
    int index = 0;
    char msg[120];

    int64_t last_sample = 0;
    int64_t last_report = 0;
    int64_t last_step = 0;

    while (1) {

        int64_t now = esp_timer_get_time();

        // ADC
        if ((now - last_sample) >= SAMPLE_PERIOD_US) {
            last_sample = now;

            adc_oneshot_read(adc_handle, ADC_CHANNEL_3, &T_sen);
            adc_oneshot_read(adc_handle, ADC_CHANNEL_2, &L_sen);

            T_medida = T_sen / 9.5;

            float L_raw = (L_sen * 100.0) / 4095.0;
            L_medida = 100.0 - L_raw;

            if (L_medida < 0) {
                L_medida = 0;
            }

            if (L_medida > 100) {
                L_medida = 100;
            }
        }

        // UART
        int len = uart_read_bytes(UART_PORT, data, sizeof(data) - 1, pdMS_TO_TICKS(20));

        if (len > 0) {
            for (int i = 0; i < len; i++) {

                if (data[i] == '\n' || data[i] == '\r') {

                    if (index > 0) {
                        buffer[index] = '\0';

                        T_deseada = atoi(buffer);

                        sprintf(msg, "Td actualizada: %d\n", T_deseada);
                        uart_write_bytes(UART_PORT, msg, strlen(msg));

                        index = 0;
                    }

                } else {
                    if (index < sizeof(buffer) - 1) {
                        buffer[index] = data[i];
                        index++;
                    } else {
                        index = 0;
                    }
                }
            }
        }

        // Temperatura
        if (T_medida > T_deseada + 5) {
            motor_speed = 600;
            direccion = 0;
            gpio_set_level(Cal, 1);
        }
        else if (T_medida > T_deseada + 3) {
            motor_speed = 300;
            direccion = 0;
            gpio_set_level(Cal, 1);
        }
        else if (T_medida > T_deseada + 1) {
            motor_speed = 100;
            direccion = 0;
            gpio_set_level(Cal, 1);
        }
        else if (T_medida >= T_deseada - 1 && T_medida <= T_deseada + 1) {
            motor_speed = 0;
            gpio_set_level(Cal, 1);
        }
        else {
            motor_speed = 100;
            direccion = 1;
            gpio_set_level(Cal, 0);
        }

        // Luz
        if (L_medida < 20) {
            set_pwm(100);
        }
        else if (L_medida < 30) {
            set_pwm(80);
        }
        else if (L_medida < 40) {
            set_pwm(60);
        }
        else if (L_medida < 60) {
            set_pwm(50);
        }
        else if (L_medida < 80) {
            set_pwm(30);
        }
        else {
            set_pwm(0);
        }

        // Motor
        if (motor_speed > 0) {
            int64_t step_time = 1000000 / (motor_speed*2);

            if ((now - last_step) >= step_time) {
                last_step = now;
                step_motor(direccion);
            }
        } else {
            motor_off();
        }

        // Reporte
        if ((now - last_report) >= REPORT_PERIOD_US) {
            last_report = now;

            sprintf(
                msg,
                "Td:%d | T:%.2f | Luz:%.2f | Motor:%d steps/s\n",
                T_deseada,
                T_medida,
                L_medida,
                motor_speed
            );

            uart_write_bytes(UART_PORT, msg, strlen(msg));
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
