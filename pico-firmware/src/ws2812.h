#ifndef WS2812_H_
#define WS2812_H_


#include "WS2812.pio.h"
typedef struct _ws2812 {
    PIO pio;
    int sm;
    uint16_t offset;
    int length;
    uint32_t* data;
} ws2812;

void set_led_grb(ws2812* led_strip, uint16_t idx, uint32_t pixel_grb);
void fill_led_grb(ws2812* led_strip, uint32_t pixel_grb);
uint32_t urgb_grbu32(uint8_t r, uint8_t g, uint8_t b);
uint32_t urgb_rgbu32(uint8_t r, uint8_t g, uint8_t b);
void ws2812_show(ws2812* led_strip);
ws2812* ws2812_initialize(PIO pio, uint16_t sm, uint8_t pin, uint16_t length, bool is_rgbw);
void ws2812_destroy(ws2812* led_strip);

#endif // WS2812_H_
