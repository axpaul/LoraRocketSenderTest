/*
   RadioLib SX1276 Transmit Simulator for NectarMC

   This program simulates an experimental rocket flight and transmits
   telemetry packets conforming to the NectarMC ground station protocol.
   
   RF Settings:
    - Frequency: 869.525 MHz
    - Spreading Factor: 8
    - Bandwidth: 250 kHz
    - Hardware CRC: Enabled
*/

#include <Arduino.h>
#include <RadioLib.h>
#include "boards.h"
#include "telemetry_formats.h"

// =========================================================================
// OPTION DE DÉBOGAGE : Données statiques vs Simulation de vol
// =========================================================================
//#define STATIC_TEST_MODE  // <-- Décommenter pour envoyer des valeurs fixes de test (débogage chaîne)
                          // <-- Commenter pour activer le simulateur de vol physique dynamique

// Instantiate the SX1276 radio module
SX1276 radio = new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_DIO1_PIN);

// Global objects declared in boards.h
#ifdef HAS_DISPLAY
DISPLAY_MODEL *u8g2 = nullptr;
#endif
SPIClass SDSPI(HSPI);

// Radio state variables
volatile bool transmittedFlag = false;
volatile bool enableInterrupt = true;
bool radioTransmitting = false;
uint32_t packetsSent = 0;

// Simulation State variables
uint8_t flightState = 0;          // 0: IDLE, 1: THRUST, 2: COASTING, 3: DESCENT, 4: LANDED
unsigned long lastSimUpdate = 0;
float simAltitude = 150.0;        // starting altitude (m)
float simVelocity = 0.0;        // velocity (m/s)
float simPressure = 101325.0;     // barometric pressure (Pa)
float simTemperature = 22.0;      // temperature (°C)
float simAccelX = 0.0;
float simAccelY = 0.0;
float simAccelZ = 1.0;            // accelerometer reading (G), 1G gravity when idle
float simGyroX = 0.0;
float simGyroY = 0.0;
float simGyroZ = 0.0;
float simLat = 43.2324;           // GPS latitude (site near Tarbes, France)
float simLon = 0.0782;            // GPS longitude
float simBattery = 4.15;          // battery voltage (V)

// Transmission timers
unsigned long lastSensorTx = 0;   // timer for APID_SENSORS (5 Hz)
unsigned long lastGpsTx = 0;      // timer for APID_GPS (0.5 Hz)

// Interrupt service routine called when packet transmission is finished
void IRAM_ATTR setFlag(void)
{
    if (!enableInterrupt) {
        return;
    }
    transmittedFlag = true;
}

// Update the flight dynamics and sensor simulation
void updateFlightSimulation() {
#ifdef STATIC_TEST_MODE
    // Values are locked to static constants for debugging the telemetry chain
    simAltitude = 500.0;
    simVelocity = 0.0;
    simPressure = 95000.0;
    simTemperature = 25.0;
    simAccelX = 0.1;
    simAccelY = -0.2;
    simAccelZ = 1.0;
    simGyroX = 10.0;
    simGyroY = -20.0;
    simGyroZ = 30.0;
    simLat = 43.5;
    simLon = 1.5;
    simBattery = 4.20;
    flightState = 0; // IDLE
    lastSimUpdate = millis();
    return;
#endif

    unsigned long now = millis();
    if (lastSimUpdate == 0) {
        lastSimUpdate = now;
        return;
    }
    
    float dt = (now - lastSimUpdate) / 1000.0;
    if (dt < 0.01) return; // limit sim refresh rate to ~100 Hz
    lastSimUpdate = now;
    
    // Slow battery discharge
    simBattery -= 0.000005 * dt;
    if (simBattery < 3.5) simBattery = 3.5;
    
    switch (flightState) {
        case 0: // IDLE (waiting on launchpad)
            simAltitude = 150.0 + (random(-10, 10) / 100.0);
            simVelocity = 0.0;
            simAccelX = (random(-5, 5) / 100.0);
            simAccelY = (random(-5, 5) / 100.0);
            simAccelZ = 1.0 + (random(-5, 5) / 100.0); // reads gravity
            simGyroX = (random(-10, 10) / 10.0);
            simGyroY = (random(-10, 10) / 10.0);
            simGyroZ = (random(-10, 10) / 10.0);
            
            // Launch automatically after 15 seconds
            if (now > 15000) {
                flightState = 1; // Transition to THRUST
                Serial.println(F("[SIM] Launch Triggered! Entering THRUST phase."));
            }
            break;
            
        case 1: // THRUST (motor powered flight, high Gs, climbing fast)
            simAccelX = (random(-30, 30) / 10.0);
            simAccelY = (random(-30, 30) / 10.0);
            simAccelZ = 6.0 + (random(-100, 100) / 100.0); // 5G to 7G acceleration
            
            simVelocity += (simAccelZ - 1.0) * 9.81 * dt;
            simAltitude += simVelocity * dt;
            
            // Spin stabilization simulation
            simGyroZ = 360.0 + (random(-50, 50));
            simGyroX = (random(-20, 20));
            simGyroY = (random(-20, 20));
            
            // Burnout after 3.5 seconds of powered flight (18.5s total run time)
            if (now > 18500) {
                flightState = 2; // Transition to COASTING
                Serial.println(F("[SIM] Burnout! Entering COASTING phase."));
            }
            break;
            
        case 2: // COASTING (free fall, deceleration, weightlessness)
            simAccelX = (random(-5, 5) / 10.0);
            simAccelY = (random(-5, 5) / 10.0);
            simAccelZ = 0.0 + (random(-5, 5) / 100.0); // ~0G weightlessness in free fall
            
            // Apply gravity and simplified air resistance drag
            {
                float drag = 0.0005 * simVelocity * abs(simVelocity);
                simVelocity -= (9.81 + drag) * dt;
            }
            simAltitude += simVelocity * dt;
            
            // Gyro rotation decays
            simGyroZ = 180.0 * (1.0 - (now - 18500.0)/8500.0) + (random(-10, 10));
            if (simGyroZ < 0) simGyroZ = 0;
            simGyroX = (random(-5, 5));
            simGyroY = (random(-5, 5));
            
            // Apogee detection (speed turns negative)
            if (simVelocity <= 0.0) {
                flightState = 3; // Transition to DESCENT
                simVelocity = -8.0; // Terminal descent rate
                Serial.printf("[SIM] Apogee reached at %.2f m! Entering DESCENT phase.\n", simAltitude);
            }
            break;
            
        case 3: // DESCENT (stable descent under parachute)
            simVelocity = -8.0 + (random(-50, 50) / 100.0); // average -8 m/s
            simAltitude += simVelocity * dt;
            
            simAccelX = (random(-20, 20) / 10.0); // oscillations
            simAccelY = (random(-20, 20) / 10.0);
            simAccelZ = 1.0 + (random(-10, 10) / 10.0); // terminal velocity balanced by drag
            
            simGyroX = (random(-15, 15));
            simGyroY = (random(-15, 15));
            simGyroZ = (random(-10, 10));
            
            // Wind drift coordinates
            simLat += 0.000015 * dt;
            simLon += 0.000025 * dt;
            
            // Landing detection
            if (simAltitude <= 150.0) {
                simAltitude = 150.0;
                simVelocity = 0.0;
                flightState = 4; // Transition to LANDED
                Serial.println(F("[SIM] Rocket Landed! Entering LANDED phase."));
            }
            break;
            
        case 4: // LANDED (on ground, transmitting beacons)
            simAltitude = 150.0 + (random(-5, 5) / 100.0);
            simVelocity = 0.0;
            simAccelX = (random(-2, 2) / 100.0);
            simAccelY = (random(-2, 2) / 100.0);
            simAccelZ = 1.0 + (random(-2, 2) / 100.0); // stable gravity
            simGyroX = (random(-2, 2) / 10.0);
            simGyroY = (random(-2, 2) / 10.0);
            simGyroZ = (random(-2, 2) / 10.0);
            break;
    }
    
    // Barometric calculations
    simPressure = 101325.0 * pow(1.0 - 2.25577e-5 * simAltitude, 5.25588);
    simTemperature = 22.0 - 0.0065 * simAltitude + (random(-10, 10) / 100.0);
}

#ifdef HAS_DISPLAY
// Render simulator status on the TTGO OLED with premium graphics (Page 0: Telemetry, Page 1: RF Config)
void drawOledScreen(uint8_t page) {
    if (!u8g2) return;
    u8g2->clearBuffer();
    
    // Header Bar
    u8g2->setFont(u8g2_font_6x12_tr);
    if (page == 0) {
        u8g2->drawStr(0, 10, "NECTAR TELEM");
    } else {
        u8g2->drawStr(0, 10, "NECTAR RF CFG");
    }
    u8g2->drawHLine(0, 13, 114);
    
    // Trajectory vertical progress bar (X: 118 to 124, Y: 15 to 61)
    u8g2->drawFrame(118, 15, 6, 47);
    float normAlt = (simAltitude - 150.0) / 850.0; // scale from 150m to 1000m (apogee)
    if (normAlt < 0.0) normAlt = 0.0;
    if (normAlt > 1.0) normAlt = 1.0;
    int fillHeight = (int)(normAlt * 45.0);
    if (fillHeight > 0) {
        u8g2->drawBox(119, 61 - fillHeight, 4, fillHeight);
    }

    // Battery Icon (X: 98, Y: 2, Width: 12, Height: 6)
    u8g2->drawFrame(98, 2, 12, 6);
    u8g2->drawBox(110, 4, 1, 2);
    int batWidth = (int)((simBattery - 3.5) / 0.7 * 8.0);
    if (batWidth < 0) batWidth = 0;
    if (batWidth > 8) batWidth = 8;
    if (batWidth > 0) {
        u8g2->drawBox(100, 4, batWidth, 2);
    }
    
    // Radio transmission indicator (Signal Beacon at top right: X: 118, Y: 2)
    if (radioTransmitting) {
        u8g2->drawDisc(121, 5, 2);
    } else {
        u8g2->drawCircle(121, 5, 2);
    }

    // Text Information
    u8g2->setFont(u8g2_font_6x10_tf);
    char buf[32];
    
    if (page == 0) {
        const char* state_str = "IDLE";
#ifdef STATIC_TEST_MODE
        state_str = "STATIC TEST";
#else
        switch (flightState) {
            case 0: state_str = "IDLE"; break;
            case 1: state_str = "PROPULSION"; break;
            case 2: state_str = "APOGEE/FREE"; break;
            case 3: state_str = "DESCENT"; break;
            case 4: state_str = "LANDED"; break;
        }
#endif
        snprintf(buf, sizeof(buf), "ETAT : %s", state_str);
        u8g2->drawStr(0, 26, buf);
        
        snprintf(buf, sizeof(buf), "ALT  : %.1f m", simAltitude);
        u8g2->drawStr(0, 38, buf);
        
        snprintf(buf, sizeof(buf), "VIT  : %.1f m/s", simVelocity);
        u8g2->drawStr(0, 50, buf);
        
        snprintf(buf, sizeof(buf), "PKTS : %d", packetsSent);
        u8g2->drawStr(0, 62, buf);
    } else {
        snprintf(buf, sizeof(buf), "FREQ : %.3f MHz", NECTAR_LORA_FREQUENCY);
        u8g2->drawStr(0, 26, buf);
        
        snprintf(buf, sizeof(buf), "PWR  : %d dBm", NECTAR_LORA_POWER);
        u8g2->drawStr(0, 38, buf);
        
        snprintf(buf, sizeof(buf), "SF   : %d", NECTAR_LORA_SPREAD_FACTOR);
        u8g2->drawStr(0, 50, buf);
        
        snprintf(buf, sizeof(buf), "BW   : %.1f kHz", NECTAR_LORA_BANDWIDTH);
        u8g2->drawStr(0, 62, buf);
    }
    
    u8g2->sendBuffer();
}
#endif

// Helper function to print packet in HEX format
void printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] < 0x10) Serial.print('0');
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}

void setup()
{
    initBoard();
    delay(1500);

    Serial.printf("[SX1276] Initializing LoRa at %.3f MHz ... ", NECTAR_LORA_FREQUENCY);
    int state = radio.begin(NECTAR_LORA_FREQUENCY);

#ifdef HAS_DISPLAY
    if (u8g2) {
        if (state != RADIOLIB_ERR_NONE) {
            u8g2->clearBuffer();
            u8g2->drawStr(0, 12, "LoRa Init: FAIL!");
            u8g2->sendBuffer();
        }
    }
#endif

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
        radio.setOutputPower(NECTAR_LORA_POWER);
        radio.setBandwidth(NECTAR_LORA_BANDWIDTH);
        radio.setCurrentLimit(NECTAR_LORA_CURRENT_LIMIT);
        radio.setSpreadingFactor(NECTAR_LORA_SPREAD_FACTOR);
        radio.setCRC(true, false);
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        #ifdef HAS_DISPLAY
        if (u8g2) {
            u8g2->clearBuffer();
            u8g2->setFont(u8g2_font_6x12_tr);
            u8g2->drawStr(0, 12, "LoRa Init: FAIL!");
            char code_str[16];
            snprintf(code_str, sizeof(code_str), "Code: %d", state);
            u8g2->drawStr(0, 26, code_str);
            u8g2->sendBuffer();
        }
        #endif
        while (true) {
            #ifdef BOARD_LED
            digitalWrite(BOARD_LED, !digitalRead(BOARD_LED));
            #endif
            delay(1000); // Yield to the watchdog and CPU to prevent hardware bootloops
        }
    }

    // Attach DIO0 interrupt handler
    radio.setDio0Action(setFlag, RISING);

    Serial.println(F("[SIM] Simulation and LoRa transmitter ready."));
}

void loop()
{
    // Update the flight simulator variables
    updateFlightSimulation();

    // 1. Process transmit completion interrupt
    if (transmittedFlag) {
        enableInterrupt = false;
        transmittedFlag = false;
        
        radio.finishTransmit();
        radioTransmitting = false;
        
        enableInterrupt = true;
    }

    // 2. Schedule LoRa packet transmission
    if (!radioTransmitting) {
        unsigned long now = millis();
        
        // APID_SENSORS (Slower frequency, 1 Hz = every 1000 ms)
        if (now - lastSensorTx >= 1000) {
            lastSensorTx = now;
            
            uint8_t packet[sizeof(LoRaHeader) + sizeof(SensorTelemetry)];
            
            // Format LoRa Header
            LoRaHeader* header = (LoRaHeader*)packet;
            header->ssid_num = TELEMETRY_SSID_NUM;
            header->apid = APID_SENSORS;
            header->ssid_type = TELEMETRY_SSID_TYPE;
            
            // Format Sensor Telemetry Payload
            SensorTelemetry* sensors = (SensorTelemetry*)(packet + sizeof(LoRaHeader));
            sensors->timestamp = now;
            sensors->baro_altitude = simAltitude;
            sensors->pressure = simPressure;
            sensors->temperature = simTemperature;
            sensors->accel_x = simAccelX;
            sensors->accel_y = simAccelY;
            sensors->accel_z = simAccelZ;
            sensors->gyro_x = simGyroX;
            sensors->gyro_y = simGyroY;
            sensors->gyro_z = simGyroZ;
            sensors->flight_state = flightState;
            
            // Trigger transmit
            int state = radio.startTransmit(packet, sizeof(packet));
            if (state == RADIOLIB_ERR_NONE) {
                radioTransmitting = true;
                packetsSent++;
                
                // Print structured info to console
                Serial.printf("\n--- [TX] APID SENSORS | Freq: %.3f MHz ---\n", NECTAR_LORA_FREQUENCY);
                Serial.print("Raw Bytes : ");
                printHex(packet, sizeof(packet));
                Serial.printf("  SSID: %d | APID: %d | Type: %d\n", header->ssid_num, header->apid, header->ssid_type);
                Serial.printf("  Time: %u ms | Alt Baro: %.2f m | Press: %.1f Pa | Temp: %.1f C\n",
                              sensors->timestamp, sensors->baro_altitude, sensors->pressure, sensors->temperature);
                Serial.printf("  Accel: (%.2f, %.2f, %.2f) G | Gyro: (%.1f, %.1f, %.1f) deg/s\n",
                              sensors->accel_x, sensors->accel_y, sensors->accel_z,
                              sensors->gyro_x, sensors->gyro_y, sensors->gyro_z);
                Serial.printf("  State: %d\n", sensors->flight_state);
                Serial.println("----------------------------------------------");
            } else {
                Serial.printf("[LoRa] APID_SENSORS transmit failed, code %d\n", state);
            }
        }
        // APID_GPS (Slower frequency, 0.2 Hz = every 5000 ms)
        else if (now - lastGpsTx >= 5000) {
            lastGpsTx = now;
            
            uint8_t packet[sizeof(LoRaHeader) + sizeof(GpsTelemetry)];
            
            // Format LoRa Header
            LoRaHeader* header = (LoRaHeader*)packet;
            header->ssid_num = TELEMETRY_SSID_NUM;
            header->apid = APID_GPS;
            header->ssid_type = TELEMETRY_SSID_TYPE;
            
            // Format GPS Telemetry Payload
            GpsTelemetry* gps = (GpsTelemetry*)(packet + sizeof(LoRaHeader));
            gps->timestamp = now;
            gps->latitude = simLat;
            gps->longitude = simLon;
            gps->altitude = simAltitude;
            gps->satellites = 10;
            gps->battery_voltage = simBattery;
            
            // Trigger transmit
            int state = radio.startTransmit(packet, sizeof(packet));
            if (state == RADIOLIB_ERR_NONE) {
                radioTransmitting = true;
                packetsSent++;
                
                // Print structured info to console
                Serial.printf("\n--- [TX] APID GPS | Freq: %.3f MHz ---\n", NECTAR_LORA_FREQUENCY);
                Serial.print("Raw Bytes : ");
                printHex(packet, sizeof(packet));
                Serial.printf("  SSID: %d | APID: %d | Type: %d\n", header->ssid_num, header->apid, header->ssid_type);
                Serial.printf("  Time: %u ms | Lat: %.6f | Lon: %.6f | Alt GPS: %.1f m\n",
                              gps->timestamp, gps->latitude, gps->longitude, gps->altitude);
                Serial.printf("  Sats: %d | Battery: %.2f V\n", gps->satellites, gps->battery_voltage);
                Serial.println("----------------------------------------------");
            } else {
                Serial.printf("[LoRa] APID_GPS transmit failed, code %d\n", state);
            }
        }
    }

    // 3. Update the display (every 200 ms to avoid flickering) and alternate pages
#ifdef HAS_DISPLAY
    static unsigned long lastDisplayUpdate = 0;
    static unsigned long lastPageSwitch = 0;
    static uint8_t currentPage = 0; // 0 = Telemetry, 1 = RF Config
    
    unsigned long oledNow = millis();
    if (oledNow - lastPageSwitch >= 3000) { // Cycle pages every 3 seconds
        lastPageSwitch = oledNow;
        currentPage = (currentPage + 1) % 2;
    }
    
    if (oledNow - lastDisplayUpdate >= 200) {
        lastDisplayUpdate = oledNow;
        drawOledScreen(currentPage);
    }
#endif
}
