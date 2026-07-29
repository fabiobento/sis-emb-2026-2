// Semana 2 — piscando LED por ACESSO DIRETO A REGISTRADOR (didático!)
// Em produção use o driver gpio; aqui o objetivo é enxergar o hardware nu.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_reg.h"     // define GPIO_OUT_REG, GPIO_ENABLE_REG
#include <stdint.h>

#define LED_BIT (1u << 2)     // GPIO 2

void app_main(void)
{
    // habilita GPIO2 como saída: bit 2 do registrador ENABLE
    *(volatile uint32_t *)GPIO_ENABLE_REG |= LED_BIT;
    while (1) {
        *(volatile uint32_t *)GPIO_OUT_REG |=  LED_BIT;  // liga
        vTaskDelay(pdMS_TO_TICKS(500));
        *(volatile uint32_t *)GPIO_OUT_REG &= ~LED_BIT;  // desliga
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
