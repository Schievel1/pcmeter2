#include <stdlib.h>
#ifdef DEBUG
 #include <stdio.h>
#endif
#include "WS2812.pio.h"
#include "ws2812.h"

struct WS2812 {
    PIO pio;
    int16_t sm;
    uint8_t pin;
    bool is_rgbw;
    uint16_t offset;
    uint16_t length;
    uint32_t* data;
};

uint32_t ws2812_urgb_grbu32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (g) << 24) |
            ((uint32_t) (r) << 16) |
            ((uint32_t) (b) << 8);
}

uint32_t ws2812_urgb_rgbu32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 24) |
            ((uint32_t) (g) << 16) |
            ((uint32_t) (b) << 8);
}

uint32_t ws2812_uwrgb_wrgbu32(uint8_t w, uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (w) << 24) |
            ((uint32_t) (r) << 16) |
            ((uint32_t) (g) << 8) |
            (uint32_t) (b);
}

int8_t ws2812_set_led(struct WS2812* led_strip, uint16_t idx, uint32_t pixel_data) {
    if (!led_strip) {
#ifdef DEBUG
        printf("set_led: led_strip is NULL\n");
#endif
        return -1;
    }
    if (idx > led_strip->length) {
#ifdef DEBUG
        printf("set_led: index %d geater than led_strip's length %d\n", idx, led_strip->length);
#endif
        return -1;
    }
    led_strip->data[idx] = pixel_data;
    return led_strip->data[idx];
}

int8_t ws2812_fill(struct WS2812* led_strip, uint32_t pixel_data) {
    if (!led_strip) {
#ifdef DEBUG
        printf("fill_led: led_strip is NULL\n");
#endif
        return -1;
    }
    for (uint16_t i = 0; i < led_strip->length; i++)
        ws2812_set_led(led_strip, i, pixel_data);
    return 0;
}

int8_t ws2812_show(struct WS2812* led_strip) {
    if (!led_strip) {
#ifdef DEBUG
        printf("ws2812_show: led_strip is NULL\n");
#endif
        return -1;
    }
    for (uint16_t i = 0; i < led_strip->length; i++)
    {
#ifdef DEBUG
        printf("putting LED %d data: %ld\n", i, led_strip->data[i]);
#endif
        pio_sm_put_blocking(led_strip->pio, led_strip->sm, led_strip->data[i]);
    }
    return 1;
}

uint16_t ws2812_get_length(struct WS2812* led_strip) {
    if (!led_strip) {
#ifdef DEBUG
        printf("ws2812_get_length: led_strip is NULL\n");
#endif
        return -1;
    }
    return led_strip->length;
}

uint8_t ws2812_get_pin(struct WS2812* led_strip) {
    if (!led_strip) {
#ifdef DEBUG
        printf("ws2812_get_pin: led_strip is NULL\n");
#endif
        return -1;
    }
    return led_strip->pin;
}

uint16_t ws2812_get_sm(struct WS2812* led_strip) {
    if (!led_strip) {
#ifdef DEBUG
        printf("ws2812_get_sm: led_strip is NULL\n");
#endif
        return -1;
    }
    return led_strip->sm;
}

PIO ws2812_get_pio(struct WS2812* led_strip) {
    if (!led_strip) {
#ifdef DEBUG
        printf("ws2812_get_pio: led_strip is NULL\n");
#endif
        return (PIO)-1;
    }
    return led_strip->pio;
}

uint32_t ws2812_get_pixel_data(struct WS2812* led_strip, uint16_t idx) {
    if (!led_strip) {
#ifdef DEBUG
        printf("ws2812_get_pixel_data: led_strip is NULL\n");
#endif
        return -1;
    }
    if (idx > led_strip->length) {
#ifdef DEBUG
        printf("ws2812_get_pixel_data: index %d geater than led_strip's length %d\n", idx, led_strip->length);
#endif
        return -1;
    }
    return led_strip->data[idx];
}

struct WS2812* ws2812_initialize(PIO pio, uint16_t sm, uint8_t pin, uint16_t length, bool is_rgbw) {
   struct WS2812* led_strip = malloc(sizeof(struct WS2812));
   if (!led_strip)
       return NULL;
   led_strip->pio = pio;
   led_strip->sm = sm;
   led_strip->pin = pin;
   led_strip->length = length;
   led_strip->is_rgbw = is_rgbw;
   led_strip->data = malloc(length * sizeof(uint32_t));
   led_strip->offset = pio_add_program(led_strip->pio, &ws2812_program);
   ws2812_program_init(led_strip->pio, led_strip->sm, led_strip->offset, led_strip->pin, 800000, led_strip->is_rgbw? 32 : 24);
   return led_strip;
}

int8_t ws2812_destroy(struct WS2812* led_strip) {
    if (!led_strip) {
#ifdef DEBUG
        printf("ws2812_destroy: led_strip is NULL\n");
#endif
        return -1;
    }
    pio_remove_program(led_strip->pio, &ws2812_program, led_strip->offset);
    free(led_strip->data);
    free(led_strip);
    return 0;
}
