// #include <WiFi.h>
// #include <PubSubClient.h>
// #include <ArduinoJson.h>
// #include <vector>
// #include <Update.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <freertos/queue.h>
// #include "DHT.h"

// // WiFi
// const char* ssid = "TIEN TRUNG";         
// const char* password = "20121978"; 

// // ThingsBoard
// const char* thingsboardServer = "app.coreiot.io"; 
// const int thingsboardPort = 1883;
// const char* accessToken = "IDgfLD6sNvU2n0IaTIQA";  

// #define DHTPIN 8
// #define DHTTYPE DHT11 
// DHT dht(DHTPIN, DHTTYPE);

// #define ledPin 48

// WiFiClient espClient;
// PubSubClient client(espClient);

// const char* ledStateControlKey = "ledState";
// volatile bool ledState = false;
// QueueHandle_t ledStateQueue;

// // OTA
// bool otaInProgress = false;
// int fw_size = 0;
// int chunk_size = 0;
// int chunks_received = 0;
// String fw_title = "", fw_version = "";
// String fw_checksum = "";
// String fw_algo = "sha256";
// int offset = 0;

// // Task declarations
// void wifiTask(void *pvParameters);
// void mqttTask(void *pvParameters);
// void ledControlTask(void *pvParameters);
// void dht11Task(void *pvParameters);
// void otaTask(void *pvParameters);
// void connectWifi();
// void connectThingsBoard();
// void callback(char* topic, byte* payload, unsigned int length);

// // ===== Base64 decode =====
// int b64decode(char c) {
//     if (c >= 'A' && c <= 'Z') return c - 'A';
//     if (c >= 'a' && c <= 'z') return c - 'a' + 26;
//     if (c >= '0' && c <= '9') return c - '0' + 52;
//     if (c == '+') return 62;
//     if (c == '/') return 63;
//     return -1;
// }

// size_t decode_base64(const char *input, uint8_t *output, size_t output_len) {
//     size_t i = 0;
//     static int buffer = 0, bits = 0;
//     while (*input && output_len) {
//         int val = b64decode(*input++);
//         if (val < 0) continue;
//         buffer = (buffer << 6) | val;
//         bits += 6;
//         if (bits >= 8) {
//             bits -= 8;
//             *output++ = (buffer >> bits) & 0xFF;
//             output_len--;
//             i++;
//         }
//     }
//     return i;
// }

// // ===== Callback =====
// void callback(char* topic, byte* payload, unsigned int length) {
//     Serial.printf("Message from topic: %s\n", topic);

//     DynamicJsonDocument doc(2048);
//     deserializeJson(doc, payload, length);

//     if (String(topic).indexOf("attributes") >= 0) {
//         if (doc.containsKey("fw_title")) {
//             fw_title = doc["fw_title"].as<String>();
//             fw_version = doc["fw_version"].as<String>();
//             fw_checksum = doc["fw_checksum"].as<String>();
//             fw_algo = doc["fw_checksum_algorithm"].as<String>();
//             fw_size = doc["fw_size"];
//             chunk_size = doc["fw_chunk_size"];
//             offset = 0;
//             chunks_received = 0;

//             otaInProgress = true;
//             client.publish("v1/devices/me/attributes", "{\"fw_state\":\"QUEUED\"}");
//         }

//         if (doc.containsKey("ledState")) {
//             String ledStateStr = doc["ledState"].as<String>();
//             bool newLedState = (ledStateStr == "ON");
//             xQueueSend(ledStateQueue, &newLedState, 0);
//         }
//     } else if (String(topic).indexOf("firmware/response") >= 0 && otaInProgress) {
//         const char* b64data = doc["data"];
//         int len = strlen(b64data);
//         std::vector<uint8_t> decoded(len / 4 * 3);

//         size_t actualLen = decode_base64(b64data, decoded.data(), decoded.size());
//         if (Update.write(decoded.data(), actualLen) != actualLen) {
//             Serial.println("Update write failed!");
//             client.publish("v1/devices/me/attributes", "{\"fw_state\":\"FAILED\"}");
//             otaInProgress = false;
//             Update.abort();
//             return;
//         }

//         offset += actualLen;
//         chunks_received++;

//         if (offset >= fw_size) {
//             if (Update.end(true)) {
//                 Serial.println("OTA Update Success!");
//                 client.publish("v1/devices/me/attributes", "{\"fw_state\":\"UPDATED\"}");
//                 ESP.restart();
//             } else {
//                 Serial.println("Update end failed.");
//                 client.publish("v1/devices/me/attributes", "{\"fw_state\":\"FAILED\"}");
//                 otaInProgress = false;
//             }
//         }
//     }
// }

// void setup() {
//     Serial.begin(115200);
//     pinMode(ledPin, OUTPUT);
//     digitalWrite(ledPin, LOW);
//     dht.begin();

//     ledStateQueue = xQueueCreate(1, sizeof(bool));

//     xTaskCreate(wifiTask, "WiFi Task", 4096, NULL, 1, NULL); 
//     xTaskCreate(mqttTask, "MQTT Task", 8192, NULL, 2, NULL);
//     xTaskCreate(ledControlTask, "LED Control Task", 2048, NULL, 3, NULL); 
//     xTaskCreate(dht11Task, "DHT11 Task", 4096, NULL, 2, NULL);
//     xTaskCreate(otaTask, "OTA Task", 8192, NULL, 3, NULL);
// }

// void loop() {}

// // ===== WiFi Task =====
// void wifiTask(void *pvParameters) {
//     for (;;) {
//         if (WiFi.status() != WL_CONNECTED) {
//             connectWifi();
//         }
//         vTaskDelay(pdMS_TO_TICKS(5000));
//     }
// }

// void connectWifi() {
//     Serial.print("Connecting to WiFi...");
//     WiFi.begin(ssid, password);
//     while (WiFi.status() != WL_CONNECTED) {
//         delay(500);
//         Serial.print(".");
//     }
//     Serial.println("Connected to WiFi");
// }

// // ===== MQTT Task =====
// void mqttTask(void *pvParameters) {
//     client.setServer(thingsboardServer, thingsboardPort);
//     client.setCallback(callback);

//     for (;;) {
//         if (!client.connected()) {
//             connectThingsBoard();
//         }
//         client.loop();
//         vTaskDelay(pdMS_TO_TICKS(100));
//     }
// }

// void connectThingsBoard() {
//     while (!client.connect("ESP32Client", accessToken, nullptr)) {
//         Serial.print("Failed to connect to ThingsBoard, rc=");
//         Serial.println(client.state());
//         delay(5000);
//     }
//     Serial.println("Connected to ThingsBoard");

//     client.subscribe("v1/devices/me/attributes");
//     client.subscribe("v1/devices/me/attributes/response");
//     client.subscribe("v1/devices/me/firmware/response");

//     String payload = "{\"shared\":[\"fw_title\",\"fw_version\",\"fw_size\",\"fw_checksum\",\"fw_checksum_algorithm\",\"fw_chunk_size\",\"ledState\"]}";
//     client.publish("v1/devices/me/attributes/request/1", payload.c_str());
// }

// // ===== OTA Task =====
// void otaTask(void *pvParameters) {
//     for (;;) {
//         if (otaInProgress) {
//             Serial.println("Requesting firmware chunks...");
//             if (!Update.begin(fw_size)) {
//                 Serial.println("Failed to begin OTA");
//                 client.publish("v1/devices/me/attributes", "{\"fw_state\":\"FAILED\"}");
//                 otaInProgress = false;
//                 continue;
//             }

//             while (offset < fw_size) {
//                 String req = "{\"title\":\"" + fw_title + "\",\"version\":\"" + fw_version + "\",\"chunkSize\":" + String(chunk_size) + ",\"chunk\":\"" + String(offset) + "\"}";
//                 client.publish("v1/devices/me/firmware/request", req.c_str());
//                 vTaskDelay(pdMS_TO_TICKS(1000));
//             }
//         }
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }

// // ===== LED Task =====
// void ledControlTask(void *pvParameters) {
//     bool currentLedState = false;
//     for (;;) {
//         if (xQueueReceive(ledStateQueue, &currentLedState, portMAX_DELAY) == pdTRUE) {
//             digitalWrite(ledPin, currentLedState ? HIGH : LOW);
//         }
//     }
// }

// // ===== DHT Task =====
// void dht11Task(void *pvParameters) {
//     float temperature = 0;
//     float humidity = 0;
    
//     for(;;) {
//         humidity = dht.readHumidity();
//         temperature = dht.readTemperature();

//         if (!isnan(humidity) && !isnan(temperature)) {
//             String payload = "{\"temperature\":" + String(temperature) + 
//                            ",\"humidity\":" + String(humidity) + "}";
//             client.publish("v1/devices/me/telemetry", payload.c_str());
//         }

//         vTaskDelay(pdMS_TO_TICKS(2000));
//     }
// }

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <Update.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "DHT.h"

// WiFi
const char* ssid = "TIEN TRUNG";
const char* password = "20121978";

// ThingsBoard
const char* thingsboardServer = "app.coreiot.io";
const int thingsboardPort = 1883;
const char* accessToken = "WytjI10Ucky6YlLErFmv";

// DHT
#define DHTPIN 8
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// LED
#define ledPin 48
volatile bool ledState = false;
QueueHandle_t ledStateQueue;

// OTA
bool otaInProgress = false;
bool waitingForChunk = false;
int fw_size = 0, chunk_size = 0, chunks_received = 0, offset = 0;
String fw_title = "", fw_version = "", fw_checksum = "", fw_algo = "sha256";

// MQTT Client
WiFiClient espClient;
PubSubClient client(espClient);

// Timeout
unsigned long lastRequestTime = 0;
const unsigned long REQUEST_TIMEOUT = 5000; // Thời gian chờ phản hồi (ms)

// ===== Base64 decode =====
int b64decode(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

size_t decode_base64(const char *input, uint8_t *output, size_t output_len) {
    size_t i = 0;
    int buffer = 0, bits = 0;
    while (*input && output_len) {
        int val = b64decode(*input++);
        if (val < 0) continue;
        buffer = (buffer << 6) | val;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            *output++ = (buffer >> bits) & 0xFF;
            output_len--;
            i++;
        }
    }
    return i;
}

// ===== Callback =====
void callback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("MQTT -> %s\n", topic);

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    String topicStr = String(topic);

    if (topicStr.indexOf("attributes") >= 0) {
        if (doc.containsKey("fw_title")) {
            fw_title = doc["fw_title"].as<String>();
            fw_version = doc["fw_version"].as<String>();
            fw_checksum = doc["fw_checksum"].as<String>();
            fw_algo = doc["fw_checksum_algorithm"].as<String>();
            fw_size = doc["fw_size"];
            chunk_size = doc["fw_chunk_size"];
            offset = 0;
            chunks_received = 0;

            otaInProgress = true;
            client.publish("v1/devices/me/attributes", "{\"fw_state\":\"INITIATED\"}");
        }

        if (doc.containsKey("ledState")) {
            bool newLedState = (doc["ledState"].as<String>() == "ON");
            xQueueSend(ledStateQueue, &newLedState, 0);
        }
    }

    if (topicStr.indexOf("firmware/response") >= 0 && otaInProgress && waitingForChunk) {
        const char* b64data = doc["data"];
        int len = strlen(b64data);
        std::vector<uint8_t> decoded(len / 4 * 3);

        size_t actualLen = decode_base64(b64data, decoded.data(), decoded.size());
        if (Update.write(decoded.data(), actualLen) != actualLen) {
            Serial.println("Update write failed!");
            client.publish("v1/devices/me/attributes", "{\"fw_state\":\"FAILED\"}");
            otaInProgress = false;
            Update.abort();
            return;
        }

        offset += actualLen;
        chunks_received++;
        Serial.printf("Chunk %d received. Total offset: %d/%d\n", chunks_received, offset, fw_size);

        waitingForChunk = false;
        lastRequestTime = 0; // Reset timeout

        if (offset >= fw_size) {
            if (Update.end(true)) {
                Serial.println("OTA Update Success!");
                client.publish("v1/devices/me/attributes", "{\"fw_state\":\"UPDATED\"}");
                vTaskDelay(pdMS_TO_TICKS(2000));
                ESP.restart();
            } else {
                Serial.println("Update end failed.");
                client.publish("v1/devices/me/attributes", "{\"fw_state\":\"FAILED\"}");
                otaInProgress = false;
            }
        }
    }
}

// ===== WiFi Task =====
void wifiTask(void *pvParameters) {
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.print("Connecting to WiFi...");
            WiFi.begin(ssid, password);
            while (WiFi.status() != WL_CONNECTED) {
                delay(500);
                Serial.print(".");
            }
            Serial.println("Connected to WiFi");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ===== MQTT Task =====
void mqttTask(void *pvParameters) {
    client.setServer(thingsboardServer, thingsboardPort);
    client.setCallback(callback);

    for (;;) {
        if (!client.connected()) {
            while (!client.connect("ESP32Client", accessToken, nullptr)) {
                Serial.println("Failed to connect to ThingsBoard, retrying...");
                delay(3000);
            }
            Serial.println("Connected to ThingsBoard");
            client.subscribe("v1/devices/me/attributes");
            client.subscribe("v1/devices/me/attributes/response");
            client.subscribe("v1/devices/me/firmware/response");

            String payload = "{\"shared\":[\"fw_title\",\"fw_version\",\"fw_size\",\"fw_checksum\",\"fw_checksum_algorithm\",\"fw_chunk_size\",\"ledState\"]}";
            client.publish("v1/devices/me/attributes/request/1", payload.c_str());
        }
        client.loop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ===== OTA Task =====
void otaTask(void *pvParameters) {
    for (;;) {
        if (otaInProgress && !waitingForChunk && offset < fw_size) {
            if (!Update.begin(fw_size)) {
                Serial.println("Failed to begin OTA");
                client.publish("v1/devices/me/attributes", "{\"fw_state\":\"FAILED\"}");
                otaInProgress = false;
                continue;
            }

            String req = "{\"title\":\"" + fw_title + "\",\"version\":\"" + fw_version + "\",\"chunkSize\":" + String(chunk_size) + ",\"chunk\":\"" + String(offset) + "\"}";
            client.publish("v1/devices/me/firmware/request", req.c_str());
            waitingForChunk = true;
            lastRequestTime = millis(); // Lưu thời điểm gửi request
            Serial.printf("Requested chunk at offset %d\n", offset);
        }

        // Kiểm tra timeout
        if (waitingForChunk && millis() - lastRequestTime > REQUEST_TIMEOUT) {
            Serial.println("Timeout waiting for chunk. Retrying...");
            waitingForChunk = false; // Hết chờ, thử lại
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ===== LED Control Task =====
void ledControlTask(void *pvParameters) {
    bool currentLedState = false;
    for (;;) {
        if (xQueueReceive(ledStateQueue, &currentLedState, portMAX_DELAY) == pdTRUE) {
            digitalWrite(ledPin, currentLedState ? HIGH : LOW);
        }
    }
}

// ===== DHT Task =====
void dht11Task(void *pvParameters) {
    float temperature, humidity;
    for (;;) {
        humidity = dht.readHumidity();
        temperature = dht.readTemperature();
        if (!isnan(humidity) && !isnan(temperature)) {
            String payload = "{\"temperature\":" + String(temperature) +
                             ",\"humidity\":" + String(humidity) + "}";
            client.publish("v1/devices/me/telemetry", payload.c_str());
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ===== Setup =====
void setup() {
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW);
    dht.begin();
    ledStateQueue = xQueueCreate(1, sizeof(bool));

    xTaskCreate(wifiTask, "WiFi Task", 4096, NULL, 1, NULL);
    xTaskCreate(mqttTask, "MQTT Task", 8192, NULL, 2, NULL);
    xTaskCreate(ledControlTask, "LED Control Task", 2048, NULL, 3, NULL);
    xTaskCreate(dht11Task, "DHT11 Task", 4096, NULL, 2, NULL);
    xTaskCreate(otaTask, "OTA Task", 8192, NULL, 3, NULL);
}

// ===== Loop =====
void loop() {}


