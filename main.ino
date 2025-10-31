#include <LoRaWan-Arduino.h>
#include <SX126x-Arduino.h>
#include <CayenneLPP.h>
#include <Adafruit_AS7341.h>
#include <DHT.h>
#include <Wire.h>
#include <esp_sleep.h>


// Enter your device EUI
uint8_t DEVEUI[8] = {
  0xA0, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01
};


uint8_t APPEUI[8] = {
  0x05, 0x30, 0x94, 0x25,
  0x39, 0x70, 0x23, 0xD5
};

uint8_t APPKEY[16] = {
  0x1C, 0x6C, 0xDB, 0x7A,
  0x19, 0xE5, 0x70, 0x4C,
  0xDA, 0xF1, 0xD3, 0xEA,
  0x28, 0x94, 0xB7, 0x22
};


// This is the configuration for TX set to HIGH (3V3) and RX set to LOW (GND)

hw_config configuration = {
  .CHIP_TYPE = 2,
  .PIN_LORA_RESET = D2,
  .PIN_LORA_NSS = D3,
  .PIN_LORA_SCLK = D8,
  .PIN_LORA_MISO = D9,
  .PIN_LORA_DIO_1 = D0,
  .PIN_LORA_BUSY = D1,
  .PIN_LORA_MOSI = D10,
  .RADIO_TXEN = -1,
  .RADIO_RXEN = -1,
  .USE_DIO2_ANT_SWITCH = false,
  .USE_DIO3_TCXO = false,
  .USE_DIO3_ANT_SWITCH = false,
  .USE_LDO = true,
  .USE_RXEN_ANT_PWR = false,
};


#define SCHED_MAX_EVENT_DATA_SIZE APP_TIMER_SCHED_EVENT_DATA_SIZE  // Maximum size of scheduler events
#define SCHED_QUEUE_SIZE 60                                        // Maximum number of events in the scheduler queue

#define LORAWAN_APP_DATA_BUFF_SIZE 256  // Size of the data to be transmitted
#define LORAWAN_APP_TX_DUTYCYCLE 5000   // Defines the application data transmission duty cycle. 10s, value in [ms]
#define APP_TX_DUTYCYCLE_RND 1000       // Defines a random delay for application data transmission duty cycle. 1s, value in [ms]
#define JOINREQ_NBTRIALS 3              // Number of trials for the join request

// DHT sensor initialization
#define DHTPIN D7
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Adafruit AS7341 initialization
Adafruit_AS7341 as7341;

// Sleep settings
#define TIME_TO_SLEEP  15 * 60               // 15 minutes in seconds
#define uS_TO_S_FACTOR 1000000ULL


static uint8_t m_lora_app_data_buffer[LORAWAN_APP_DATA_MAX_SIZE];
static lmh_app_data_t m_lora_app_data = { m_lora_app_data_buffer, 0, 0, 0, 0 };
static lmh_param_t lora_param_init = { LORAWAN_ADR_OFF, DR_3, LORAWAN_PUBLIC_NETWORK, PHY_NB_JOIN_TRIALS, LORAWAN_DEFAULT_TX_POWER, LORAWAN_DUTYCYCLE_OFF };

CayenneLPP lpp(51);


// Foward declaration
/** LoRaWAN callback when join network finished */
static void lorawan_has_joined_handler(void);
/** LoRaWAN callback when join network failed */
static void lorawan_join_fail_handler(void);
/** LoRaWAN callback when data arrived */
static void lorawan_rx_handler(lmh_app_data_t *app_data);
/** LoRaWAN callback after class change request finished */
static void lorawan_confirm_class_handler(DeviceClass_t Class);
/** LoRaWAN callback after class change request finished */
static void lorawan_unconfirm_tx_finished(void);
/** LoRaWAN callback after class change request finished */
static void lorawan_confirm_tx_finished(bool result);
/** LoRaWAN Function to send a package */
static void send_lora_frame(void);
static uint32_t timers_init(void);



void lorawan_rx_handler(lmh_app_data_t *appdata) {
  Serial.printf("Received data on port %d, RSSI: %d, SNR: %d\n", appdata->port, appdata->rssi, appdata->snr);
}

void lorawan_has_joined_handler(void) {
  Serial.println("Successfully joined the LoRa network!");
  send_lora_frame();
}


void lorawan_confirm_class_handler(DeviceClass_t Class) {
  Serial.print("Switched to class ");
  Serial.println((char)('A' + Class));
}

void onJoinFailed(void) {
  Serial.println("Join failed. Retrying...");
}

void lorawan_unconfirm_tx_finished(void) {
  Serial.println("Unconfirmed TX finished.");
  Serial.printf("Entering deep sleep for %d minutes...\n\n", TIME_TO_SLEEP / 60);
  Serial.flush();   // Flushing serial buffer before sleeping
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);    // Configure the timer wakep source
  esp_deep_sleep_start();
}

void onConfirmedTxResult(bool result) {
  Serial.print("Confirmed TX ");
  Serial.println(result ? "succeeded." : "failed.");
}


// Creating and sending UPLINK message containing the sensor data
static void send_lora_frame(void) {
  if (lmh_join_status_get() != LMH_SET) {
    Serial.println("Not joined, skipping send.");
    return;
  }

  // --- Read DHT22 Sensor ---
  Serial.println("Reading DHT22 sensor...");
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature(); // Read temperature in Celsius
  float heatIndexCelsius = dht.computeHeatIndex(temperature, humidity, false);

  // Check if any reads failed and exit early (to try again later).
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    Serial.printf("Temperature: %.2f *C, Humidity: %.2f %%\n", temperature, humidity);
  }
  
  // --- Read AS7341 Sensor ---
  Serial.println("Reading AS7341 spectral sensor...");
  uint16_t readings[12]; // Array to store all channel readings
  
  // The readAllChannels function is a blocking call that handles the measurement
  if (!as7341.readAllChannels(readings)) {
    Serial.println("Error reading from AS7341!");
  } else {
    Serial.println("AS7341 Readings:");
    Serial.printf(" F1 (415nm): %d\n", readings[0]);
    Serial.printf(" F2 (445nm): %d\n", readings[1]);
    Serial.printf(" F3 (480nm): %d\n", readings[2]);
    Serial.printf(" F4 (515nm): %d\n", readings[3]);
    Serial.printf(" F5 (555nm): %d\n", readings[4]);
    Serial.printf(" F6 (590nm): %d\n", readings[5]);
    Serial.printf(" F7 (630nm): %d\n", readings[6]);
    Serial.printf(" F8 (680nm): %d\n", readings[7]);
  }
  
  // --- 3. Prepare CayenneLPP Payload ---
  lpp.reset();
  
  // Add DHT22 data if valid
  if (!isnan(temperature) && !isnan(humidity)) {
    lpp.addTemperature(1, temperature);
    lpp.addRelativeHumidity(2, humidity);
  }

  // Add AS7341 data (8 channels). We use Analog Input for generic integer values.
  // LPP Channels 3 through 10 will be used for AS7341 F1 through F8.
  for(int i = 0; i < 8; i++) {
    // LPP channel number is i + 3
    lpp.addAnalogInput(i + 3, readings[i]);
  }

  Serial.printf("LPP payload size: %d bytes\n", lpp.getSize());

  // --- 4. Prepare and Send LoRaWAN Packet ---
  memset(m_lora_app_data.buffer, 0, LORAWAN_APP_DATA_BUFF_SIZE);
  memcpy(m_lora_app_data.buffer, lpp.getBuffer(), lpp.getSize());
  m_lora_app_data.buffsize = lpp.getSize();
  m_lora_app_data.port = 10; // Use a common port for CayenneLPP data

  lmh_error_status error = lmh_send(&m_lora_app_data, LMH_UNCONFIRMED_MSG);
  if (error == LMH_SUCCESS) {
    Serial.printf("Successfully queued %d bytes to be sent\n", m_lora_app_data.buffsize);
  } else {
    Serial.printf("Error sending data, code: %d\n", error);
  }
}


static lmh_callback_t lora_callbacks = {
  .BoardGetBatteryLevel = BoardGetBatteryLevel,
  .BoardGetUniqueId = BoardGetUniqueId,
  .BoardGetRandomSeed = BoardGetRandomSeed,
  .lmh_RxData = lorawan_rx_handler,
  .lmh_has_joined = lorawan_has_joined_handler,
  .lmh_ConfirmClass = lorawan_confirm_class_handler,
  .lmh_has_joined_failed = onJoinFailed,
  .lmh_unconf_finished = lorawan_unconfirm_tx_finished,
};




void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("====================================="));
  Serial.println(F("XIAO ESP32-S3 LoRaWAN Node with SX1262"));
  Serial.println(F("====================================="));

  dht.begin();

  Wire.begin(5, 6); // SDA on GPIO5, SCL on GPIO6
  if (!as7341.begin()) {
    Serial.println("Could not find AS7341 sensor, check wiring!");
    while (1) delay(10);
  }

  as7341.setATIME(100);
  as7341.setASTEP(999);
  as7341.setGain(AS7341_GAIN_2X);
  Serial.println("Sensors initialized.");


  // Initialize LoRa hardware
  if (lora_hardware_init(configuration) != 0) {
    Serial.println(F("FAILED to initialize radio hardware!"));
    while (1)
      ;  // Stop forever
  }

  lmh_init(&lora_callbacks, lora_param_init, true, CLASS_A, LORAMAC_REGION_EU868);
  lmh_setDevEui(DEVEUI);
  lmh_setAppEui(APPEUI);
  lmh_setAppKey(APPKEY);

  // Start the join process
  Serial.println(F("Sending LoRaWAN join request..."));
  lmh_join();
}

void loop() {
}
