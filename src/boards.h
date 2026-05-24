#ifndef BOARDS_H
#define BOARDS_H

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "utilities.h"

#define FW_VERSION "1.0.0"

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
        u8g2->setFlipMode(0);
        u8g2->setFontMode(1); // Transparent
        u8g2->setDrawColor(1);
        u8g2->setFontDirection(0);

        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char idBuf[32];
        snprintf(idBuf, sizeof(idBuf), "ID: %02X%02X  v%s", mac[4], mac[5], FW_VERSION);

        // Dynamic 3-second startup animation (15 frames of 200ms)
        for (int frame = 0; frame < 15; frame++) {
            u8g2->clearBuffer();
            
            // Draw Title and Subtitle on the left
            u8g2->setFont(u8g2_font_fur17_tf);
            u8g2->drawStr(5, 26, "NECTAR");
            u8g2->drawHLine(2, 33, 85);
            u8g2->drawHLine(2, 34, 85);
            u8g2->setFont(u8g2_font_fur11_tf);
            u8g2->drawStr(5, 52, "TX SENDER");
            
            // Info footer
            u8g2->setFont(u8g2_font_5x7_tr);
            u8g2->drawStr(5, 62, idBuf);
            
            // Draw Antenna Tower on the right (centered at x = 110)
            // Base legs
            u8g2->drawLine(110, 35, 100, 58);
            u8g2->drawLine(110, 35, 120, 58);
            u8g2->drawHLine(97, 58, 27);
            // Cross lattice
            u8g2->drawLine(103, 51, 110, 43);
            u8g2->drawLine(117, 51, 110, 43);
            // Vertical mast
            u8g2->drawVLine(110, 22, 13);
            // Tip disc
            u8g2->drawDisc(110, 22, 2);

            // Animate transmitting waves (left quadrants, propagating outward)
            int waveStage = frame % 5; // Cycle: 0 -> 1 -> 2 -> 3 -> 4
            if (waveStage >= 1) {
                u8g2->drawCircle(110, 22, 7, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_LOWER_LEFT);
            }
            if (waveStage >= 2) {
                u8g2->drawCircle(110, 22, 14, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_LOWER_LEFT);
            }
            if (waveStage >= 3) {
                u8g2->drawCircle(110, 22, 21, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_LOWER_LEFT);
            }
            if (waveStage >= 4) {
                u8g2->drawCircle(110, 22, 28, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_LOWER_LEFT);
            }

            u8g2->sendBuffer();
            delay(200);
        }
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
