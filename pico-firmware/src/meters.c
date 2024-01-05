/*
    PC Meter 2

    Drives PC Meter device.
    http://www.swvincent.com/pcmeter

    Written in 2018 by Scott W. Vincent
    http://www.swvincent.com
    Email: my first name at swvincent.com

    This is an update to my original PC Meter program from 2013. This new version provides smoother movement
    of the CPU meter, overhauls the serial communcation and has better variable names than before. I've also
    removed use of the delay function and other improvements. I'm sure there's still room for more :-)

    Developed and Tested on an Arduino Leonardo with IDE 1.8.5

    Updated and ported to USB and Raspberry Pi Pico by Pascal Jaeger
    http://www.leimstift.de
    Email: pascal.jaeger at leimstift.de

    Serial communcation code from/based on Robin2's tutorial at:
    http://forum.arduino.cc/index.php?topic=396450.0

    Thanks to Hayden Thring for his analog PC meter project inspired by my original Arduino program,
    which has in turn inspired this one. Some ideas on how to further smooth the meter movement
    came from studying and using his code. https://hackaday.io/project/10629-pc-analog-panel-meters-w-arduino

    To the extent possible under law, the author has dedicated all copyright and related and neighboring rights to this
    software to the public domain worldwide. This software is distributed without any warranty.

    You should have received a copy of the CC0 Public Domain Dedication along with this software.
    If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include <stdlib.h>
#include "pico/stdlib.h"
#include "meters.h"
#include "hardware/pwm.h"
#include "bsp/board.h"
#include "WS2812.pio.h"
#include "ws2812.h"

//Constants
const int METER_PINS[NUMBER_OF_METERS] = {3, 4};     // Meter output pins
// Set this value to correct cheap meters that display wrong
// also if you have 3V meters tune this down so it shows 3V at 100% CPU load
const int METER_MAX[NUMBER_OF_METERS] = {228, 228};    // Max value for meters
const int METER_UPDATE_FREQ = 100;      // Frequency of meter updates in milliseconds
const long SERIAL_TIMEOUT = 2000;       // How long to wait until serial "times out"
#define READINGS_COUNT 20          // Number of readings to average for each meter

//Variables
#define numRecChars  32            // Sets size of receive buffer
char receivedChars[numRecChars];        // Array for received serial data
bool newData = false;                   // Indicates if new data has been received
unsigned long lastSerialRecd = 0;       // Time last serial recd
unsigned long lastMeterUpdate = 0;      // Time meters last updated
int lastValueReceived[NUMBER_OF_METERS] = {0};      // Last value received
int valuesRecd[NUMBER_OF_METERS][READINGS_COUNT];      // Readings to be averaged
int runningTotal[NUMBER_OF_METERS] = {0};           // Running totals
int valuesRecdIndex = 0;                // Index of current reading

// Values for WS2812 LED strip
#define WS2812_PIN 2
#define WS2812_IS_RGBW false
const int WS2812_LEN = 4 * NUMBER_OF_METERS;
struct WS2812* led_strip;

// Arduino map function
long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

//Set Meter position
static void setMeter(int meterPin, int perc, int meterMax) {
  //Map perc to proper meter position
  int pos = map(perc, 0, 100, 0, meterMax);
  pwm_set_gpio_level(meterPin, pos);
}

static void map_percent_green_to_red (uint8_t percent, uint8_t *r, uint8_t *g, uint8_t *b) {
  long byte_percent = map(percent, 0, 100, 0, 255);
  *r = byte_percent;
  *g = 255 - byte_percent;
  *b = 0;
}

static void setLEDStrip(uint8_t meteridx, uint8_t percent) {
    uint8_t r,g,b, i0, i1, i2, i3;
    map_percent_green_to_red(percent, &r,&g,&b);

    i0 = meteridx * 4;
    i1 = meteridx * 4 + 1;
    i2 = meteridx * 4 + 2;
    i3 = meteridx * 4 + 3;
    ws2812_set_led(led_strip, i0, ws2812_urgb_grbu32(r, g, b));
    ws2812_set_led(led_strip, i1, ws2812_urgb_grbu32(r, g, b));
    ws2812_set_led(led_strip, i2, ws2812_urgb_grbu32(r, g, b));
    ws2812_set_led(led_strip, i3, ws2812_urgb_grbu32(r, g, b));
}

//Max both meters on startup as a test
static void meterStartup(void) {
  ws2812_fill(led_strip, ws2812_urgb_grbu32(0, 0, 0));
  ws2812_show(led_strip);
  for (int i = 0; i<100; i++) {
    for (uint8_t j = 0; j < NUMBER_OF_METERS; j++)
      setMeter(METER_PINS[j], i, METER_MAX[j]);
    sleep_ms(5);
  }
  for (int i = 100; i>0; i--) {
    for (uint8_t j = 0; j < NUMBER_OF_METERS; j++)
      setMeter(METER_PINS[j], i, METER_MAX[j]);
    sleep_ms(5);
  }
}

void meters_setup(void) {
    uint* slice_num = malloc(NUMBER_OF_METERS * sizeof(uint));
    for (uint8_t j = 0; j < NUMBER_OF_METERS; j++) {
      gpio_set_function(METER_PINS[j], GPIO_FUNC_PWM);
      gpio_set_function(METER_PINS[j], GPIO_FUNC_PWM);
      slice_num[j] = pwm_gpio_to_slice_num(METER_PINS[j]),
      pwm_set_wrap(slice_num[j], 254);
      pwm_set_enabled(slice_num[j], true);
      pwm_set_enabled(slice_num[j], true);

      //Init values Received array
      for (int counter = 0; counter < READINGS_COUNT; counter++)
        valuesRecd[j][counter] = 0;
    }

    led_strip = ws2812_initialize(pio0, 0, WS2812_PIN, WS2812_LEN, WS2812_IS_RGBW);

    meterStartup();

    //Get times started
    lastMeterUpdate = board_millis();
    lastSerialRecd = board_millis();
}

#define ENDSTDIN 255
void meters_receiveSerialData(void) {
    // This is the recvWithEndMarker() function
    // from Robin2's serial data tutorial
    static uint8_t ndx = 0;
    char endMarker = '\r';
    char rc;

    while ((rc = getchar_timeout_us(0)) != ENDSTDIN && newData == false) {
      if (rc != endMarker) {
          receivedChars[ndx] = rc;
          ndx++;
          if (ndx >= numRecChars) {
            ndx = numRecChars - 1;
          }
      } else {
          receivedChars[ndx] = '\0'; // terminate the string
          ndx = 0;
          newData = true;
#ifdef DEBUG
          printf("ACM got: %s\n", receivedChars);
#endif
      }
  }
}

void meters_updateStats(void) {
  if (newData == true) {
    switch (receivedChars[0]) {
      case 'C':
        //CPU
        lastValueReceived[CPU] = MIN(atoi(&receivedChars[1]), 100);
        break;
      case 'M':
        //Memory
        lastValueReceived[MEM] = MIN(atoi(&receivedChars[1]), 100);
        break;
    }

    //Update last serial received
    updateLastTimeReceived();

    //Ready to receive again
    newData = false;
  }
}

void updateLastValueReceived(int idx, int val) {
  lastValueReceived[idx] = val;
}

void updateLastTimeReceived(void) {
  lastSerialRecd = board_millis();
}

//Update meters and running stats
void meters_updateMeters(void) {
  unsigned long currentMillis = board_millis();

  if (currentMillis - lastMeterUpdate > METER_UPDATE_FREQ)
  {
    //Update both meters
    int i;
    for(i = 0; i < NUMBER_OF_METERS; i++) {
      int perc = 0;

      //Based on https://www.arduino.cc/en/Tutorial/Smoothing
      runningTotal[i] = runningTotal[i] - valuesRecd[i][valuesRecdIndex];
      valuesRecd[i][valuesRecdIndex] = lastValueReceived[i];
      runningTotal[i] = runningTotal[i] + valuesRecd[i][valuesRecdIndex];
      perc = runningTotal[i] / READINGS_COUNT;

      setMeter(METER_PINS[i], perc, METER_MAX[i]);
      setLEDStrip(i, perc);
    }
    ws2812_show(led_strip);

    //Advance index
    valuesRecdIndex = valuesRecdIndex + 1;
    if (valuesRecdIndex >= READINGS_COUNT)
      valuesRecdIndex = 0;

    lastMeterUpdate = currentMillis;
  }
}

//Move needles back and forth to show no data is
//being received. Stop once serial data rec'd again.
void meters_screenSaver(void) {
  if (board_millis() - lastSerialRecd > SERIAL_TIMEOUT) {
    static int aPos = 0;
    int bPos = 0;
    static int incAmt = 0;
    static unsigned long lastSSUpdate = 0;

    char in = getchar_timeout_us(0);
    if (in == ENDSTDIN || in < 0) {
      unsigned long currentMillis = board_millis();

      //B meter position is opposite of A meter position
      bPos = 100 - aPos;

      //Move needles
      for (uint8_t j = 0; j < NUMBER_OF_METERS; j+=2) {
        setMeter(METER_PINS[j], aPos, METER_MAX[j]);
      }
      for (uint8_t j = 1; j < NUMBER_OF_METERS; j+=2) {
        setMeter(METER_PINS[j], bPos, METER_MAX[j]);
      }

      //Update every 100ms
      if (currentMillis - lastSSUpdate > 100) {
        lastSSUpdate = board_millis();

        //Change meter direction if needed.
        if (aPos == 100)
          incAmt = -1;
        else if (aPos == 0)
          incAmt = 1;

        //Increment position
        aPos = aPos + incAmt;
        lastSSUpdate = currentMillis;
      }
    }
  }
}
