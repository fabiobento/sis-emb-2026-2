#include <stdio.h>                 // printf para o monitor serial
#include "freertos/FreeRTOS.h"     // o RTOS que vive dentro do ESP-IDF (semana 5)
#include "freertos/task.h"         // vTaskDelay e criação de tarefas
#include "driver/gpio.h"           // driver de GPIO do ESP-IDF (semana 3)

#define PINO_LED GPIO_NUM_2        // GPIO 2: LED azul embutido na maioria dos DevKits

void app_main(void)                // ponto de entrada do ESP-IDF (não é main()!)
{
    gpio_reset_pin(PINO_LED);                          // devolve o pino ao estado padrão
    gpio_set_direction(PINO_LED, GPIO_MODE_OUTPUT);    // configura como SAÍDA

    int nivel = 0;
    while (1) {                                        // firmware nunca "termina"
        nivel = !nivel;                                // alterna 0 ↔ 1
        gpio_set_level(PINO_LED, nivel);               // escreve no pino
        printf("LED = %d\n", nivel);                   // log no monitor serial
        vTaskDelay(pdMS_TO_TICKS(500));                // dorme 500 ms SEM gastar CPU
    }
}