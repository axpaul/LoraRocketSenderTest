# Nectar LoRa Transmitter Simulator (TTGO T3 V1.6.1)

Ce projet configure une carte **LilyGO TTGO T3 V1.6.1** (ESP32 + SX1276 + OLED + SD) en émetteur de test dynamique de télémétrie. Il simule en temps réel le comportement physique d'une fusée expérimentale durant son vol et transmet périodiquement des trames LoRa formatées selon les spécifications attendues par la station au sol compatible avec le logiciel de traitement de données **NectarMC**.

---

## 📡 Paramètres de configuration RF LoRa

Pour que la liaison s'établisse avec le récepteur Nectar, l'émetteur utilise les paramètres radio suivants :
* **Fréquence** : `869.525 MHz`
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

| Offset (octets) | Taille (octets) | Type | Nom du champ | Description |
| :--- | :--- | :--- | :--- | :--- |
| **0** | 1 | `uint8_t` | `SSID_NUM` | Identifiant de la mission (configuré à `99` pour `FX99`). |
| **1** | 1 | `uint8_t` | `APID` | Identifiant du paquet (configuré à `1` / `0x01`). |
| **2** | 1 | `uint8_t` | `SSID_TYPE` | Type de mission (configuré à `0` / `0x00` pour `FX`). |
| **3** | 4 | `uint32_t` | `timestamp` | Temps écoulé depuis le démarrage en millisecondes. |
| **7** | 4 | `float` | `latitude` | Latitude GPS simulée en degrés (ex : `43.2324` pour Tarbes). |
| **11** | 4 | `float` | `longitude` | Longitude GPS simulée en degrés (ex : `0.0782` pour Tarbes). |
| **15** | 4 | `float` | `altitude` | Altitude GPS simulée en mètres (calquée sur l'altitude physique). |
| **19** | 1 | `uint8_t` | `satellites` | Nombre de satellites GPS accrochés (simulé à `10`). |
| **20** | 4 | `float` | `battery_voltage` | Tension de la batterie d'alimentation de la fusée en Volts. |

---

### 2. Trame APID 2 (0x02) — Centrale Inertielle (IMU) & Baromètre
* **Fréquence d'émission** : Toutes les 1 seconde (1 Hz).
* **Taille totale de la trame** : 44 octets (3 octets en-tête + 41 octets payload).

| Offset (octets) | Taille (octets) | Type | Nom du champ | Description |
| :--- | :--- | :--- | :--- | :--- |
| **0** | 1 | `uint8_t` | `SSID_NUM` | Identifiant de la mission (configuré à `99` pour `FX99`). |
| **1** | 1 | `uint8_t` | `APID` | Identifiant du paquet (configuré à `2` / `0x02`). |
| **2** | 1 | `uint8_t` | `SSID_TYPE` | Type de mission (configuré à `0` / `0x00` pour `FX`). |
| **3** | 4 | `uint32_t` | `timestamp` | Temps écoulé depuis le démarrage en millisecondes. |
| **7** | 4 | `float` | `baro_altitude` | Altitude barométrique simulée en mètres. |
| **11** | 4 | `float` | `pressure` | Pression atmosphérique simulée en Pascals (formule barométrique). |
| **15** | 4 | `float` | `temperature` | Température ambiante simulée en °C (décroissante avec l'altitude). |
| **19** | 4 | `float` | `accel_x` | Accélération sur l'axe X (transverse) en G. |
| **23** | 4 | `float` | `accel_y` | Accélération sur l'axe Y (transverse) en G. |
| **27** | 4 | `float` | `accel_z` | Accélération sur l'axe Z (vertical/poussée) en G (inclut la gravité). |
| **31** | 4 | `float` | `gyro_x` | Vitesse angulaire sur l'axe X en °/s. |
| **35** | 4 | `float` | `gyro_y` | Vitesse angulaire sur l'axe Y en °/s. |
| **39** | 4 | `float` | `gyro_z` | Vitesse angulaire sur l'axe Z (taux de roulis) en °/s. |
| **43** | 1 | `uint8_t` | `flight_state` | Phase actuelle du vol :<br>`0` = IDLE (Attente au sol)<br>`1` = PROPULSION (Moteur allumé)<br>`2` = APOGEE/FREE (Vol balistique, apesanteur)<br>`3` = DESCENT (Sous parachute à vitesse terminale)<br>`4` = LANDED (Fusée posée) |

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

Le projet est configuré sous **PlatformIO**.

Pour compiler :
```bash
pio run
```

Pour compiler et flasher la carte en USB :
```bash
pio run -t upload
```

Pour visualiser la console série de diagnostic (115200 bauds) :
```bash
pio device monitor
```
