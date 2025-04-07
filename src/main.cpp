#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "DHT.h"

// Thông tin kết nối WiFi
const char* ssid = "TIEN TRUNG";         
const char* password = "20121978"; 

// Thông tin kết nối ThingsBoard
const char* thingsboardServer = "app.coreiot.io"; 
const int thingsboardPort = 1883;// Cổng MQTT mặc định
const char* accessToken = "88ne10cmngvsirz1vbq6";         

//cấu hình dht11
#define DHTPIN 8       //D5 -> chân dht11
#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE); // Khởi tạo đối tượng DHT

// Chân GPIO kết nối với đèn LED
#define ledPin 48 // Chân LED

WiFiClient espClient;
PubSubClient client(espClient);

// Shared attribute keys
const char* ledStateControlKey = "ledState"; // Sử dụng ledState làm key

// Biến lưu trữ trạng thái LED
volatile bool ledState = false; // Mặc định đèn tắt, volatile vì được truy cập từ nhiều task

//nhận trạng thái led từ task mqtt
QueueHandle_t ledStateQueue;

// khai báo hàm
void wifiTask(void *pvParameters);
void mqttTask(void *pvParameters);
void ledControlTask(void *pvParameters);
void connectWifi();
void connectThingsBoard();
void callback(char* topic, byte* payload, unsigned int length);
void dht11Task(void *pvParameters);

void setup() {
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW); // Đảm bảo ban đầu đèn tắt
    dht.begin();

    ledStateQueue = xQueueCreate(1, sizeof(bool)); // Queue size 1 to hold the latest state

    // tạo task
    xTaskCreate(wifiTask, "WiFi Task", 4096, NULL, 1, NULL); 
    xTaskCreate(mqttTask, "MQTT Task", 8192, NULL, 2, NULL);
    xTaskCreate(ledControlTask, "LED Control Task", 2048, NULL, 3, NULL); 
    xTaskCreate(dht11Task, "DHT11 Task", 4096, NULL, 2, NULL);
}

void loop() {

}

//kết nối wifi
void wifiTask(void *pvParameters) {
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            connectWifi();
        }
        vTaskDelay(pdMS_TO_TICKS(5000)); // Check WiFi status every 5 seconds
    }
}

//task kết nối mqtt và giao tiếp với ThingsBoard
void mqttTask(void *pvParameters) {
    client.setServer(thingsboardServer, thingsboardPort);
    client.setCallback(callback);

    for (;;) {
        if (!client.connected()) {
            connectThingsBoard();
        }
        client.loop();
        vTaskDelay(pdMS_TO_TICKS(100)); // Process MQTT messages
    }
}

// Task to control the LED based on the ledState variable
void ledControlTask(void *pvParameters) {
    bool currentLedState = false;
    for (;;) {
        if (xQueueReceive(ledStateQueue, &currentLedState, portMAX_DELAY) == pdTRUE) {
            digitalWrite(ledPin, currentLedState ? HIGH : LOW);
            Serial.print("Setting LED to: ");
            Serial.println(currentLedState ? "ON" : "OFF");
        }
        // No need for additional delay here as the task will wait for a message in the queue
    }
}

// Hàm để kết nối WiFi
void connectWifi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected to WiFi");
}

// Hàm để kết nối ThingsBoard
void connectThingsBoard() {
    while (!client.connect("ESP32Client", accessToken, nullptr)) {
        Serial.print("Failed to connect to ThingsBoard, rc=");
        Serial.println(client.state());
        delay(5000);
    }
    Serial.println("Connected to ThingsBoard");
    // Đăng ký nhận thông tin shared attributes
    client.subscribe("v1/devices/me/attributes");
    // Yêu cầu giá trị ban đầu của các shared attributes
    String payload = "{\"shared\":[\"" + String(ledStateControlKey) + "\"]}";
    client.publish("v1/devices/me/attributes", payload.c_str());
    Serial.println("Sent request for shared attributes.");
}

// Callback function khi nhận được tin nhắn MQTT
void callback(char* topic, byte* payload, unsigned int length) {
    Serial.println("Callback function called in MQTT Task.");
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    // Xử lý phản hồi shared attributes
    if (strstr(topic, "attributes")) {
        Serial.println("Processing shared attributes response...");
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, payload, length);

        if (doc.containsKey("ledState")) {
            String ledStateStr = doc["ledState"].as<String>();
            Serial.print("ledState value from TB: ");
            Serial.println(ledStateStr);

            bool newLedState = false;
            if (ledStateStr == "ON") {
                newLedState = true;
                Serial.println("ledState is now TRUE");
            } else {
                newLedState = false;
                Serial.println("ledState is now FALSE");
            }

            // Send the new LED state to the LED control task via the queue
            if (xQueueSend(ledStateQueue, &newLedState, 0) != pdTRUE) {
                Serial.println("Failed to send LED state to queue.");
            }
            Serial.print("Sent ledState to LED Control Task: ");
            Serial.println(newLedState ? "ON" : "OFF");
        } else {
            Serial.println("Attribute 'ledState' not found in response.");
        }
    }
}

//dht11 task
void dht11Task(void *pvParameters) {
    float temperature = 0;
    float humidity = 0;
    
    for(;;) {
        // Đọc dữ liệu từ DHT11
        humidity = dht.readHumidity();
        temperature = dht.readTemperature();

        // Kiểm tra nếu đọc thành công
        if (!isnan(humidity) && !isnan(temperature)) {
            // In ra Serial để debug
            Serial.printf("Nhiệt độ: %.2f°C, Độ ẩm: %.2f%%\n", temperature, humidity);
            
            // Gửi dữ liệu lên ThingsBoard
            String payload = "{\"temperature\":" + String(temperature) + 
                           ",\"humidity\":" + String(humidity) + "}";
            client.publish("v1/devices/me/telemetry", payload.c_str());
        } else {
            Serial.println("Lỗi đọc cảm biến DHT11!");
        }

        // Đợi 2 giây trước khi đọc lại
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}