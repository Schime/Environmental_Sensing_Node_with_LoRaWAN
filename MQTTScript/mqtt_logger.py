import paho.mqtt.client as mqtt
import json
import base64
import csv
from datetime import datetime

# --- Configuration ---
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC = "application/+/device/+/event/up"
CSV_FILE = "lora_sensor_data.csv"

# --- CSV File Setup ---
CSV_HEADERS = [
    'timestamp', 'device_name', 'dev_eui', 'temperature', 'humidity',
    'f1_415nm', 'f2_445nm', 'f3_480nm', 'f4_515nm',
    'f5_555nm', 'f6_590nm', 'f7_630nm', 'f8_680nm'
]

try:
    with open(CSV_FILE, 'x', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(CSV_HEADERS)
except FileExistsError:
    pass

# --- CayenneLPP Payload Decoder ---
# This function correctly decodes the raw bytes from your node.
def decode_cayenne_lpp(payload_base64):
    """
    Decodes a base64 CayenneLPP payload into a dictionary.
    """
    data = {}
    payload = base64.b64decode(payload_base64)
    i = 0
    while i < len(payload):
        channel = payload[i]
        type = payload[i+1]
        
        # Temperature: 0x67, 2 bytes, 0.1°C signed
        if type == 0x67:
            temp_val = int.from_bytes(payload[i+2:i+4], 'big', signed=True)
            data[f'temperature_{channel}'] = temp_val / 10.0
            i += 4
        # Humidity: 0x68, 1 byte, 0.5% unsigned
        elif type == 0x68:
            hum_val = payload[i+2]
            data[f'humidity_{channel}'] = hum_val / 2.0
            i += 3
        # Analog Input: 0x02, 2 bytes. We treat as UNSIGNED to get the raw sensor value.
        elif type == 0x02:
            analog_val = int.from_bytes(payload[i+2:i+4], 'big', signed=False)
            data[f'analog_{channel}'] = analog_val # This gives the raw positive integer
            i += 4
        else:
            print(f"Skipping unknown Cayenne type {type} on channel {channel}")
            # To prevent infinite loops, we need a way to advance 'i'
            # This is a basic guess; a real implementation would need a type-length map
            i += 2 
    return data


# --- MQTT Callbacks ---
def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print(f"Connected to MQTT Broker at {MQTT_BROKER}")
        client.subscribe(MQTT_TOPIC)
        print(f"Subscribed to topic: {MQTT_TOPIC}")
    else:
        print(f"Failed to connect, return code {rc}\n")

def on_message(client, userdata, msg):
    print(f"Message received on topic {msg.topic}")
    try:
        chirpstack_data = json.loads(msg.payload.decode())

        payload_b64 = chirpstack_data.get('data')

        if not payload_b64:
            print("No 'data' field found in the message from ChirpStack. Skipping.")
            return

        decoded_payload = decode_cayenne_lpp(payload_b64)
        print(f"Correctly decoded payload: {decoded_payload}")
        
        dev_eui = chirpstack_data['deviceInfo']['devEui']
        device_name = chirpstack_data['deviceInfo']['deviceName']
        
        row_data = {
            'timestamp': datetime.now().isoformat(),
            'device_name': device_name,
            'dev_eui': dev_eui,
            'temperature': decoded_payload.get('temperature_1', ''),
            'humidity': decoded_payload.get('humidity_2', ''),
            'f1_415nm': decoded_payload.get('analog_3', ''),
            'f2_445nm': decoded_payload.get('analog_4', ''),
            'f3_480nm': decoded_payload.get('analog_5', ''),
            'f4_515nm': decoded_payload.get('analog_6', ''),
            'f5_555nm': decoded_payload.get('analog_7', ''),
            'f6_590nm': decoded_payload.get('analog_8', ''),
            'f7_630nm': decoded_payload.get('analog_9', ''),
            'f8_680nm': decoded_payload.get('analog_10', ''),
        }

        with open(CSV_FILE, 'a', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=CSV_HEADERS)
            writer.writerow(row_data)
        print(f"Data for {device_name} successfully appended to {CSV_FILE}")

    except Exception as e:
        print(f"An error occurred while processing message: {e}")
        print(f"Raw payload for debugging: {msg.payload.decode()}")

# Use the newer Callback API version to avoid the deprecation warning
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.on_message = on_message

client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()