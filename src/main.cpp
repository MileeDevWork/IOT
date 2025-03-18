#include <Arduino.h>
#include <Wire.h>
#include "DHT20.h"
#include "DHT.h"
#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>


//cấu hình chân dht11
#define DHTPIN 8       //D5
#define DHTTYPE DHT11     
DHT dht(DHTPIN, DHTTYPE);
//cấu hình wifi
constexpr char WIFI_SSID[] = "Min";      
constexpr char WIFI_PASSWORD[] = "123456789"; 
//cấu hình coreiot
constexpr char TOKEN[] = "P4el0SBJgMs4rngDWaSE"; // Token xác thực
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io"; // Máy chủ ThingsBoard
constexpr uint16_t THINGSBOARD_PORT = 1883U; // Cổng MQTT
//cấu hình chuẩn kết nối 
//thời gian gửi dữ liệu lên coreiot
constexpr int16_t telemetrySendInterval = 5000U; // Gửi mỗi 5 giây
uint32_t previousDataSend;
//khởi tạo kết nối wifi và ccorreiot
WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, 1024U);

//biến lưu dữ liệu cảm biến dùng chung cho các task
float temperature = NAN;
float humidity = NAN;
SemaphoreHandle_t sensorDataMutex;

//HÀM KẾT NỐI WIFI
void InitWiFi() {
  Serial.println("Đang kết nối WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Kiểm tra kết nối WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nĐã kết nối WiFi!");
}

//KIỂM TRA VÀ KẾT NỐI LẠI WIFI NẾU MẤT KẾT NỐI
const bool reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    return true;
  }
  InitWiFi();
  return true;
}



// Task 1: read dht11
void Task1(void *pvParameters)
{
  unsigned long lastReadTime = 0;  // Lưu thời gian đọc gần nhất

  while (1) {
    if (millis() - lastReadTime >= 2000) {  // Kiểm tra nếu đã qua 2 giây
      lastReadTime = millis();  // Cập nhật thời gian đọc mới nhất

      float temp = dht.readTemperature();
      float hum = dht.readHumidity();

      if (!isnan(temp) && !isnan(hum)) {
        if (xSemaphoreTake(sensorDataMutex, portMAX_DELAY)) {
          temperature = temp;
          humidity = hum;
          xSemaphoreGive(sensorDataMutex);
        }
        Serial.printf("Nhiệt độ: %.2f °C | Độ ẩm: %.2f %%\n", temp, hum);
      } else {
        Serial.println("Lỗi! Không thể đọc từ DHT11.");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500));  // Giảm tải CPU, kiểm tra lại sau 500ms
  }
}


// Task 2: send data to coreiot
void TaskThingsBoard(void *pvParameters) {
  uint32_t previousDataSend = 0;

  while (1) {
    if (!reconnect()) {
      vTaskDelay(pdMS_TO_TICKS(5000));  // Đợi trước khi thử lại
      continue;
    }

    if (!tb.connected()) {
      Serial.println("Đang kết nối ThingsBoard...");
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
        Serial.println("Kết nối thất bại!");
        vTaskDelay(pdMS_TO_TICKS(5000));  // Thử lại sau 5 giây
        continue;
      }
      tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());
      Serial.println("Kết nối ThingsBoard thành công!");
    }

    // Gửi dữ liệu mỗi 10 giây
    if (millis() - previousDataSend > telemetrySendInterval) {
      previousDataSend = millis();

      float temp, hum;
      if (xSemaphoreTake(sensorDataMutex, portMAX_DELAY)) {
        temp = temperature;
        hum = humidity;
        xSemaphoreGive(sensorDataMutex);
      }

      if (!isnan(temp) && !isnan(hum)) {
        Serial.println("Gửi dữ liệu lên ThingsBoard...");
        tb.sendTelemetryData("temperature", temp);
        tb.sendTelemetryData("humidity", hum);
      } else {
        Serial.println("Không có dữ liệu hợp lệ để gửi!");
      }
    }

    tb.loop();  // Xử lý MQTT
    vTaskDelay(pdMS_TO_TICKS(1000));  // Kiểm tra mỗi giây
  }
}

void setup()
{
  Serial.begin(115200);
  dht.begin();
  InitWiFi();
  // Tạo semaphore để bảo vệ dữ liệu cảm biến
  sensorDataMutex = xSemaphoreCreateMutex();
  xTaskCreate(Task1, "DHT20Task", 4096, NULL, 2, NULL);
  // Tạo task gửi dữ liệu lên ThingsBoard
  xTaskCreate(TaskThingsBoard, "ThingsBoard_Task", 4096, NULL, 2, NULL);
}

void loop()
{

}
