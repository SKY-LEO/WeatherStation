#include <Arduino.h>
#include <esp_task_wdt.h>
#include "epd_driver.h"
#include "esp_adc_cal.h"
#include <SPI.h>
#include <Wire.h>
#include <RH_ASK.h>
#include <RTClib.h>
#include <Adafruit_HTU21DF.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include "cert.h"
#include "opensans8b.h"
#include "opensans10b.h"
#include "opensans12b.h"
#include "opensans18b.h"
#include "opensans24b.h"
#include "opensans26b.h"
#include "opensans26.h"
#include "OpenSans32b.h"
#include "OpenSans64b.h"
#include "OpenSans48b.h"
#include "OpenSans48.h"
#include "picture.h"
#define URL_Firmware_Version "https://raw.githubusercontent.com/SKY-LEO/WeatherStation/main/WeatherStation/version.txt"
#define URL_Firmware_Bin "https://raw.githubusercontent.com/SKY-LEO/WeatherStation/main/WeatherStation/WeatherStation.ino.esp32.bin"
#define PERIOD_WAITING 20000 //milliseconds
#define PERIOD_WAITING1 10000 //milliseconds
#define SEALEVELPRESSURE_HPA (1013.25)
#define SCREEN_WIDTH   EPD_WIDTH
#define SCREEN_HEIGHT  EPD_HEIGHT
#define I2C_SDA 14
#define I2C_SCL 15
#define White         0xFF
#define LightGrey     0xBB
#define Grey          0x88
#define DarkGrey      0x44
#define Black         0x00
#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false
enum alignment {LEFT, RIGHT, CENTER};
const char * ssid = "ssid";
const char * password = "password";
String FirmwareVer = {"0.8"};
int vref = 1100;
long StartTime = 0;
long SleepTimer = 1200;
float outdoor_temperature = 0;
float outdoor_humidity = 0;
float outdoor_pressurehPa = 0;
float outdoor_pressuremmHg = 0;
float outdoor_voltage = 0;
float indoor_temperature = 0;
float indoor_humidity = 0;
GFXfont  currentFont;
uint8_t *framebuffer;
RTC_DATA_ATTR int t_hour = 0;
RTC_DATA_ATTR int t_minute = 0;
RTC_DATA_ATTR int tempnew = 0;
RTC_DATA_ATTR int temp0 = 0;
RTC_DATA_ATTR int temp1 = 0;
RTC_DATA_ATTR int temp2 = 0;
RTC_DATA_ATTR int temp3 = 0;
RTC_DATA_ATTR int temp4 = 0;
RTC_DATA_ATTR int temp5 = 0;
RTC_DATA_ATTR int temp6 = 0;
RTC_DATA_ATTR int temp7 = 0;
RTC_DATA_ATTR int temp8 = 0;
RTC_DATA_ATTR int temp9 = 0;
RTC_DATA_ATTR int Z = 0;
RTC_DATA_ATTR int prevZ = 0;
RTC_DATA_ATTR int VersionCheck = 5;
int altitude;   // Here you should put the REAL altitude
RH_ASK driver(2000, 12, NO_PIN);
Adafruit_HTU21DF htu = Adafruit_HTU21DF();
RTC_DS3231 rtc;
struct datastruct {
  int32_t temp;
  int32_t pres;
  int32_t hum;
  int32_t vcc;
} mydata;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup()
{
  InitialiseSystem();
  InternetServices();
  GetWeather();
  epd_poweron();      // Switch on EPD display
  epd_clear();     // Clear the screen
  DisplayWeather();   // Display the weather data
  edp_update();       // Update the display to show the information
  poweroff_output(13);
  epd_poweroff_all(); // Switch off all power to EPD
  //BeginSleep();
}

void InitialiseSystem()
{
  StartTime = millis();
  Serial.begin(115200);
  Serial.println("Starting...");
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  driver.init();
  Wire.begin(I2C_SDA, I2C_SCL);
  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  htu.begin();
  if (!htu.begin())
  {
    Serial.println("Couldn't find sensor HTU21D!");
    while (1);
  }
  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) Serial.println("Memory alloc failed!");
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
}

void InternetServices()
{
  if (CheckInternet())
  {
    if (VersionCheck > 4)
    {
      if (FirmwareVersionCheck())
      {
        VersionCheck = 0;
        long StartTime = millis();
        long Time = 0;
        bool flag = 0;
        pinMode(35, INPUT);
        pinMode(39, INPUT);
        epd_poweron();
        epd_clear();
        setFont(OpenSans32B);
        drawString1(70, 250, "New version is available.", LEFT);
        drawString1(50, 300, "Install this update?(10sec)", LEFT);
        setFont(OpenSans26B);
        drawString1(335, 40, "YES", LEFT);
        drawString1(477, 40, "NO", LEFT);
        Rect_t area = {
          .x = 365,
          .y = 00,
          .width = picture_width,
          .height =  picture_height
        };
        Rect_t area1 = {
          .x = 500,
          .y = 0,
          .width = picture_width,
          .height =  picture_height
        };
        epd_draw_grayscale_image(area, (uint8_t *) picture_data);
        epd_draw_grayscale_image(area1, (uint8_t *) picture_data);
        while (flag != 1 && Time < PERIOD_WAITING1)
        {
          Time = millis() - StartTime;
          if (!digitalRead(35))
          {
            Serial.println("Update has begun");
            firmwareUpdate();
            flag = 1;
          } else if (!digitalRead(39))
          {
            Serial.println("Update canceled");
            flag = 1;
          }
        }
      }
    } else
    {
      VersionCheck++;
    }
  }
}

bool CheckInternet()
{
  long StartTime = millis();
  long Time = 0;
  Serial.println("Waiting for WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && Time < PERIOD_WAITING)
  {
    Time = millis() - StartTime;
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("WiFi connected");
    return true;
  } else
  {
    Serial.println("");
    Serial.println("NO INTERNET!!!");
    return false;
  }
}

bool FirmwareVersionCheck()
{
  String payload;
  int httpCode;
  String firmwareurl = "";
  firmwareurl += URL_Firmware_Version;
  firmwareurl += "?";
  firmwareurl += String(rand());
  Serial.println(firmwareurl);
  WiFiClientSecure * client = new WiFiClientSecure;
  if (client)
  {
    client -> setCACert(rootCACertificate);
    // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
    HTTPClient https;
    if (https.begin( * client, firmwareurl))
    { // HTTPS
      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      delay(100);
      httpCode = https.GET();
      delay(100);
      if (httpCode == HTTP_CODE_OK) // if version received
      {
        payload = https.getString(); // save received version
      } else {
        Serial.print("error in downloading version file:");
        Serial.println(httpCode);
      }
      https.end();
    }
    delete client;
  }
  if (httpCode == HTTP_CODE_OK) // if version received
  {
    payload.trim();
    if (payload.equals(FirmwareVer)) {
      Serial.printf("\nDevice already on latest firmware version:%s\n", FirmwareVer);
      return false;
    }
    else
    {
      Serial.println(payload);
      Serial.println("New firmware detected");
      return true;
    }
  }
  return false;
}

void firmwareUpdate()
{
  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  t_httpUpdate_return ret = httpUpdate.update(client, URL_Firmware_Bin);
  switch (ret)
  {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
}

void GetWeather()
{
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);
  bool flagrec = 0;
  int i = 0;
  long StartTime = millis();
  long Time = 0;
  Serial.print("Waiting data from indoor sensor...");
  indoor_temperature = htu.readTemperature();
  indoor_humidity = htu.readHumidity();
  Serial.println("GOT!");
  Serial.println("Indoor:");
  Serial.print("Temperature: ");
  Serial.print(indoor_temperature);
  Serial.println(" °C");
  Serial.print("Humidity: ");
  Serial.print(indoor_humidity);
  Serial.println(" %");
  while (Time < PERIOD_WAITING && flagrec != 1)
  {
    Time = millis() - StartTime;
    for (i; i < 1; i++)
    {
      Serial.print("Waiting data from outdoor sensor...");
    }
    if (driver.recv(buf, &buflen))
    {
      Serial.println("GOT!");
      memcpy(&mydata, buf, sizeof(mydata));
      outdoor_temperature = mydata.temp;
      outdoor_temperature /= 100;
      outdoor_pressurehPa = mydata.pres;
      outdoor_pressurehPa /= 100;
      outdoor_pressuremmHg = outdoor_pressurehPa / 1.333;
      outdoor_humidity = mydata.hum;
      outdoor_humidity /= 100;
      outdoor_voltage = mydata.vcc;
      outdoor_voltage /= 1000;
      Serial.println("Outdoor:");
      Serial.print("Temperature: ");
      Serial.print(outdoor_temperature);
      Serial.println(" °C");
      Serial.print("Pressure: ");
      Serial.print(outdoor_pressuremmHg);
      Serial.println(" mmHg");
      Serial.print("Humidity: ");
      Serial.print(outdoor_humidity);
      Serial.println(" %");
      Serial.print("Battery voltage: ");
      Serial.print(outdoor_voltage);
      Serial.println("v");
      flagrec = 1;
    }
  }
  if (flagrec == 0)
  {
    Serial.println("NO DATA FROM OUTDOOR SENSOR!!! CHECK BATTERY AND CONNECTION!!!");
  }
}

void DisplayWeather()
{ // 4.7" e-paper display is 960x540 resolution
  DisplayStatusSection(680, 20);    // Wi-Fi and Battery voltage
  DisplayTimeSection(0, 0);                   // Time and day of week
  DisplayMainWeatherSection(0, 110);
  //DisplayWeatherIcon(810, 130);                  // Display weather icon    scale = Large;
  //DisplayForecastSection(320, 220);
}

void DisplayStatusSection(int x, int y)
{
  setFont(OpenSans8B);
  DrawBattery(x, y);
}

void DisplayTimeSection(int x, int y)
{
  setFont(OpenSans10B);
  DrawTime(x, y);
}

void DrawTime(int x, int y)
{
  DateTime now = rtc.now();
  drawString(x, y, String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second()) + ", " + String(daysOfTheWeek[now.dayOfTheWeek()]) + ", " + String(now.day()) + "." + String(now.month()) + "." + String(now.year()), LEFT);
}

void DrawBattery(int x, int y)
{
  uint8_t percentage = 100;
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
  {
    Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
    vref = adc_chars.vref;
  }
  float voltage = analogRead(36) / 4096.0 * 6.566 * (vref / 1000.0);
  if (voltage > 1 )
  {
    Serial.println("\nVoltage = " + String(voltage));
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.20) percentage = 0;  // orig 3.5
    drawRect(x + 40, y - 16, 40, 15, Black);
    fillRect(x + 80, y - 12, 4, 7, Black);
    fillRect(x + 42, y - 14, 36 * percentage / 100.0, 11, Black);
    drawString(x + 12, y - 14, "IN: ", LEFT);
    drawString(x + 85, y - 14, String(percentage) + "%  " + String(voltage, 2) + "v OUT: " + String(outdoor_voltage, 2) + "v" , LEFT);
  }
}

void DisplayMainWeatherSection(int x, int y)
{
  //setFont(OpenSans26b);
  DisplayOutdoorSection(x, y + 200);
  DisplayIndoorSection(x + 375, y + 200);
  //DisplayForecastTextSection(x - 55, y + 25);
  DisplayNamings(x + 245, y + 248);
}

void DisplayOutdoorSection(int x, int y)
{
  setFont(OpenSans48);
  DisplayOTemperatureSection(x, y - 30);
  DisplayOHumiditySection(x, y + 60);
  DisplayOPressureSection(x, y + 150);
}

void DisplayIndoorSection(int x, int y)
{
  setFont(OpenSans48);
  DisplayITemperatureSection(x, y + 60);
  DisplayIHumiditySection(x, y + 150);
}

void DisplayOTemperatureSection(int x, int y)
{
  if (outdoor_temperature > -10.00)
  {
    drawString(x, y, String(outdoor_temperature), LEFT);
  } else
  {
    drawString(x, y, String(outdoor_temperature, 1), LEFT);
  }
}

void DisplayOHumiditySection(int x, int y)
{
  drawString(x, y, String(outdoor_humidity), LEFT);
}

void DisplayOPressureSection(int x, int y)
{
  drawString(x, y, String(outdoor_pressuremmHg, 1), LEFT);
}

void DisplayITemperatureSection(int x, int y)
{
  drawString(x, y, String(indoor_temperature), LEFT);
}

void DisplayIHumiditySection(int x, int y)
{
  drawString(x, y, String(indoor_humidity), LEFT);
}

void DisplayNamings(int x, int y)
{
  setFont(OpenSans32B);
  drawString(x - 245, y - 145, "Out:", LEFT);
  drawString(x + 130, y - 55, "In:", LEFT);
  setFont(OpenSans24B);
  drawString(x, y - 40, " °C", LEFT);
  drawString(x, y + 50, " %", LEFT);
  drawString(x + 375, y + 50, " °C", LEFT);
  drawString(x + 375, y + 140, " %", LEFT);
  setFont(OpenSans12B);
  drawString(x + 7, y + 152, " mmHg", LEFT);
}

void poweroff_output(int output)
{
  digitalWrite(output, LOW);
  pinMode(output, INPUT);
}

void drawString(int x, int y, String text, alignment align)
{
  char * data  = const_cast<char*>(text.c_str());
  int  x1, y1;
  int w, h;
  int xx = x, yy = y;
  get_text_bounds(&currentFont, data, &xx, &yy, &x1, &y1, &w, &h, NULL);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  int cursor_y = y + h;
  write_string(&currentFont, data, &x, &cursor_y, framebuffer);
}

void drawString1(int x, int y, String text, alignment align)
{
  char * data  = const_cast<char*>(text.c_str());
  int  x1, y1;
  int w, h;
  int xx = x, yy = y;
  get_text_bounds(&currentFont, data, &xx, &yy, &x1, &y1, &w, &h, NULL);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  int cursor_y = y + h;
  write_string(&currentFont, data, &x, &cursor_y, NULL);
}

void fillCircle(int x, int y, int r, uint8_t color)
{
  epd_fill_circle(x, y, r, color, framebuffer);
}

void drawFastHLine(int16_t x0, int16_t y0, int length, uint16_t color)
{
  epd_draw_hline(x0, y0, length, color, framebuffer);
}

void drawFastVLine(int16_t x0, int16_t y0, int length, uint16_t color)
{
  epd_draw_vline(x0, y0, length, color, framebuffer);
}

void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
  epd_write_line(x0, y0, x1, y1, color, framebuffer);
}

void drawCircle(int x0, int y0, int r, uint8_t color)
{
  epd_draw_circle(x0, y0, r, color, framebuffer);
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  epd_draw_rect(x, y, w, h, color, framebuffer);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  epd_fill_rect(x, y, w, h, color, framebuffer);
}

void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
  epd_fill_triangle(x0, y0, x1, y1, x2, y2, color, framebuffer);
}

void drawPixel(int x, int y, uint8_t color)
{
  epd_draw_pixel(x, y, color, framebuffer);
}

void setFont(GFXfont const & font)
{
  currentFont = font;
}

void edp_update()
{
  epd_draw_grayscale_image(epd_full_screen(), framebuffer); // Update the screen
}

void loop() {
  // put your main code here, to run repeatedly:

}
