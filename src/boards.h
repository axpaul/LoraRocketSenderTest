#ifndef BOARDS_H
#define BOARDS_H

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "utilities.h"

#ifdef HAS_SDCARD
#include <SD.h>
#include <FS.h>
#endif

#ifdef HAS_DISPLAY
#include <U8g2lib.h>
#define DISPLAY_MODEL U8G2_SSD1306_128X64_NONAME_F_HW_I2C
extern DISPLAY_MODEL *u8g2;
#endif

extern SPIClass SDSPI;

inline void initBoard()
{
    Serial.begin(115200);
    Serial.println(F("[BOARD] Initializing TTGO T3 V1.6.1..."));

    // Initialize SPI and I2C buses
    SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);
    Wire.begin(I2C_SDA, I2C_SCL);

    // Initialize LED
#ifdef BOARD_LED
    pinMode(BOARD_LED, OUTPUT);
    digitalWrite(BOARD_LED, LED_ON);
#endif

    // Initialize Display if configured
#ifdef HAS_DISPLAY
    Wire.beginTransmission(0x3C);
    if (Wire.endTransmission() == 0) {
        Serial.println(F("[BOARD] OLED Display detected at 0x3C. Initializing..."));
        u8g2 = new DISPLAY_MODEL(U8G2_R0, U8X8_PIN_NONE);
        u8g2->begin();
        u8g2->clearBuffer();
        u8g2->setFlipMode(0);
        u8g2->setFontMode(1); // Transparent
        u8g2->setDrawColor(1);
        u8g2->setFontDirection(0);
        u8g2->firstPage();
        do {
            u8g2->setFont(u8g2_font_ncenB08_tr);
            u8g2->drawStr(0, 16, "Nectar Simulator");
            u8g2->drawStr(0, 32, "TTGO T3 V1.6.1");
            u8g2->drawStr(0, 48, "Ready to Send");
        } while ( u8g2->nextPage() );
        u8g2->sendBuffer();
        delay(1500);
    } else {
        Serial.println(F("[BOARD] Warning: OLED display not found on I2C bus."));
    }
#endif

    // Initialize SD Card if configured
#ifdef HAS_SDCARD
    pinMode(SDCARD_MISO, INPUT_PULLUP);
    SDSPI.begin(SDCARD_SCLK, SDCARD_MISO, SDCARD_MOSI, SDCARD_CS);
    if (!SD.begin(SDCARD_CS, SDSPI)) {
        Serial.println(F("[BOARD] Warning: SD Card initialization failed."));
    } else {
        uint32_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial.printf("[BOARD] SD Card initialized successfully. Size: %.2f GB\n", cardSize / 1024.0);
    }
#endif
}

#endif // BOARDS_H
