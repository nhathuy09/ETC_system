# 🚗 Automated Toll Collection (ETC) System

## 📌 Overview

This project is an **Automated Toll Station Monitoring, Control, and Management System** built using IoT, Computer Vision, and AI.
The system automatically detects vehicles, recognizes license plates, validates them against a database, and controls the barrier gate in real time.

---

## 🏗️ System Architecture

```
Vehicle → Sensor Trigger → ESP32 → MQTT → ESP32-CAM → Capture Image
        → Node-RED → Python AI Processing → MySQL Database
        → Decision → Open Barrier (Servo)
```

---

## 🔧 Hardware Components

* ESP32 (Sensor + Control Unit)
* ESP32-CAM (Image Capture)
* Servo Motor (Barrier Gate)
* IR Sensors / Ultrasonic Sensors
* LCD I2C 16x2 Display

---

## 💻 Software Stack

### 🔹 Embedded Systems

* Arduino Framework (ESP32)
* MQTT Communication (PubSubClient)

### 🔹 Backend & Orchestration

* Node-RED (Flow control + API endpoints)
* MQTT Broker

### 🔹 AI & Computer Vision

* YOLOv8 (License plate detection)
* OpenCV (Image preprocessing)
* PaddleOCR (Text recognition)

### 🔹 Database

* MySQL

  * `vehicles`
  * `transactions`
  * `logs`

---

## ⚙️ Core Features

* 📸 Automatic image capture via ESP32-CAM
* 🚘 License plate detection using YOLOv8
* 🔍 OCR recognition using PaddleOCR
* 💾 Vehicle validation via MySQL
* 💰 Automated toll processing
* 🚧 Barrier control (open/close)
* 📊 Logging & transaction tracking
* 🌐 Web interface via Node-RED

---

## 🔄 Data Flow

1. Vehicle arrives → Sensor detects
2. ESP32 publishes MQTT message (`vehicle/detect`)
3. ESP32-CAM receives `cam/capture` → captures image
4. Image sent via HTTP POST to Node-RED
5. Node-RED:

   * Decodes base64 image
   * Saves image to disk
   * Calls Python processing script
6. Python:

   * Detect plate (YOLO)
   * Preprocess image (OpenCV)
   * Recognize text (PaddleOCR)
7. System queries MySQL:

   * Valid vehicle → allow
   * Invalid → deny
8. Node-RED sends command → ESP32 opens gate

---

## 🧠 AI Processing Pipeline

```python
Input Image
   ↓
YOLOv8 (Detect Plate Region)
   ↓
OpenCV (Grayscale + Denoise + Threshold)
   ↓
PaddleOCR (Text Recognition)
   ↓
Post-processing (Format Plate)
```

---

## 📂 Project Structure

```
├── esp32/
│   ├── sensor_control.ino
│   └── cam_capture.ino
│
├── node-red/
│   └── flows.json
│
├── python/
│   ├── detect_plate.py
│   ├── ocr_pipeline.py
│   └── database_handler.py
│
├── images/
│   └── captured_frames/
│
└── README.md
```

---

## 🌐 Network Configuration

| Component   | IP Address   |
| ----------- | ------------ |
| MQTT Broker | 192.168.1.4  |
| Node-RED    | 192.168.1.10 |
| ESP32-CAM   | 192.168.1.5  |

---

## ⚠️ Known Issues & Fixes

* ❌ `Duplicate Content-Length` → Fixed by removing manual header
* ❌ Corrupted images → Ensure proper buffer size & PSRAM usage
* ❌ OCR inaccuracies → Improved with PaddleOCR + preprocessing

---

## 🚀 Future Improvements

* Convert Python script → FastAPI microservice
* Add Redis cache for fast lookup
* Implement message queue (RabbitMQ/Kafka)
* AI Agent + RAG for intelligent fallback handling
* Deploy system on cloud / edge hybrid

---

## 📊 Use Cases

* Smart Toll Stations
* Parking Management Systems
* Vehicle Access Control
* Smart City Infrastructure

---

## 👨‍💻 Author

**Huy Võ**
Final-year IoT Engineering Project

---

## 📜 License

This project is for educational and research purposes.

