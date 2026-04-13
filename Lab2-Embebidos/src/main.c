#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define NUM_FILAS 5
#define NUM_COLS  8

#define PLAYER_COL 1

#define FIL7 27
#define FIL6 14
#define FIL5 13
#define FIL4 23
#define FIL3 22

#define COL1R 21
#define COL2R 19
#define COL3R 5
#define COL4R 16
#define COL5R 32
#define COL6R 33
#define COL7R 25
#define COL8R 26

#define COL2V 18

#define BTN_START 4
#define BTN_UP    2
#define BTN_DOWN  15

#define STEP_TIME_US       150000
#define PIPE_SPACING_MS    1500
#define PIPE_SPACING_TICKS ((PIPE_SPACING_MS + 199) / 200)

const int filas[NUM_FILAS] = {FIL7,FIL6,FIL5,FIL4,FIL3};
const int colsR[NUM_COLS]  = {COL1R,COL2R,COL3R,COL4R,COL5R,COL6R,COL7R,COL8R};

volatile uint8_t frameR[NUM_FILAS];
volatile uint8_t frameV[NUM_FILAS];

volatile int bird = 2;
volatile bool juego = false;

uint8_t obst[NUM_COLS];
volatile int spawn_cooldown = 0;

volatile bool ev_up=false, ev_down=false, ev_start=false;
volatile bool lock_up=false, lock_down=false, lock_start=false;

void IRAM_ATTR isr_up(void* arg){
    if(!lock_up){ ev_up=true; lock_up=true; gpio_intr_disable(BTN_UP); }
}
void IRAM_ATTR isr_down(void* arg){
    if(!lock_down){ ev_down=true; lock_down=true; gpio_intr_disable(BTN_DOWN); }
}
void IRAM_ATTR isr_start(void* arg){
    if(!lock_start){ ev_start=true; lock_start=true; gpio_intr_disable(BTN_START); }
}

void liberar(int pin, volatile bool* lock){
    if(*lock && gpio_get_level(pin)==1){
        *lock=false;
        gpio_intr_enable(pin);
    }
}

void clear_frame(){
    for(int i=0;i<NUM_FILAS;i++){
        frameR[i]=0;
        frameV[i]=0;
    }
}

void render(){
    clear_frame();

    frameV[bird] |= (1<<PLAYER_COL);

    for(int c=0;c<NUM_COLS;c++){
        if(obst[c] != 255){
            for(int r=0;r<NUM_FILAS;r++){
                if(r != obst[c]){
                    frameR[r] |= (1<<c);
                }
            }
        }
    }
}

volatile int fila_actual = 0;

void multiplex(){
    static int prev = -1;

    if(prev != -1){
        gpio_set_level(filas[prev],1);
    }

    for(int c=0;c<NUM_COLS;c++){
        gpio_set_level(colsR[c],1);
    }
    gpio_set_level(COL2V,1);

    uint8_t r = frameR[fila_actual];
    uint8_t v = frameV[fila_actual];

    for(int c=0;c<NUM_COLS;c++){
        if(r & (1<<c)){
            gpio_set_level(colsR[c],0);
        }
    }

    if(v & (1<<PLAYER_COL)){
        gpio_set_level(COL2V,0);
    }

    gpio_set_level(filas[fila_actual],0);

    prev = fila_actual;

    fila_actual++;
    if(fila_actual >= NUM_FILAS) fila_actual = 0;
}

void init_game(){
    bird = 2;
    for(int i=0;i<NUM_COLS;i++){
        obst[i] = 255;
    }
    spawn_cooldown = 0;
}

bool colision(){
    if(obst[PLAYER_COL] == 255) return false;
    return (obst[PLAYER_COL] != bird);
}

void tarea_juego(void* arg){
    int64_t last = esp_timer_get_time();

    while(1){

        liberar(BTN_UP,&lock_up);
        liberar(BTN_DOWN,&lock_down);
        liberar(BTN_START,&lock_start);

        if(ev_start){
            ev_start = false;
            juego = true;
            init_game();
        }

        if(ev_up && juego){
            ev_up = false;
            if(bird < NUM_FILAS-1) bird++;
        }

        if(ev_down && juego){
            ev_down = false;
            if(bird > 0) bird--;
        }

        if(juego){
            int64_t now = esp_timer_get_time();

            if(now - last > STEP_TIME_US){
                last = now;

                for(int i=0;i<NUM_COLS-1;i++){
                    obst[i] = obst[i+1];
                }

                if(spawn_cooldown <= 0){
                    obst[NUM_COLS-1] = rand() % NUM_FILAS;
                    spawn_cooldown = PIPE_SPACING_TICKS - 1;
                }else{
                    obst[NUM_COLS-1] = 255;
                    spawn_cooldown--;
                }

                if(colision()){
                    juego = false;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void tarea_display(void* arg){
    while(1){
        render();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void tarea_mux(void* arg){
    while(1){
        multiplex();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void app_main(){
    srand(esp_timer_get_time());

    for(int i=0;i<NUM_FILAS;i++){
        gpio_reset_pin(filas[i]);
        gpio_set_direction(filas[i],GPIO_MODE_OUTPUT);
        gpio_set_level(filas[i],1);
    }

    for(int i=0;i<NUM_COLS;i++){
        gpio_reset_pin(colsR[i]);
        gpio_set_direction(colsR[i],GPIO_MODE_OUTPUT);
        gpio_set_level(colsR[i],1);
    }

    gpio_reset_pin(COL2V);
    gpio_set_direction(COL2V,GPIO_MODE_OUTPUT);
    gpio_set_level(COL2V,1);

    int btns[3] = {BTN_UP,BTN_DOWN,BTN_START};

    for(int i=0;i<3;i++){
        gpio_reset_pin(btns[i]);
        gpio_set_direction(btns[i],GPIO_MODE_INPUT);
        gpio_set_pull_mode(btns[i],GPIO_PULLUP_ONLY);
        gpio_set_intr_type(btns[i],GPIO_INTR_NEGEDGE);
    }

    gpio_install_isr_service(0);

    gpio_isr_handler_add(BTN_UP,isr_up,NULL);
    gpio_isr_handler_add(BTN_DOWN,isr_down,NULL);
    gpio_isr_handler_add(BTN_START,isr_start,NULL);

    xTaskCreate(tarea_mux,"mux",2048,NULL,1,NULL);
    xTaskCreate(tarea_display,"disp",2048,NULL,2,NULL);
    xTaskCreate(tarea_juego,"game",4096,NULL,2,NULL);
}