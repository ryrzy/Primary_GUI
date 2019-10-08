#include "Arduino.h"
#define SUPLADEVICE_CPP
#include <SuplaDevice.h>

#include "supla_settings.h"
#include "supla_eeprom.h"
#include "supla_web_server.h"
#include "supla_board_settings.h"
#include "supla_oled.h"

// For a connection via I2C using the Arduino Wire include:
#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
//#include "SSD1306Wire.h"        // legacy: #include "SSD1306.h"
#include "SH1106Wire.h"   // legacy: #include "SH1106.h"


// Initialize the OLED display using Arduino Wire:
//SSD1306Wire display(0x3c, SDA, SCL);   // ADDRESS, SDA, SCL  ->supla_settings.h
SH1106Wire display(0x3c, SDA, SCL);     // ADDRESS, SDA, SCL ->supla_settings.h

#define FRAME_DURATION 3000

typedef void (*Frame)(void);
Frame *frames;

int frameCount = 0;
long timeSinceLastModeSwitch = 0;
int frameMode = 0;
int frameModeDS = 0;
int frameModeDHT = 0;

void display_signal(int x, int y) {
  int value = read_rssi_oled();
  //clear area only
  display.setColor(BLACK);
  display.fillRect(x, y, x + 46, 16);
  display.setColor(WHITE);
  if (value == -1) {

    display.setFont(ArialMT_Plain_10);
    display.drawString(x + 1, y, "x");

  } else {
    if (value > 0)
      display.fillRect(x, y + 6, 3, 4);
    else
      display.drawRect(x, y + 6, 3, 4);

    if (value >= 25)
      display.fillRect(x + 4, y + 4, 3, 6);
    else
      display.drawRect(x + 4, y + 4, 3, 6);

    if (value >= 50)
      display.fillRect(x + 8, y + 2, 3, 8);
    else
      display.drawRect(x + 8, y + 2, 3, 8);

    if (value >= 75)
      display.fillRect(x + 12, y, 3, 10);
    else
      display.drawRect(x + 12, y, 3, 10);

    //String s = String(value);
    // s+="%";
    //set current font size
    //display.setFont(ArialMT_Plain_10);
    //display.drawString(x+16, y, s.c_str());
  }
}
void  display_supla_status(int x, int y) {
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setColor(WHITE);
  display.drawStringMaxWidth(x, y, display.getWidth(), String(supla_status.status_msg));
  display.display();
}

void  display_config_mode() {
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setColor(WHITE);
  display.drawString(0, 15, "Tryb konfiguracyjny: " + String(Modul_tryb_konfiguracji));
  display.drawString(0, 28, "AP name: " + String(Config_Wifi_name));
  display.drawString(0, 41, "AP pass: "  + String(Config_Wifi_pass));
  display.drawString(0, 54, "IP: 192.168.4.1");
  display.display();
}

void display_relay_state(int x, int y) {
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  int xx = x;
  for (int i = 1; i <= nr_relay; ++i) {
    byte v = digitalRead(relay_button_channel[i - 1].relay);
    if (relay_button_channel[i - 1].invert == 1) v ^= 1;
    if (v == 1) {
      display.setColor(WHITE);
      display.fillRect(xx, y + 1, 10, 10);
      display.setColor(BLACK);
      display.drawString(xx + 2, y, String(i));
    } else {
      display.setColor(WHITE);
      display.drawString(xx + 2, y, String(i));
    }
    xx += 15;
  }
}

void display_temperature() {
  display.setColor(WHITE);

  if (nr_ds18b20 > 1) {
    int y = 0;
    int val = frameModeDS * 2;
    for (int i = val; i < val + 2; i++) {
      display.setFont(ArialMT_Plain_10);
      display.drawString(y, display.getHeight() / 2 - 10, "CH" + String(i));
      display.setFont(ArialMT_Plain_16);
      display.drawString(y, display.getHeight() / 2, String(ds18b20_channel[i].last_val, 1) + "ºC");
      y += display.getWidth() / 2;
    }
  } else {
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(display.getWidth() / 2, display.getHeight() / 2, String(ds18b20_channel[0].last_val, 1) + "ºC");
  }
}

void display_dht() {
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, display.getHeight() / 2 , String(dht_channel[frameModeDHT].temp, 1) + "ºC");
  display.drawString(display.getWidth() / 2, display.getHeight() / 2, String(dht_channel[frameModeDHT].humidity, 1) + "%");
}

void display_bme280() {
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, display.getHeight() / 2 - 10 , String(bme_channel.temp, 1) + "ºC");
  display.drawString(display.getWidth() / 2, display.getHeight() / 2 - 10, String(bme_channel.humidity, 1) + "%");
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(display.getWidth() / 2, display.getHeight() - 16, String(bme_channel.pressure, 1) + "hPa");
}

void supla_oled_logo() {
  display.clear();
  display.drawXbm(34, 14, supla_logo_width, supla_logo_height, supla_logo_bits);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(supla_logo_width + 40, display.getHeight() / 2, "SUPLA");
  display.display();
}

void supla_oled_start() {
  display.init();
  display.flipScreenVertically();

  supla_oled_logo();

  frames = (Frame*)malloc(sizeof(Frame) * (nr_ds18b20 / 2) + nr_dht + nr_bme);
  
  if (nr_ds18b20 > 1) {
    for (int i = 0; i < (nr_ds18b20 / 2); i++) {
      frames[frameCount] = {display_temperature};
      frameCount += 1;
    }
  } else {
    frames[frameCount] = {display_temperature};
    frameCount += 1;
  }
  if (nr_dht > 0) {
    for (int i = 0; i < nr_dht; i++) {
      frames[frameCount] = {display_dht};
      frameCount += 1;
    }
  }
  if (nr_bme > 0) {
    for (int i = 0; i < nr_bme; i++) {
      frames[frameCount] = {display_bme280};
      frameCount += 1;
    }
  }
}

void supla_oled_timer() {
  display.clear();
  display_signal(display.getWidth() - 16, 0);
  display_relay_state(0, 0);
  display.drawHorizontalLine(0, 14, display.getWidth());

  if (Modul_tryb_konfiguracji != 0) {
    display_config_mode();
    return;
  }
  if (supla_status.status != 17) {
    display_supla_status(0, display.getHeight() / 3);
    return;
  }

  frames[frameMode]();

  display.display();

  if (millis() - timeSinceLastModeSwitch > FRAME_DURATION) {
    frameMode = (frameMode + 1)  % frameCount;

    if (frameMode < nr_ds18b20 / 2 && nr_ds18b20 != 0)
      frameModeDS = (frameModeDS + 1)  % (nr_ds18b20 / 2);

    if (frameMode < (nr_ds18b20 / 2) + nr_dht && nr_dht != 0)
      frameModeDHT = (frameModeDHT + 1)  % nr_dht;

    timeSinceLastModeSwitch = millis();
  }
}