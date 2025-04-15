import paho.mqtt.client as mqtt
import base64
import os
import json

# MQTT Configuration
BROKER = "app.coreiot.io"
PORT = 1883
ACCESS_TOKEN = "WytjI10Ucky6YlLErFmv"
TOPIC_REQUEST = "v1/devices/me/firmware/request"
TOPIC_RESPONSE = "v1/devices/me/firmware/response"
TOPIC_STATUS = "v1/devices/me/attributes"

# Firmware Configuration
FIRMWARE_PATH = "d:/IOT/lab3/lab2_rtos/firmware.bin"
CHUNK_SIZE = 1024

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print(f"Connected to {BROKER}")
        client.subscribe(TOPIC_REQUEST)
    else:
        print(f"Connection failed with code {rc}")

def read_firmware():
    if not os.path.exists(FIRMWARE_PATH):
        raise FileNotFoundError(f"Firmware file not found at {FIRMWARE_PATH}")
    with open(FIRMWARE_PATH, "rb") as f:
        return f.read()

def send_firmware_chunks(client, firmware):
    total_chunks = len(firmware) // CHUNK_SIZE + (1 if len(firmware) % CHUNK_SIZE else 0)
    
    for chunk_index in range(total_chunks):
        start = chunk_index * CHUNK_SIZE
        end = start + CHUNK_SIZE
        chunk_data = firmware[start:end]
        chunk_base64 = base64.b64encode(chunk_data).decode()

        payload = {
            "chunk_index": chunk_index,
            "total_chunks": total_chunks,
            "data": chunk_base64
        }
        client.publish(TOPIC_RESPONSE, json.dumps(payload))
        print(f"Sent chunk {chunk_index+1}/{total_chunks}")

def on_message(client, userdata, msg):
    if msg.topic == TOPIC_REQUEST:
        print("Received firmware request")
        try:
            firmware = read_firmware()
            send_firmware_chunks(client, firmware)
        except Exception as e:
            print(f"Error: {e}")

def main():
    # Create client with MQTT v5 protocol
    client = mqtt.Client(protocol=mqtt.MQTTv5)
    
    # Set callbacks
    client.on_connect = on_connect
    client.on_message = on_message
    client.username_pw_set(ACCESS_TOKEN)

    try:
        client.connect(BROKER, PORT, 60)
        print(f"Connecting to {BROKER}...")
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nDisconnecting...")
        client.disconnect()
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()