#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_reg.h"      
#include "driver/gpio.h"

#define BIT_LED (1u << 2)      // GPIO 2 → bit 2 dos registradores de GPIO 0–31

void app_main(void)
{
    gpio_reset_pin(GPIO_NUM_2);
    
    // Substitui a direção via driver pelo acesso direto ao registrador ENABLE
    *(volatile uint32_t *)GPIO_ENABLE_REG |= BIT_LED;

    // Aponta para o registrador de saída geral (onde moram todos os 32 pinos)
    volatile uint32_t *out = (volatile uint32_t *)GPIO_OUT_REG; 

    while (1) {
        *out |= BIT_LED;                    // liga: lê, modifica e escreve (NÃO atômico!)
        vTaskDelay(pdMS_TO_TICKS(250));
        
        *out &= ~BIT_LED;                   // desliga: lê, modifica e escreve (NÃO atômico!)
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}