#ifndef TELEMETRY_FORMATS_H
#define TELEMETRY_FORMATS_H

#include <Arduino.h>

// Configuration LoRa matching the Nectar Ground Station
#define NECTAR_LORA_FREQUENCY       869.525 // MHz
#define NECTAR_LORA_SPREAD_FACTOR   8
#define NECTAR_LORA_BANDWIDTH       250.0   // kHz
#define NECTAR_LORA_POWER           17      // dBm
#define NECTAR_LORA_CURRENT_LIMIT   120     // mA

// Mission Configuration
#define TELEMETRY_SSID_NUM          99      // Mission number (e.g., FX99)
#define TELEMETRY_SSID_TYPE         0       // 0 = FX (Fusée Expérimentale)

// APIDs
#define APID_GPS                    1       // GPS and system telemetry
#define APID_SENSORS                2       // IMU & baro telemetry

#pragma pack(push, 1)

// LoRa header (3 bytes) expected by the Nectar receiver
struct LoRaHeader {
    uint8_t ssid_num;   // ID or number of the mission (0-255)
    uint8_t apid;       // APID / application ID (0-63)
    uint8_t ssid_type;  // Type of mission (0: FX, 1: MF, 2: BALLOON, 3: OTHER)
};

// APID 1 payload structure (21 bytes)
struct GpsTelemetry {
    uint32_t timestamp;      // Milliseconds since boot
    float latitude;          // GPS Latitude in degrees
    float longitude;         // GPS Longitude in degrees
    float altitude;          // GPS Altitude in meters
    uint8_t satellites;      // Number of satellites
    float battery_voltage;   // Battery voltage (V)
};

// APID 2 payload structure (41 bytes)
struct SensorTelemetry {
    uint32_t timestamp;      // Milliseconds since boot
    float baro_altitude;     // Barometric altitude in meters
    float pressure;          // Barometric pressure in Pascals
    float temperature;       // Temperature in °C
    float accel_x;           // Acceleration X in G
    float accel_y;           // Acceleration Y in G
    float accel_z;           // Acceleration Z in G
    float gyro_x;            // Angular velocity X in °/s
    float gyro_y;            // Angular velocity Y in °/s
    float gyro_z;            // Angular velocity Z in °/s
    uint8_t flight_state;    // Flight phase (0: IDLE, 1: THRUST, 2: COASTING, 3: DESCENT, 4: LANDED)
};

#pragma pack(pop)

#endif // TELEMETRY_FORMATS_H
