#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/adc.h"

#include <math.h>
#include <stdlib.h>

uint const XPIN = 26;
uint const YPIN = 27;

QueueHandle_t xQueueAdc;

typedef struct adc {
    int axis;
    int val;
} adc_t;

#define MOVING_AVERAGE_SIZE 5



void x_task(void *p) {
    adc_t x_read;
    
    x_read.axis = 0; // define o canal 0 pro eixo x
    int x_buffer[MOVING_AVERAGE_SIZE] = {0};
    int x_index = 0;

    while (1) {
        adc_select_input(0);  // Seleciona o canal ADC para o eixo X (GP26 = ADC0)
        int current_read = adc_read(); // Realiza a leitura do ADC
        if ((current_read - 2047) / 8 > -170 && (current_read - 2047) / 8 < 170) { //zona morta
            x_buffer[x_index] = 0;
        } else {
            x_buffer[x_index] = (current_read - 2047) / 128;  // Normaliza o valor lido
        }

        // Atualiza a soma para calcular a média móvel
        int sum = 0;
        for (int i = 0; i < MOVING_AVERAGE_SIZE; i++) {
            sum += x_buffer[i];
        }
        x_read.val = sum / MOVING_AVERAGE_SIZE;

        xQueueSend(xQueueAdc, &x_read, portMAX_DELAY);  // Envia a leitura para a fila
        x_index = (x_index + 1) % MOVING_AVERAGE_SIZE;  // Atualiza o índice circularmente

        vTaskDelay(pdMS_TO_TICKS(100));  // Atraso para desacoplamento das tarefas
    }
}

void y_task(void *p) {
    adc_t y_read;
    
    y_read.axis = 1; // define o canal 1 pro eixo y

    
    int y_buffer[MOVING_AVERAGE_SIZE] = {0};
    
    int y_index = 0;

    while (1) {
        adc_select_input(1);  // Seleciona o canal ADC para o eixo Y (GP27 = ADC1)
        int current_read = adc_read(); // Realiza a leitura do ADC
        if ((current_read - 2047) / 8 > -170 && (current_read - 2047) / 8 < 170) { //zona morta
            y_buffer[y_index] = 0;
        } else {
            y_buffer[y_index] = (current_read - 2047) / 128;  // Normaliza o valor lido
        }

        // Atualiza a soma para calcular a média móvel
        int sum = 0;
        for (int i = 0; i < MOVING_AVERAGE_SIZE; i++) {
            sum += y_buffer[i];
        }
        y_read.val = sum / MOVING_AVERAGE_SIZE;

        xQueueSend(xQueueAdc, &y_read, portMAX_DELAY);  // Envia a leitura para a fila
        y_index = (y_index + 1) % MOVING_AVERAGE_SIZE;  // Atualiza o índice circularmente

        vTaskDelay(pdMS_TO_TICKS(100));  // Atraso para desacoplamento das tarefas
    }
}

void write(adc_t data){
    int val = data.val;
    int msb = val >> 8;
    int lsb = val & 0xFF ;

    uart_putc_raw(uart0, data.axis);
    uart_putc_raw(uart0, lsb);
    uart_putc_raw(uart0, msb);
    uart_putc_raw(uart0, -1); 
}

void uart_task(void *p) {
    adc_t data;

    while (1) {
        if (xQueueReceive(xQueueAdc, &data, portMAX_DELAY)) {
            write(data);
        }
    }
}

int main() {
    stdio_init_all();
    adc_init();

    adc_gpio_init(XPIN);
    adc_gpio_init(YPIN);

    xQueueAdc = xQueueCreate(32, sizeof(adc_t));

    xTaskCreate(uart_task, "uart_task", 4096, NULL, 1, NULL);
    xTaskCreate(x_task, "x_task", 4096, NULL, 1, NULL);
    xTaskCreate(y_task, "y_task", 4096, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true);
}