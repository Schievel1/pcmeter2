#include <stdlib.h>
#include "ws2812.h"
#include "WS2812.pio.h"


uint32_t urgb_grbu32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

uint32_t urgb_rgbu32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (g) << 8) |
            ((uint32_t) (r) << 16) |
            (uint32_t) (b);
}

void set_led_grb(ws2812* led_strip, uint16_t idx, uint32_t pixel_grb) {
    if (idx < led_strip->length)
        led_strip->data[idx] = pixel_grb;
}

void fill_led_grb(ws2812* led_strip, uint32_t pixel_grb) {
    for (uint16_t i = 0; i < led_strip->length; i++)
        set_led_grb(led_strip, i, pixel_grb);
}

void ws2812_show(ws2812* led_strip) {
    for (uint16_t i = 0; i < led_strip->length; i++)
    {
        pio_sm_put_blocking(led_strip->pio, led_strip->sm, led_strip->data[i] << 8u);
    }
}

ws2812* ws2812_initialize(PIO pio, uint16_t sm, uint8_t pin, uint16_t length, bool is_rgbw) {
   ws2812* led_strip = malloc(sizeof(ws2812));
   led_strip->pio = pio;
   led_strip->sm = sm;
   led_strip->length = length;
   led_strip->data = malloc(length * sizeof(uint32_t));
   led_strip->offset = pio_add_program(led_strip->pio, &ws2812_program);
   ws2812_program_init(led_strip->pio, led_strip->sm, led_strip->offset, pin, 800000, is_rgbw? 32 : 24);
   return led_strip;
}

void ws2812_destroy(ws2812* led_strip) {
    pio_remove_program(led_strip->pio, &ws2812_program, led_strip->offset);
    free(led_strip->data);
    free(led_strip);
}
