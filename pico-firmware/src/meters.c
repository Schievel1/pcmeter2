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

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include "pico/stdlib.h"
#include "pico.h"
#include "hardware/timer.h"
#include "meters.hpp"
#include "hardware/pwm.h"
#include "bsp/board.h"
#include "WS2812.hpp"
#include "WS2812.pio.h"

//Constants
const int METER_PINS[NUMBER_OF_METERS] = {3, 4, 5};     // Meter output pins
// Set this value to correct cheap meters that display wrong
// also if you have 3V meters tune this down so it shows 3V at 100% CPU load
const int METER_MAX[NUMBER_OF_METERS] = {228, 228, 228};    // Max value for meters
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

WS2812 ledStrip(
    2,            // Data line is connected to pin 0. (GP0)
    24,         // Strip is 6 LEDs long.
    pio0,               // Use PIO 0 for creating the state machine.
    0,                  // Index of the state machine that will be created for controlling the LED strip
                        // You can have 4 state machines per PIO-Block up to 8 overall.
                        // See Chapter 3 in: https://datasheets.raspberrypi.org/rp2040/rp2040-datasheet.pdf
    WS2812::FORMAT_GRB  // Pixel format used by the LED strip
);


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


//Set LED color
static void setLED(int greenPin, int redPin, int perc, int redPerc) {
  int isGreen = (perc < redPerc);
  gpio_put(greenPin, isGreen);
  gpio_put(redPin, !isGreen);
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
    ledStrip.setPixelColor(i0, WS2812::RGB(r,g,b));
    ledStrip.setPixelColor(i1, WS2812::RGB(r,g,b));
    ledStrip.setPixelColor(i2, WS2812::RGB(r,g,b));
    ledStrip.setPixelColor(i3, WS2812::RGB(r,g,b));
}

//Max both meters on startup as a test
static void meterStartup(void) {
 ledStrip.fill( WS2812::RGB(0, 0, 0) );
 ledStrip.show();
 for (int i = 0; i<100; i++) {
   setMeter(METER_PINS[CPU], i, METER_MAX[CPU]);
   setMeter(METER_PINS[MEM], i, METER_MAX[MEM]);
   sleep_ms(5);
 }
 for (int i = 100; i>0; i--) {
   setMeter(METER_PINS[CPU], i, METER_MAX[CPU]);
   setMeter(METER_PINS[MEM], i, METER_MAX[MEM]);
   sleep_ms(15);
 }
}

void meters_setup(void) {
    gpio_set_function(METER_PINS[CPU], GPIO_FUNC_PWM);
    gpio_set_function(METER_PINS[MEM], GPIO_FUNC_PWM);
    uint slice_num[] = {
      pwm_gpio_to_slice_num(METER_PINS[CPU]),
      pwm_gpio_to_slice_num(METER_PINS[MEM])
    };
    pwm_set_wrap(slice_num[CPU], 254);
    pwm_set_wrap(slice_num[MEM], 254);
    pwm_set_enabled(slice_num[CPU], true);
    pwm_set_enabled(slice_num[MEM], true);

    //Init values Received array
    for (int counter = 0; counter < READINGS_COUNT; counter++) {
        valuesRecd[CPU][counter] = 0;
        valuesRecd[MEM][counter] = 0;
    }

    meterStartup();

  //Get times started
  lastMeterUpdate = board_millis();
  lastSerialRecd = board_millis();
}

#define ENDSTDIN 255
void meters_receiveSerialData(void)
{
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
          printf("ACM got: %s\n", receivedChars);
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
    ledStrip.show();

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
      setMeter(METER_PINS[CPU], aPos, METER_MAX[CPU]);
      setMeter(METER_PINS[MEM], bPos, METER_MAX[MEM]);

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
