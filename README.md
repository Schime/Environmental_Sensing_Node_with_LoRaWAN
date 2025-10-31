# 🌱 Environmental Sensing Node with LoRaWAN

This project collects environmental and spectral data using a **SeeedStudio XIAO ESP32-S3**, a **DHT22 temperature & humidity sensor**, and an **Adafruit AS7341 spectral sensor**. The data is transmitted via **LoRaWAN** (SX1262 module) to a **ChirpStack server**, then decoded using **MQTT** and saved in a CSV file for logging and analysis.

---

## 📦 Components Used

| Component             | Description                            |
|----------------------|----------------------------------------|
| [Seeed XIAO ESP32-S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)  | Microcontroller with Wi-Fi/BLE/USB-C   |
| [Adafruit AS7341](https://cdn-learn.adafruit.com/downloads/pdf/adafruit-as7341-10-channel-light-color-sensor-breakout.pdf)      | 10-channel Spectral Sensor             |
| [LAMBDA62 (SX1262)](https://www.farnell.com/datasheets/2720463.pdf)    | LoRa Transceiver (868/915 MHz)         |
| [WisGate RAK7268](https://docs.rakwireless.com/product-categories/wisgate/rak7268/quickstart/) | Indoor LoRaWAN Gateway                 |
| [DHT22](https://cdn.sparkfun.com/assets/f/7/d/9/c/DHT22.pdf)                | Temperature and Humidity Sensor        |
| 3 × AA Batteries     | Power source                           |
| 10kΩ Resistor        | Pull-up resistor for DHT22             |

---


## 🔌 Wiring Diagram

| Device             | XIAO ESP32-S3 Pin |
|--------------------|-------------------|
| **DHT22**          | D7 (signal)       |
|                    | 3V3 (VCC)         |
|                    | GND               |
| **10kΩ Resistor**  | Between D7 and 3V3 |
| **AS7341**         | SDA → D5          |
|                    | SCL → D6          |
|                    | VIN → 3V3         |
|                    | GND               |
| **LAMBDA62 LoRa**  | NSS → D3          |
|                    | RST → D2          |
|                    | SCK → D8          |
|                    | MISO → D9         |
|                    | MOSI → D10        |
|                    | DIO1 → D0         |
|                    | BUSY → D1         |
|                    | 3V3 & GND         |

---

## 🔧 Arduino Setup

1. **Install Arduino IDE** (v1.8+ or Arduino 2.0)
2. **Install ESP32 Board Support**  
   - Go to **Preferences** → Add this URL in *Additional Board URLs*:  
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   - Then install **ESP32 by Espressif** in the Board Manager.
3. **Install Required Libraries**:
   - LoRaWan-Arduino (by IBM)
   - SX126x-Arduino (by Bernd Giesecke)
   - CayenneLPP (by Electronic Cats)
   - Adafruit AS7341 (by Adafruit)
   - DHT sensor library (by Adafruit)
   - Adafruit Unified Sensor (by Adafruit)
4. **Board Selection**:
   - Board: Seeed XIAO ESP32-S3
   - Partition Scheme: Default
5. **Configure LoRaWAN Settings**:
   - Go to **MCCI_LoRaWAN_LMIC_library > project_config > lmic_project_config.h**
   - Edit config file to match EU frequency and SX1262 radio:

     ```cpp
     #define CFG_eu868 1
     #define CFG_sx1262_radio 1
     ```

---
## 🛰️ Gateway Configuration (WisGate RAK7268)

Settings used for this project:

- **Channel Plan**:
  - Region: `EU863-870`
  - *Conform to LoRaWAN*: ✅ Enabled  
  - *LoRaWAN Public*: ✅ Enabled

- **LoRa Network**:
  - Mode: `Packet Forwarder`

- **Packet Forwarder Settings**:
  - Protocol: `Semtech UDP GWMPP Protocol`
  - Server Address: [your local server address]
  - Server Port Up: `1700`
  - Server Port Down: `1700`

 - **DISCLAIMER**:
    - The current setup works if device running ChirpStack server is connected directly to RAK gateway
-    **Steps for connecting gateway to your home Wi-Fi router:**
   1. Connect gateway to your home router via Ethernet cable (preferrably for stable connection)
   2. Connect your laptop to the same router as your gateway
   3. Use command ```ipconfig``` or ```hostname -I``` to locate your device (or use [Angry IP Scanner](https://angryip.org/) to locate connected devices)
   4. Now that you know your laptop's IP address on home network, connect back to your gateway and then go to **LoRa Network --> Network Settings --> Server Address** and put your laptop's IP address (the one which your home router assigns)
   5. Click *Save & Apply*
   6. Connect back to your home router and run the code
---

## 📡 ChirpStack LoRaWAN Server With Docker
The MQTT broker (e.g., **Mosquitto**) used in this project is run inside a Docker container for ease of deployment and management.

**Install Docker:** The easiest way to run ChirpStack and its dependencies (PostgreSQL database, Redis cache, MQTT broker) is with Docker. Install [Docker Desktop](https://docs.docker.com/engine/install/) on your laptop.

🔧 After installing Docker, be sure to complete the “Post-installation steps” to ensure proper permissions and setup.

Ensure this container is running before starting the Python MQTT logger script.

**Download and Run ChirpStack:** Use the official chirpstack-docker repository.
   ```bash
   # Clone the repository
   git clone https://github.com/chirpstack/chirpstack-docker.git
   cd chirpstack-docker

   # Start everything
   # This command will download and run ChirpStack, its Gateway Bridge, a Mosquitto MQTT Broker, and databases.
   docker-compose up -d
   ```

1. **Provision your device** on ChirpStack:
   - Register the DevEUI, AppEUI, and AppKey.
   - Use OTAA mode.
   - Match with your Arduino sketch values:
     

```cpp
     uint8_t DEVEUI[8] = {...};
     uint8_t APPEUI[8] = {...};
     uint8_t APPKEY[16] = {...};
```


1. **Install Dependencies**:

```bash
pip install paho-mqtt
```



2. **Run the script**:

```bash
python mqtt_lora_logger.py
```


Make sure the MQTT broker (like Mosquitto) is running and ChirpStack is configured to publish uplink events to it.

---
## 🐍 MQTT Decoding Script (Python)

Once data is received by ChirpStack, it is sent to MQTT broker. This Python script:

- Connects to MQTT (localhost:1883)
- Listens to topic application/+/device/+/event/up
- Decodes **CayenneLPP** payload (base64)
- Extracts:
  - Temperature (channel 1)
  - Humidity (channel 2)
  - AS7341 F1-F8 readings (channels 3–10)
- Appends it into lora_sensor_data.csv
---


## 📜 Arduino Sketch Overview

The firmware does:

- Initializes LoRa radio, AS7341, and DHT22 sensors
- Joins LoRaWAN network (OTAA)
- Reads temperature, humidity, and spectral data
- Encodes them using **CayenneLPP**
- Sends the data to LoRaWAN server every 15 minutes
- Goes into deep sleep to save power

---

## 📁 File Outputs
- lora_sensor_data.csv will contain rows like:

```bash
timestamp,device_name,dev_eui,temperature,humidity,f1_415nm,f2_445nm,...f8_680nm
2025-07-11T10:12:45,EnvNode,ABC12345,24.6,45.5,235,240,...199
```


---
### 🧠 Notes & Tips
- **Deep Sleep** saves battery by putting the ESP32-S3 to sleep for 15 minutes.
- **CayenneLPP** helps pack multiple sensor readings into a compact LoRa message.
- Make sure antennas are connected before powering LoRa module.
- If sensor readings are failing:
  - Double-check wiring (especially pull-up resistor on DHT)
  - Ensure AS7341 is properly detected on I2C bus
