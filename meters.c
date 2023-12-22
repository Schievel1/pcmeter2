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
#include "meters.h"
#include "hardware/pwm.h"
#include "bsp/board.h"

//Constants
const int METER_PINS[2] = {11, 10};     // Meter output pins
// Set this value to correct cheap meters that display wrong
// also if you have 3V meters tune this down so it shows 3V at 100% CPU load
const int METER_MAX[2] = {228, 228};    // Max value for meters
const int GREEN_LEDS[2] = {4, 2};       // Green LED pins
const int RED_LEDS[2] = {5, 3};         // Red LED pins
const int RED_ZONE_PERC = 80;           // Percent at which LED goes from green to red
const int METER_UPDATE_FREQ = 100;      // Frequency of meter updates in milliseconds
const long SERIAL_TIMEOUT = 2000;       // How long to wait until serial "times out"
#define READINGS_COUNT 20          // Number of readings to average for each meter

//Variables
#define numRecChars  32            // Sets size of receive buffer
char receivedChars[numRecChars];        // Array for received serial data
bool newData = false;                   // Indicates if new data has been received
unsigned long lastSerialRecd = 0;       // Time last serial recd
unsigned long lastMeterUpdate = 0;      // Time meters last updated
int lastValueReceived[2] = {0, 0};      // Last value received
int valuesRecd[2][READINGS_COUNT];      // Readings to be averaged
int runningTotal[2] = {0, 0};           // Running totals
int valuesRecdIndex = 0;                // Index of current reading

// Emulate Arduinos board_millis() function
/* static uint32_t board_millis() { */
    /* return to_ms_since_boot(get_absolute_time()); */
/* } */

// Arduino map function
long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

//Set Meter position
static void setMeter(int meterPin, int perc, int meterMax)
{
  //Map perc to proper meter position
  int pos = map(perc, 0, 100, 0, meterMax);
  printf("setting pin %d to %d (was %d perc)\n", meterPin, pos, perc);
  pwm_set_gpio_level(meterPin, pos);
}


//Set LED color
static void setLED(int greenPin, int redPin, int perc, int redPerc)
{
  int isGreen = (perc < redPerc);
  gpio_put(greenPin, isGreen);
  gpio_put(redPin, !isGreen);
}


//Max both meters on startup as a test
static void meterStartup()
{
 gpio_put(RED_LEDS[0], true);
 gpio_put(RED_LEDS[1], true);
 for (int i = 0; i<100; i++) {
   setMeter(METER_PINS[0], i, METER_MAX[0]);
   setMeter(METER_PINS[1], i, METER_MAX[1]);
   sleep_ms(5);
 }
 for (int i = 100; i>0; i--) {
   setMeter(METER_PINS[0], i, METER_MAX[0]);
   setMeter(METER_PINS[1], i, METER_MAX[1]);
   sleep_ms(15);
 }
 sleep_ms(500);
}

void meters_setup() {
  //Setup pin modes
    /* gpio_init(METER_PINS[0]); */
    gpio_set_function(METER_PINS[0], GPIO_FUNC_PWM);
    gpio_set_function(METER_PINS[1], GPIO_FUNC_PWM);
    uint slice_num[] = {
      pwm_gpio_to_slice_num(METER_PINS[0]),
      pwm_gpio_to_slice_num(METER_PINS[1])
    };
    pwm_set_wrap(slice_num[0], 254);
    pwm_set_wrap(slice_num[1], 254);
    pwm_set_enabled(slice_num[0], true);
    pwm_set_enabled(slice_num[1], true);
    pwm_set_gpio_level(METER_PINS[0], 0);
    pwm_set_gpio_level(METER_PINS[1], 0);

    gpio_init(GREEN_LEDS[0]);
    gpio_set_dir(GREEN_LEDS[0], GPIO_OUT);
    gpio_init(RED_LEDS[0]);
    gpio_set_dir(RED_LEDS[0], GPIO_OUT);
    gpio_init(GREEN_LEDS[1]);
    gpio_set_dir(GREEN_LEDS[1], GPIO_OUT);
    gpio_init(RED_LEDS[1]);
    gpio_set_dir(RED_LEDS[1], GPIO_OUT);

    //Init values Received array
    for (int counter = 0; counter < READINGS_COUNT; counter++)
    {
        valuesRecd[0][counter] = 0;
        valuesRecd[1][counter] = 0;
    }

    meterStartup();

  //Get times started
  lastMeterUpdate = board_millis();
  lastSerialRecd = board_millis();
}

#define ENDSTDIN 255
void meters_receiveSerialData()
{
    // This is the recvWithEndMarker() function
    // from Robin2's serial data tutorial
    static uint8_t ndx = 0;
    char endMarker = '\r';
    char rc;

    while ((rc = getchar_timeout_us(0)) != ENDSTDIN && newData == false)
      {
        if (rc != endMarker)
        {
          receivedChars[ndx] = rc;
          ndx++;
          if (ndx >= numRecChars)
          {
            ndx = numRecChars - 1;
          }
          printf("got char %c \n", rc);
        }
        else
        {
          receivedChars[ndx] = '\0'; // terminate the string
          ndx = 0;
          newData = true;
          char* debugstr = receivedChars;
          printf("got string: %s \n", debugstr);
        }

    }
}

void meters_updateStats()
{
  if (newData == true)
  {
    switch (receivedChars[0])
    {
      case 'C':
        //CPU
        lastValueReceived[0] = MIN(atoi(&receivedChars[1]), 100);
        break;
      case 'M':
        //Memory
        lastValueReceived[1] = MIN(atoi(&receivedChars[1]), 100);
        break;
    }

    //Update last serial received
    lastSerialRecd = board_millis();

    //Ready to receive again
    newData = false;
  }
}

//Update meters and running stats
void meters_updateMeters()
{
  unsigned long currentMillis = board_millis();

  if (currentMillis - lastMeterUpdate > METER_UPDATE_FREQ)
  {
    //Update both meters
    int i;
    for(i = 0; i < 2; i++)
    {
      int perc = 0;

      //Based on https://www.arduino.cc/en/Tutorial/Smoothing
      runningTotal[i] = runningTotal[i] - valuesRecd[i][valuesRecdIndex];
      valuesRecd[i][valuesRecdIndex] = lastValueReceived[i];
      runningTotal[i] = runningTotal[i] + valuesRecd[i][valuesRecdIndex];
      perc = runningTotal[i] / READINGS_COUNT;

      setMeter(METER_PINS[i], perc, METER_MAX[i]);
      setLED(GREEN_LEDS[i], RED_LEDS[i], perc, RED_ZONE_PERC);
    }

    //Advance index
    valuesRecdIndex = valuesRecdIndex + 1;
    if (valuesRecdIndex >= READINGS_COUNT)
      valuesRecdIndex = 0;

    lastMeterUpdate = currentMillis;
  }
}

//Move needles back and forth to show no data is
//being received. Stop once serial data rec'd again.
void meters_screenSaver()
{
  if (board_millis() - lastSerialRecd > SERIAL_TIMEOUT)
  {
    int aPos = 0;
    int bPos = 0;
    int incAmt = 0;
    unsigned long lastSSUpdate = board_millis();

    //Turn off all LEDs
    gpio_put(GREEN_LEDS[0], false);
    gpio_put(GREEN_LEDS[1], false);
    gpio_put(RED_LEDS[0], false);
    gpio_put(RED_LEDS[1], false);

    while (getchar_timeout_us(0) == ENDSTDIN)
    {
      unsigned long currentMillis = board_millis();

      //Update every 100ms
      if (currentMillis - lastSSUpdate > 100)
      {
        //B meter position is opposite of A meter position
        bPos = 100 - aPos;

        //Move needles
        setMeter(METER_PINS[0], aPos, METER_MAX[0]);
        setMeter(METER_PINS[1], bPos, METER_MAX[1]);

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
