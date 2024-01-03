#ifndef WS2812_H_
#define WS2812_H_

struct WS2812;

/*
 * Use these function to create a pixel data.
 * Select the function according to the type of LED strip.
 */
 /* grb strips */
uint32_t ws2812_urgb_grbu32(uint8_t r, uint8_t g, uint8_t b);
/* rgb strips */
uint32_t ws2812_urgb_rgbu32(uint8_t r, uint8_t g, uint8_t b);
/* wrgb strips */
uint32_t ws2812_uwrgb_wrgbu32(uint8_t w, uint8_t r, uint8_t g, uint8_t b);
/*
 *  set the color of a single LED
 *  Example: ws2812_led(led_strip, 42, ws2812_urgb_grbu32(255, 0, 0));
 */
int8_t ws2812_set_led(struct WS2812 *led_strip, uint16_t idx, uint32_t pixel_data);
/*  set the color of all LEDs of a strip at once */
int8_t ws2812_fill(struct WS2812 *led_strip, uint32_t pixel_data);

/* write the data to the LED strip */
int8_t ws2812_show(struct WS2812 *led_strip);

/* create a new LED strip object */
struct WS2812* ws2812_initialize(PIO pio, uint16_t sm, uint8_t pin, uint16_t length, bool is_rgbw);
/* destroy the LED strip object */
int8_t ws2812_destroy(struct WS2812 *led_strip);

/* getters */
uint16_t ws2812_get_length(struct WS2812 *led_strip);
uint8_t ws2812_get_pin(struct WS2812 *led_strip);
uint16_t ws2812_get_sm(struct WS2812 *led_strip);
PIO ws2812_get_pio(struct WS2812 *led_strip);
uint32_t ws2812_get_pixel_data(struct WS2812 *led_strip, uint16_t idx);

#endif // WS2812_H_
