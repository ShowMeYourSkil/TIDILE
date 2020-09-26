#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "TIDILE.hpp"
#include "ClockInfo.hpp"
#include "ClockConfig.hpp"
#include "Handler.hpp"
#include "Webserver.hpp"
#include "config.h"

//#define BMP_SDA 21
//#define BMP_SCL 22

Adafruit_BME280 bmp; // I2C

CRGB leds[NUM_LEDS];
ClockConfig config;
TIDILE tidile;
Preferences preferences;

int lastSec = 0;

// Webserver
Handler handler(&config, &tidile, &preferences);
Webserver webserver;
AsyncWebServer server(HTTP_ENDPOINT_PORT);

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

ClockTime getTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return ClockTime{0, 0, 0, 0, 0, 0};
  }
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  return ClockTime{
    seconds : timeinfo.tm_sec,
    minutes : timeinfo.tm_min,
    hours : timeinfo.tm_hour,
    day : timeinfo.tm_mday,
    month : timeinfo.tm_mon,
    year : timeinfo.tm_year
  };
}

ClockEnv getEnv() {
  return ClockEnv {
    temperature: bmp.readTemperature(),
    pressure: bmp.readPressure()/100
  };
}

#pragma region setup
void setup()
{

  Serial.begin(115200);
  delay(10);

#pragma region Connecting to WiFi
  WiFi.begin();
  delay(2000);

  while (WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_AP_STA);
    delay(500);
    WiFi.beginSmartConfig();
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      Serial.println(WiFi.smartConfigDone());
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#pragma endregion

  // Time
  configTime(3600, 3600, ntpServer);

  Wire.begin();
  // BMP280
  bool ok = bmp.begin(0x76);
  if (!ok) {  
    Serial.println("Could not find a valid BMP280!");
    //while (1);
  }

  //register leds
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(config.brightness);

  config.colorHours = Color((byte)0x00, (byte)0xFF, (byte)0xFF);
  config.colorMinutes = Color((byte)0xFF, (byte)0xFF, (byte)0x00);

  tidile.setup(leds, NUM_LEDS, &config);
  webserver.setup(&handler, &server);
  
  startupLEDs(leds, 16);
  //preferences.clear();
  //config.deserialize(&preferences);
}
#pragma endregion

#pragma region loop
void loop()
{
  ClockTime time = getTime();
  tidile.displayTime(time);
  if (config.blinkingEnabled && time.seconds != lastSec)
  {
    FastLED.setBrightness(BLINK_BRIGHTNESS * config.brightness);
    FastLED.show();
    delay(100);
  }
  FastLED.setBrightness(config.brightness);
  FastLED.show();

  // ENV
  //printEnv();
  if (touchRead(4) < 20)
  {
    Serial.println("Display env...");
    tidile.displayEnv(getEnv());
    delay(2000);
  }

  // Set variables
  lastSec = time.seconds;
}
#pragma endregion
