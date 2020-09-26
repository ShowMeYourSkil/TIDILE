#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include "TIDILE.hpp"
#include "helper.hpp"
#include "Handler.hpp"
#include "Webserver.hpp"
#include "definements.hpp"
#if defined(LIGHT_SENSOR) || defined(TEMPERATURE_SENSOR) || defined(HUMIDITY_SENSOR) || defined(PRESSURE_SENSOR)
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#endif

//#define BMP_SDA 21
//#define BMP_SCL 22

#if defined(HUMIDITY_SENSOR) && defined(BME280)
Adafruit_BME280 bmp; // I2C
#endif

CRGB leds[NUM_LEDS];
ClockConfig config;
TIDILE tidile;
Preferences preferences;

int lastSec = 0;

// Webserver
Handler handler(&config, &tidile, &preferences);
Webserver webserver;
AsyncWebServer server(HTTP_ENDPOINT_PORT);

#pragma region startup animation
void startupLEDs(CRGB *leds, int delayEach)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB::White;
    FastLED.show();
    delay(delayEach);
  }
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CRGB::Black;
    FastLED.show();
    delay(delayEach);
  }
}
#pragma endregion

ClockEnv getEnv()
{
  return ClockEnv{
#ifdef BME280
    temperature : bmp.readTemperature(),
    pressure : bmp.readPressure() / 100
#endif
  };
}

#pragma region setup
void setup()
{
  Serial.begin(115200);
  delay(100);

#pragma region Connecting to WiFi
  WiFi.begin();
  delay(2000);

  while (WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_AP_STA);
    delay(500);
    WiFi.beginSmartConfig();
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      Serial.println(WiFi.smartConfigDone());
      tries++;
      if (tries > 5)
        ESP.restart();
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#pragma endregion

#if defined(LIGHT_SENSOR) || defined(TEMPERATURE_SENSOR) || defined(HUMIDITY_SENSOR) || defined(PRESSURE_SENSOR)
  Wire.begin();
#ifdef HUMIDITY_SENSOR
  // BMP280
  bool ok = bmp.begin(0x76);
  if (!ok)
  {
    Serial.println("Could not find a valid BMP280!");
    //while (1);
  }
#endif
#endif

  config.deserialize(&preferences);

  //register leds
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(config.brightness);

  tidile.setup(leds, NUM_LEDS, &config);
  webserver.setup(&handler, &server);
  Helper.getTime();

#ifndef fastStartup
  startupLEDs(leds, 16);
#endif
}
#pragma endregion

#pragma region loop
int loopI = 0;
int average = 30;
void loop()
{
  ClockTime time = tidile.displayTime();
#pragma region blinking seconds
  if (config.blinkingEnabled && time.seconds != lastSec)
  {
    FastLED.setBrightness(BLINK_BRIGHTNESS * config.brightness);
    FastLED.show();
    delay(100);
  }
  FastLED.setBrightness(config.brightness);
  FastLED.show();
#pragma endregion

#pragma region touch pin handler
#if defined(DISPLAY_HUMIDIY) || defined(DISPLAY_TEMPERATURE) || defined(DISPLAY_PRESSURE)
  if (loopI >= SMOOTH_LOOPS)
  {
    if (average / SMOOTH_LOOPS < THRESHOLD)
    {
      Serial.println("Display env...");
      tidile.displayEnv(getEnv());
      delay(2000);
    }
    loopI = 0;
    average = 0;
  }
  else
    average += touchRead(TOUCH_PIN);
#endif

  // Set variables
  lastSec = time.seconds;
  loopI++;
#pragma endregion
}
#pragma endregion
