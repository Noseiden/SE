#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "esp_timer.h"

// salidas

#define PIN_A 15
#define PIN_B 2
#define PIN_C 0
#define PIN_D 4
#define PIN_E 16
#define PIN_F 17
#define PIN_G 5

#define DIG1 18
#define DIG2 19

#define bajop 23
#define normalp 22
#define altop 21

// Entradas

#define pmas 32
#define pmenos 25
#define cal 26
#define rst 13

// aux

volatile uint8_t peso = 0;
volatile uint8_t dosis = 0;
volatile uint8_t decena = 0;
volatile uint8_t unidad = 0;
volatile uint8_t decenaini = 0;
volatile uint8_t unidadini = 0;
volatile bool calc = 0;
volatile bool led_b = 0;
volatile bool led_n = 0;
volatile bool led_a = 0;
volatile int64_t last_pmas_time = 0;
volatile int64_t last_pmenos_time = 0;
volatile int64_t last_cal_time = 0;
volatile int64_t last_rst_time = 0;
#define DEBOUNCE_US 200000

const uint8_t digitos[10] = {
    0b00111111,
    0b00000110,
    0b01011011,
    0b01001111,
    0b01100110,
    0b01101101,
    0b01111101,
    0b00000111,
    0b01111111,
    0b01101111
};

void apagar_todos_los_digitos(void) {
    gpio_set_level(DIG1, 1);
    gpio_set_level(DIG2, 1);
}

void mostrar_numero(int n){
    uint8_t patron = digitos[n];

    gpio_set_level(PIN_A, !((patron >> 0) & 1));
    gpio_set_level(PIN_B, !((patron >> 1) & 1));
    gpio_set_level(PIN_C, !((patron >> 2) & 1));
    gpio_set_level(PIN_D, !((patron >> 3) & 1));
    gpio_set_level(PIN_E, !((patron >> 4) & 1));
    gpio_set_level(PIN_F, !((patron >> 5) & 1));
    gpio_set_level(PIN_G, !((patron >> 6) & 1));
}

// Activa un solo dígito
void activar_digito(uint8_t dig) {
    apagar_todos_los_digitos();

    if (dig == 1) {
        gpio_set_level(DIG1, 0);
    }
    else if (dig == 2) {
        gpio_set_level(DIG2, 0);
    }
}

static void IRAM_ATTR pmas_isr(void *arg){

    int64_t now = esp_timer_get_time();

    if(now - last_pmas_time > DEBOUNCE_US){
        if (calc == 0){
            gpio_set_level(bajop, 0);
            gpio_set_level(normalp, 0);
            gpio_set_level(altop, 0);

            if (peso == 20)
            {
                peso = 20;
            }

            else{
                peso++;
            }

            decena = peso/10;
            unidad = peso%10;
            
            last_pmas_time = now;
        }
    }
}

static void IRAM_ATTR pmenos_isr(void *arg){

    int64_t now = esp_timer_get_time();

    if(now - last_pmenos_time > DEBOUNCE_US){

        if (calc == 0)
        {
            gpio_set_level(bajop, 0);
            gpio_set_level(normalp, 0);
            gpio_set_level(altop, 0);

            if (peso == 0)
            {
                peso = 0;
            }

            else{
                peso--;
            }

            decena = peso/10;
            unidad = peso%10;
            
            last_pmenos_time = now;
        }
    }
        
}

static void IRAM_ATTR cal_isr(void *arg){

    int64_t now = esp_timer_get_time();

    if(now - last_cal_time > DEBOUNCE_US){

        calc = 1;
        timer_start(TIMER_GROUP_0, TIMER_0);

        dosis = (3*peso + 5) - peso;

        decena = dosis / 10;
        unidad = dosis % 10;

        last_cal_time = now;

    }
        
}

static void IRAM_ATTR rst_isr(void *arg){

    int64_t now = esp_timer_get_time();

    if(now - last_cal_time > DEBOUNCE_US){

        calc = 0;
        gpio_set_level(bajop, 0);
        gpio_set_level(normalp, 0);
        gpio_set_level(altop, 0);
        timer_pause(TIMER_GROUP_0, TIMER_0);
        timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
        unidad = unidadini;
        decena = decenaini;
        peso = 0;

        last_cal_time = now;

    }
        
}

static bool IRAM_ATTR led_isr(void *arg){

    if (calc == 1){
        if (peso >= 0 && peso <= 7){
            led_b = !led_b;
            gpio_set_level(bajop, led_b);
        }

        else if (peso >= 8 && peso <= 14){
            led_n = !led_n;
            gpio_set_level(normalp, led_n);
        }

        else if (peso >= 15 && peso <= 20){
            led_a = !led_a;
            gpio_set_level(altop, led_a);
        }
    }
    
    return pdFALSE;
    
}

void app_main(void) {

    // Configuración de pines de segmentos como salida
    gpio_config_t seg_conf = {
        .pin_bit_mask = (1ULL << PIN_A) | (1ULL << PIN_B) | (1ULL << PIN_C) |
                        (1ULL << PIN_D) | (1ULL << PIN_E) | (1ULL << PIN_F) |
                        (1ULL << PIN_G) | (1ULL << bajop) | (1ULL << DIG1) | 
                        (1ULL << DIG2) | (1ULL << normalp) | (1ULL << altop),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&seg_conf);
    gpio_set_level(led_b, 0);
    gpio_set_level(led_n, 0);
    gpio_set_level(led_a, 0); 
    apagar_todos_los_digitos();

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << pmas) | (1ULL << pmenos) | (1ULL << cal) | (1ULL << rst),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE // flanco de bajada
    };
    gpio_config(&in_cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(pmas, pmas_isr, NULL);
    gpio_isr_handler_add(pmenos, pmenos_isr, NULL);
    gpio_isr_handler_add(cal, cal_isr, NULL);
    gpio_isr_handler_add(rst, rst_isr, NULL);

    //para led de 2Hz

    timer_config_t timer_led = {
        .divider = 80,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = true
    };

    timer_init(TIMER_GROUP_0, TIMER_0, &timer_led);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);

    timer_set_alarm_value(
        TIMER_GROUP_0, 
        TIMER_0, 
        500000
    );

    timer_isr_callback_add(
        TIMER_GROUP_0, 
        TIMER_0, 
        led_isr,
        NULL,
        0
    );
    
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    

    while (1) {
        
        apagar_todos_los_digitos();
        mostrar_numero(decena);
        activar_digito(1);
        vTaskDelay(pdMS_TO_TICKS(2));

        apagar_todos_los_digitos();
        mostrar_numero(unidad);
        activar_digito(2);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}