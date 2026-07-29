/* Semana 12 — libgpiod (API v1, Ubuntu/RPi OS Bookworm: libgpiod-dev)
 * gcc led_gpiod.c -lgpiod -o led && ./led  */
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>

#define CHIP "/dev/gpiochip0"
#define LINHA_LED 17

int main(void)
{
    struct gpiod_chip *chip = gpiod_chip_open(CHIP);
    if (!chip) { perror("chip"); return 1; }
    struct gpiod_line *led = gpiod_chip_get_line(chip, LINHA_LED);
    gpiod_line_request_output(led, "lab12", 0);

    for (int i = 0; i < 10; i++) {          // pisca 10x a 2 Hz
        gpiod_line_set_value(led, 1); usleep(250000);
        gpiod_line_set_value(led, 0); usleep(250000);
    }
    gpiod_line_release(led);
    gpiod_chip_close(chip);
    return 0;
}
