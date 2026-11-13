#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ======================= 1. CẤU HÌNH =======================
// Wifi (trên Wokwi dùng Wokwi-GUEST, mạch thật thì đổi lại)
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// MQTT broker công cộng, không cần tài khoản
const char* MQTT_SERVER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;

const char* TOPIC_DATA = "VuDangThanh_nhom5/fall/data";    // ESP32 -> Web
const char* TOPIC_LED  = "VuDangThanh_nhom5/fall/led";     // Web -> ESP32 ("ON"/"OFF")
const char* TOPIC_BUZ  = "VuDangThanh_nhom5/fall/buzzer";  // Web -> ESP32 ("ON"/"OFF")

// Chân thiết bị
#define LED_PIN     26
#define BUZZER_PIN  27

// Ngưỡng phát hiện té ngã
const float FALL_ACC_G   = 2.0;   // tổng gia tốc > 2g (va đập mạnh)
const float FALL_ANGLE   = 60.0;  // góc nghiêng > 60 độ
const unsigned long ALERT_TIME_MS = 5000;  // cảnh báo kéo dài 5 giây

// ======================= 2. BIẾN TOÀN CỤC =======================
Adafruit_MPU6050 mpu;
WiFiClient espClient;
PubSubClient client(espClient);

// Gói dữ liệu truyền qua Queue
struct SensorData {
  float ax, ay, az;   // gia tốc 3 trục (đơn vị g)
  float acc;          // tổng gia tốc (g)
  float angle;        // góc nghiêng (độ)
  bool  fall;         // true nếu vượt ngưỡng
};

QueueHandle_t dataQueue;
QueueHandle_t alertQueue;

volatile bool ledManual = false;
volatile bool buzManual = false;
volatile bool alarmOn   = false;
volatile bool ledState  = false;
volatile bool buzState  = false;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  bool on = (msg == "ON");
  if (String(topic) == TOPIC_LED) ledManual = on;
  if (String(topic) == TOPIC_BUZ) buzManual = on;
  Serial.printf("[MQTT] Nhan lenh %s = %s\n", topic, msg.c_str());
}

void mqttReconnect() {
  while (!client.connected()) {
    Serial.print("[MQTT] Dang ket noi broker...");
    String clientId = "ESP32_FALL_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println(" OK");
      client.subscribe(TOPIC_LED);
      client.subscribe(TOPIC_BUZ);
    } else {
      Serial.printf(" that bai (rc=%d), thu lai sau 2s\n", client.state());
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }
}

void taskTruyenData(void *pvParameters) {
  Serial.print("[WIFI] Dang ket noi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }
  Serial.println("\n[WIFI] Da ket noi Wifi!");
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqttCallback);
  SensorData d;
  char json[160];
  for (;;) {
    if (!client.connected()) mqttReconnect();
    client.loop();
    if (xQueueReceive(dataQueue, &d, pdMS_TO_TICKS(50)) == pdTRUE) {
      snprintf(json, sizeof(json),
        "{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"acc\":%.2f,\"angle\":%.1f,\"fall\":%d,\"led\":%d,\"buz\":%d}",
        d.ax, d.ay, d.az, d.acc, d.angle,
        alarmOn ? 1 : 0, ledState ? 1 : 0, buzState ? 1 : 0);
      client.publish(TOPIC_DATA, json);
    }
  }
}

void taskDocData(void *pvParameters) {
  sensors_event_t a, g, temp;
  SensorData d;
  for (;;) {
    mpu.getEvent(&a, &g, &temp);
    d.ax = a.acceleration.x / 9.80665;
    d.ay = a.acceleration.y / 9.80665;
    d.az = a.acceleration.z / 9.80665;
    d.acc = sqrt(d.ax * d.ax + d.ay * d.ay + d.az * d.az);
    d.angle = atan2(sqrt(d.ax * d.ax + d.ay * d.ay), d.az) * 180.0 / PI;
    d.fall = (d.acc > FALL_ACC_G) || (d.angle > FALL_ANGLE);
    Serial.printf("[DOC] ax=%.2f ay=%.2f az=%.2f | acc=%.2f g | goc=%.1f do | %s\n",
      d.ax, d.ay, d.az, d.acc, d.angle, d.fall ? "TE NGA" : "binh thuong");
    xQueueSend(dataQueue, &d, 0);
    xQueueSend(alertQueue, &d, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void taskCanhBao(void *pvParameters) {
  SensorData d;
  unsigned long fallUntil = 0;
  bool lastBuz = false;
  for (;;) {
    if (xQueueReceive(alertQueue, &d, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (d.fall) {
        if (millis() >= fallUntil) Serial.println("[CANH BAO] !!! PHAT HIEN TE NGA !!!");
        fallUntil = millis() + ALERT_TIME_MS;
      }
    }
    alarmOn = (millis() < fallUntil);
    bool blink = (millis() / 200) % 2;
    ledState = ledManual || alarmOn;
    digitalWrite(LED_PIN, ledManual || (alarmOn && blink));
    buzState = buzManual || alarmOn;
    if (buzState && !lastBuz) tone(BUZZER_PIN, 2000);
    if (!buzState && lastBuz) noTone(BUZZER_PIN);
    lastBuz = buzState;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Wire.begin(21, 22);
  while (!mpu.begin()) {
    Serial.println("Khong tim thay MPU6050, kiem tra day noi!");
    delay(500);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.println("MPU6050 san sang");
  dataQueue = xQueueCreate(10, sizeof(SensorData));
  alertQueue = xQueueCreate(10, sizeof(SensorData));
  xTaskCreatePinnedToCore(taskTruyenData, "TruyenData", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskDocData, "DocData", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskCanhBao, "CanhBao", 4096, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelete(NULL);
}