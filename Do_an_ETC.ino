#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// WiFi / MQTT
const char* ssid = "Van Ut_Lau 4-5";
const char* password = "vanut1978@";
const char* mqtt_server = "192.168.1.11";
const char* mqtt_id = "esp32_ir_sensor";
const char* topic_publish = "vehicle/detect";
const char* topic_subscribe = "to-Esp32";

// Pins
#define IR_SENSOR_IN 14
#define IR_SENSOR_OUT 33
#define SERVO_PIN 32
#define ledRed 25
#define ledYellow 26
#define ledGreen 27
#define buttonPin 35
#define SERVO_PIN2 13
// Debounce
const unsigned long DEBOUNCE_TIME = 50;

// Gate timeout (ms)
const unsigned long GATE_TIMEOUT = 10000; // 10 giây — chỉnh nếu cần

Servo servoMotor;
Servo servoMotor2;
WiFiClient espClient;
PubSubClient client(espClient);

// trạng thái cảm biến / debounce
int lastStateIn = HIGH;
int lastStateOut = HIGH;
unsigned long lastDebounceTimeIn = 0;
unsigned long lastDebounceTimeOut = 0;

// nút
bool lastButtonState = HIGH;
unsigned long lastButtonDebounce = 0;
bool manualToggle = false; // trạng thái mở/đóng do nút

// trạng thái mở cổng
bool gateOpenedRemotely = false;
bool gateOpenedManually = false;
unsigned long gateOpenTimestamp = 0;

unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 5000;
void openGate(bool byRemote) {
  // Mở mượt từ 0 -> 90
  for (int p = 0; p <= 90; p += 5) {
    servoMotor.write(p);
    delay(10);
  }
  digitalWrite(ledGreen, HIGH);
  digitalWrite(ledRed, LOW);
  digitalWrite(ledYellow, LOW);
  if (byRemote) {
    gateOpenedRemotely = true;
    gateOpenedManually = false;
  } else {
    gateOpenedManually = true;
    gateOpenedRemotely = false;
  }
  gateOpenTimestamp = millis();
}

void closeGate() {
  // Đóng mượt 90 -> 0
  for (int p = 90; p >= 0; p -= 5) {
    servoMotor.write(p);
    delay(10);
  }
  digitalWrite(ledGreen, LOW);
  digitalWrite(ledRed, HIGH);
  digitalWrite(ledYellow, LOW);
  gateOpenedRemotely = false;
  gateOpenedManually = false;
  gateOpenTimestamp = 0;
  // reset LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tram Thu Phi ETC");
}

void handleNonJsonMessage(const String &msg) {
  // ví dụ nhận "SERVO" để mở khẩn
  if (msg == "SERVO") {
    Serial.println("MQTT: SERVO received - emergency open");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("OPEN BARRIER");
    logToSQL("unknow","mqtt","0");
    openGate(true);
  }
  else if(msg =="SERVO2"){
    Serial.println("Mở servo thứ 2");
    servoMotor2.write(90);
  } else {
    Serial.println("MQTT: Non-JSON payload: " + msg);
  }
}
void logToSQL(String plate, String method, String fee) {
  StaticJsonDocument<256> doc;
  doc["plate"] = plate;
  doc["method"] = method; 
  doc["fee"] = fee;

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  client.publish("toll/log", buffer, n);
}
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println("MQTT Message: " + message);
  message.trim();
  if (!message.startsWith("{")) {
    handleNonJsonMessage(message);
    return;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.print(" Lỗi parse JSON: ");
    Serial.println(error.c_str());
    return;
  }

const char* command = doc["command"];
const char* plate=doc["plate"];
int fees = doc["fee"].as<int>();          
const char* status = doc["status"];
const char* mess = doc["message"];
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Plate:");
  lcd.setCursor(6, 0);
  lcd.print(plate ? plate : "------");


  if ( command && String(command) == "OPEN") {
      openGate(true);
      Serial.println("OPEN gate for plate: " + String(plate ? plate : "unknown"));
  } else if (status) {
      String st = String(status);
      if (st == "error") {
      Serial.println("Lỗi biển hoặc hệ thống: " + String(mess ? mess : ""));
      lcd.setCursor(0,1);
      lcd.print("Chua Dang Ki");
      // hiển thị led đỏ
      digitalWrite(ledRed, HIGH);
      digitalWrite(ledGreen, LOW);
      digitalWrite(ledYellow, LOW);
    } else if (st == "insufficient") {
      digitalWrite(ledRed, HIGH);
      digitalWrite(ledGreen, LOW);
      digitalWrite(ledYellow, LOW);
      lcd.setCursor(0,1);
      lcd.print("Not Enough Money");
    }else if(st == "success"){
      lcd.setCursor(0, 1);
      lcd.print("Pass  Fee:");
      lcd.print(fees);
    } else {
      Serial.println("MQTT status: " + st);
    }
  }
}

void reconnectWiFiIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Kết nối WiFi...");
  WiFi.disconnect(true);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi thất bại");
  }
}

void reconnectMQTT() {
  if (client.connected()) return;

  Serial.print("MQTT reconnect...");
  while (!client.connected()) {
    if (client.connect(mqtt_id)){
      Serial.println(" MQTT kết nối");
      client.subscribe(topic_subscribe);
    } else {
      Serial.print(".");
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(IR_SENSOR_IN, INPUT);
  pinMode(IR_SENSOR_OUT, INPUT);
  pinMode(ledRed, OUTPUT);
  pinMode(ledYellow, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  digitalWrite(ledRed, HIGH);
  digitalWrite(ledYellow, LOW);
  digitalWrite(ledGreen, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Tram Thu Phi ETC");

  servoMotor.attach(SERVO_PIN);
  servoMotor2.attach(SERVO_PIN2);
  servoMotor.write(0); // đóng ban đầu

  pinMode(buttonPin, INPUT_PULLUP);

  reconnectWiFiIfNeeded();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {

if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > reconnectInterval) {
      lastReconnectAttempt = now;
      Serial.println("Trying to reconnect MQTT...");
      reconnectMQTT();
    }
  } else {
    client.loop();
  }
  

  int readingIn = digitalRead(IR_SENSOR_IN);
  if (readingIn != lastStateIn) {
    if (millis() - lastDebounceTimeIn > DEBOUNCE_TIME) {
      lastDebounceTimeIn = millis();
      lastStateIn = readingIn;
      if (readingIn == LOW && !(gateOpenedRemotely || gateOpenedManually)) {
        Serial.println("Phát hiện xe đến");
        digitalWrite(ledYellow, HIGH);
        digitalWrite(ledRed, LOW);
        digitalWrite(ledGreen, LOW);
        bool sent = client.publish(topic_publish, "DETECTED");
        Serial.println(sent ? " Gửi MQTT thành công" : "Gửi MQTT thất bại");
      }
    }
  }

  int readingOut = digitalRead(IR_SENSOR_OUT);
  if (readingOut != lastStateOut) {
    if (millis() - lastDebounceTimeOut > DEBOUNCE_TIME) {
      lastDebounceTimeOut = millis();
      lastStateOut = readingOut;
      if (readingOut == LOW && (gateOpenedRemotely || gateOpenedManually)) {
        Serial.println("Xe đã đi qua - đóng cổng");
        closeGate();
      }
    }
  }

  int readButton = digitalRead(buttonPin);
  if (readButton != lastButtonState) {
    lastButtonDebounce = millis();
  }
  if ((millis() - lastButtonDebounce) > DEBOUNCE_TIME) {
    if (readButton == LOW && lastButtonState == HIGH) {
      Serial.println("Nút bấm: toggle manual gate");
      if (gateOpenedManually) {
        closeGate();
      } else {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Open barrier");
        lcd.setCursor(0,1);
        lcd.print("Go ahead, please");
        logToSQL("unknow","manual","0");
        servoMotor2.write(0);
        openGate(false);
      }
    }
  }
  lastButtonState = readButton;
  if ((gateOpenedRemotely || gateOpenedManually) && gateOpenTimestamp != 0) {
    if (millis() - gateOpenTimestamp > GATE_TIMEOUT) {
      Serial.println("Gate timeout: tự đóng sau " + String(GATE_TIMEOUT) + " ms");
      closeGate();
    }
  }
  delay(10);
}
