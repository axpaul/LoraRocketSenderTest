# Nectar LoRa Transmitter Simulator (TTGO T3 V1.6.1)

Ce projet configure une carte **LilyGO TTGO T3 V1.6.1** (ESP32 + SX1276 + OLED + SD) en émetteur de test dynamique de télémétrie. Il simule en temps réel le comportement physique d'une fusée expérimentale durant son vol et transmet périodiquement des trames LoRa formatées selon les spécifications attendues par la station au sol compatible avec le logiciel de traitement de données **NectarMC**.

---

## 📡 Paramètres de configuration RF LoRa

Le transmetteur utilise les paramètres radio suivants pour s'aligner sur la station de réception et respecter les réglementations sur les bandes **ISM (Industrial, Scientific and Medical)** :

* **Fréquences de transmission par défaut** :
  * **Bande 868 MHz** : `869.525 MHz` (Plage ISM/ICM autorisée : **863.00 – 870.00 MHz**)
  * **Bande 433 MHz** : `433.050 MHz` (Plage ISM/ICM autorisée : **433.05 – 434.79 MHz**)
* **Sécurité de Fréquence (Bandes ICM)** : Le code intègre des vérifications strictes à la compilation (`static_assert`). Si la fréquence configurée sort des plages autorisées ci-dessus, la compilation échouera automatiquement avec un message d'erreur explicite.
* **Spreading Factor (SF)** : `8`
* **Bande Passante (BW)** : `250 kHz`
* **Puissance d'émission** : `17 dBm`
* **Limitation de courant** : `120 mA`
* **CRC matériel LoRa** : Activé (`true`)

---

## 📊 Format détaillé des trames LoRa (Émises dans l'air)

Chaque paquet LoRa envoyé par l'émetteur contient un **en-tête LoRa de 3 octets** suivi par les données utiles (**Payload**) encodées en **Little-Endian** (standard ESP32).

### 1. Trame APID 1 (0x01) — GPS & Diagnostic Système
* **Fréquence d'émission** : Toutes les 5 secondes (0.2 Hz).
* **Taille totale de la trame** : 24 octets (3 octets en-tête + 21 octets payload).

| Offset (octets) | Taille (octets) | Type | Nom du champ | Unité / Format | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **0** | 1 | `uint8_t` | `SSID_NUM` | - | Identifiant de la mission (configuré à `99` pour `FX99`). |
| **1** | 1 | `uint8_t` | `APID` | - | Identifiant du paquet (configuré à `1` / `0x01`). |
| **2** | 1 | `uint8_t` | `SSID_TYPE` | - | Type de mission (configuré à `0` / `0x00` pour `FX`). |
| **3** | 4 | `uint32_t` | `timestamp` | Millisecondes (ms) | Temps écoulé depuis le démarrage de la carte. |
| **7** | 4 | `float` | `latitude` | Degrés Décimaux (DD) | Latitude GPS simulée (ex : `43.232400` pour Tarbes). |
| **11** | 4 | `float` | `longitude` | Degrés Décimaux (DD) | Longitude GPS simulée (ex : `0.078200` pour Tarbes). |
| **15** | 4 | `float` | `altitude` | Mètres (m) | Altitude GPS simulée au-dessus du niveau moyen de la mer. |
| **19** | 1 | `uint8_t` | `satellites` | - | Nombre de satellites GPS détectés (simulé à `10`). |
| **20** | 4 | `float` | `battery_voltage` | Volts (V) | Tension électrique d'alimentation de la carte. |

---

### 2. Trame APID 2 (0x02) — Centrale Inertielle (IMU) & Baromètre
* **Fréquence d'émission** : Toutes les 1 seconde (1 Hz).
* **Taille totale de la trame** : 44 octets (3 octets en-tête + 41 octets payload).

| Offset (octets) | Taille (octets) | Type | Nom du champ | Unité / Format | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **0** | 1 | `uint8_t` | `SSID_NUM` | - | Identifiant de la mission (configuré à `99` pour `FX99`). |
| **1** | 1 | `uint8_t` | `APID` | - | Identifiant du paquet (configuré à `2` / `0x02`). |
| **2** | 1 | `uint8_t` | `SSID_TYPE` | - | Type de mission (configuré à `0` / `0x00` pour `FX`). |
| **3** | 4 | `uint32_t` | `timestamp` | Millisecondes (ms) | Temps écoulé depuis le démarrage de la carte. |
| **7** | 4 | `float` | `baro_altitude` | Mètres (m) | Altitude barométrique calculée. |
| **11** | 4 | `float` | `pressure` | Pascals (Pa) | Pression atmosphérique absolue. |
| **15** | 4 | `float` | `temperature` | Degrés Celsius (°C) | Température mesurée. |
| **19** | 4 | `float` | `accel_x` | G-Force (G) | Accélération linéaire sur l'axe transverse X (1 G ≈ 9.81 m/s²). |
| **23** | 4 | `float` | `accel_y` | G-Force (G) | Accélération linéaire sur l'axe transverse Y. |
| **27** | 4 | `float` | `accel_z` | G-Force (G) | Accélération linéaire sur l'axe vertical Z (inclut la gravité). |
| **31** | 4 | `float` | `gyro_x` | Degrés par seconde (°/s) | Vitesse de rotation angulaire sur l'axe X (tangage). |
| **35** | 4 | `float` | `gyro_y` | Degrés par seconde (°/s) | Vitesse de rotation angulaire sur l'axe Y (lacet). |
| **39** | 4 | `float` | `gyro_z` | Degrés par seconde (°/s) | Vitesse de rotation angulaire sur l'axe Z (roulis). |
| **43** | 1 | `uint8_t` | `flight_state` | Code d'état | Phase actuelle du vol (`0`: IDLE, `1`: PROPULSION, `2`: APOGEE, `3`: DESCENT, `4`: LANDED). |

---

## 🚀 Fonctionnement du simulateur de vol

Lors de la mise sous tension de la carte, une simulation physique démarre en tâche de fond dans la boucle `loop()` :
1. **IDLE (0 à 15s)** : La fusée attend sur la rampe. L'altitude reste stable à 150m. L'accélération verticale (Z) indique 1.0G (gravité).
2. **PROPULSION (15s à 18.5s)** : Le moteur s'allume. L'accélération verticale grimpe à 6.0G. La vitesse et l'altitude augmentent rapidement. Une rotation de stabilisation en roulis (Z) de 360°/s est générée.
3. **APOGEE/FREE (18.5s jusqu'à inversion de vitesse)** : Le moteur s'éteint. La fusée subit la gravité et la traînée de l'air. L'accélération détectée tombe à 0G (chute libre). La fusée atteint son apogée (environ 850m de gain, soit 1000m d'altitude) puis commence à basculer.
4. **DESCENT (D'apogée jusqu'au sol)** : Le parachute s'ouvre. La fusée descend à une vitesse stabilisée d'environ -8 m/s. Le vent fait dériver doucement les coordonnées GPS. L'accélération Z remonte à 1G.
5. **LANDED (Fusée au sol)** : La fusée touche le sol à son altitude de départ (150m) et s'immobilise. Les transmissions LoRa continuent d'envoyer des balises de localisation fixes.

---

## 🖥️ Rendu OLED local (TTGO T3)

L'écran OLED SSD1306 affiche les informations de fonctionnement avec une esthétique épurée :
* **En-tête** : `NECTAR SENDER` souligné.
* **Barre de progression verticale (droite)** : Représente graphiquement l'altitude de la fusée (de 150m au sol à 1000m d'apogée).
* **Indicateur de batterie** : Dessin dynamique de pile indiquant la charge restante de l'accu (3.5V à 4.2V).
* **Balise de transmission (Tx)** : Un point circulaire en haut à droite s'allume en noir plein au moment exact de chaque émission radio.
* **Télémétrie** : Affiche l'état textuel de vol, l'altitude (m), la vitesse (m/s) et le compteur total de paquets émis.

---

## 🛠️ Compilation et Téléversement

Le projet est configuré sous **PlatformIO** avec deux environnements distincts définissant la bande de fréquence de transmission.

### 1. Bande 868 MHz (Fréquence réelle : 869.525 MHz)

Pour compiler :
```bash
pio run -e ttgo-lora32-v21-868
```

Pour compiler et flasher par USB :
```bash
pio run -e ttgo-lora32-v21-868 -t upload
```

### 2. Bande 433 MHz (Fréquence réelle : 433.050 MHz)

Pour compiler :
```bash
pio run -e ttgo-lora32-v21-433
```

Pour compiler et flasher par USB :
```bash
pio run -e ttgo-lora32-v21-433 -t upload
```

### Moniteur Série (USB)

Pour visualiser la console de diagnostic en temps réel (115200 bauds) :
```bash
pio device monitor
```

