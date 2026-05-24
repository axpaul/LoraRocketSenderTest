#ifndef TELEMETRY_FORMATS_H
#define TELEMETRY_FORMATS_H

#include <Arduino.h>

// Limites réglementaires des bandes ISM (dites bandes ICM : Industrielles, Scientifiques et Médicales)
#define LORA_433_MIN_FREQ           433.05  // MHz
#define LORA_433_MAX_FREQ           434.79  // MHz
#define LORA_868_MIN_FREQ           863.00  // MHz
#define LORA_868_MAX_FREQ           870.00  // MHz

#if defined(LORA_BAND_NATIVE) && LORA_BAND_NATIVE == 433
#define NECTAR_LORA_FREQUENCY       433.05  // MHz (Bande 433 MHz, limite basse de la plage autorisée 433.05 - 434.79 MHz)
#else
#define NECTAR_LORA_FREQUENCY       869.525 // MHz (Bande 868 MHz, fréquence Nectar standard dans la plage 863 - 870 MHz)
#endif

// Validation de conformité des fréquences aux limites des bandes ICM à la compilation
#if defined(LORA_BAND_NATIVE) && LORA_BAND_NATIVE == 433
static_assert(NECTAR_LORA_FREQUENCY >= LORA_433_MIN_FREQ && NECTAR_LORA_FREQUENCY <= LORA_433_MAX_FREQ,
              "ERREUR: La fréquence LoRa configurée est en dehors de la plage ICM autorisée pour la bande 433 MHz (433.05 - 434.79 MHz) !");
#else
static_assert(NECTAR_LORA_FREQUENCY >= LORA_868_MIN_FREQ && NECTAR_LORA_FREQUENCY <= LORA_868_MAX_FREQ,
              "ERREUR: La fréquence LoRa configurée est en dehors de la plage ICM autorisée pour la bande 868 MHz (863.00 - 870.00 MHz) !");
#endif
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
