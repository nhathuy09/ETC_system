#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_camera.h>
#include <HTTPClient.h>
#include <WebServer.h>

// WiFi & MQTT config
// const char* ssid = "GDU-Core";
// const char* password = "GDU2024@371NK@";
const char* ssid = "Van Ut_Lau 4-5";
const char* password = "vanut1978@";


// const char* ssid = "OngNgoai";
// const char* password = "0913858753";
const char* mqtt_server = "192.168.1.11";
const char* topic_sub = "cam/capture";
const char* serverUrl = "http://192.168.1.11:1880/upload_image";

// ESP32 clients
WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);

#define FLASH_PIN 4
int imageCounter = 0;
void startCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
 config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1; // sử dụng double buffer để tránh ảnh cũ

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf(" Camera init failed: 0x%x\n", err);
    ESP.restart();
  } else {
    Serial.println(" Camera ready");
  }
}


void sendImage() {
  // Flush 3 frame liên tiếp để lấy ảnh mới nhất
  for (int i = 0; i < 3; i++) {
    camera_fb_t* flush_fb = esp_camera_fb_get();
    if (flush_fb) esp_camera_fb_return(flush_fb);
    delay(100);
  }
  // Chụp ảnh thật
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb || fb->len == 0) {
    Serial.println(" Không lấy được ảnh hợp lệ");
    if (fb) esp_camera_fb_return(fb);
    return;
  }
  Serial.printf(" Kích thước ảnh: %u bytes\n", fb->len);
  String filename = "/plate_" + String(millis()) + "_" + String(imageCounter++) + ".jpg";
  String fullUrl = String(serverUrl) + "?filename=" + filename;
  HTTPClient http;
  http.begin(fullUrl);
  http.addHeader("Content-Type", "application/octet-stream");

  int responseCode = http.POST(fb->buf, fb->len);
  if (responseCode > 0) {
    String response = http.getString();
    Serial.printf(" Ảnh đã gửi: %s - Mã: %d - Server: %s\n", filename.c_str(), responseCode, response.c_str());

    digitalWrite(FLASH_PIN, HIGH); delay(100); digitalWrite(FLASH_PIN, LOW);
  } else {
    Serial.printf(" Lỗi gửi ảnh: %s\n", http.errorToString(responseCode).c_str());
  }
  http.end();
  esp_camera_fb_return(fb);
}

void handleCapture() {
  sendImage();
  server.send(200, "text/plain", "Ảnh đã được gửi");
}

// ================= MQTT =================
void callback(char* topic, byte* payload, unsigned int length) {
  String command = "";
  for (unsigned int i = 0; i < length; i++) command += (char)payload[i];
  command.trim();

  if (command == "DETECTED") {
    Serial.println(" Nhận lệnh MQTT: DETECTED");
    sendImage();
  }
  if(command=="PICTURE"){
    Serial.println("Nhận lệnh chụp hình");
    sendImage();
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print(" Đang kết nối MQTT...");
    String clientId = "esp32cam-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println(" MQTT đã kết nối");
      client.subscribe(topic_sub);
    } else {
      Serial.printf(" Lỗi MQTT [%d], thử lại sau 2s\n", client.state());
      delay(2000);
    }
  }
}

// ================= WIFI =================
void reconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print(" Đang kết nối WiFi...");
  WiFi.disconnect();
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n Đã kết nối WiFi");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n WiFi thất bại");
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, LOW);

  reconnectWiFi();
  startCamera();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  server.on("/capture", HTTP_GET, handleCapture);
  server.begin();

  Serial.println(" HTTP server đã khởi động");
}

// ================= MAIN LOOP =================
void loop() {
  reconnectWiFi();
  if (!client.connected()) reconnectMQTT();
  client.loop();
  server.handleClient();
}
