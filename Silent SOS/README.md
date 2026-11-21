# 🚨 Silent SOS – IoT-Based Emergency Alert System

An ESP32-based emergency alert system that monitors motion-related activity, provides a short cancellation window, and sends emergency notifications through IoT services.

## 📌 Overview

Silent SOS is an embedded systems and IoT project built around the **ESP32 DevKit V1** and **MPU6050 (GY-521)** motion sensor.

The system supports **SAFE**, **ARMED**, and **DANGER** operating modes. When an emergency condition is detected, a **5-second cancellation window** gives the user an opportunity to cancel the alert. If the alert is not cancelled, the system can notify through Telegram and update Firebase-based monitoring.

The project also uses **smartphone GPS** for location sharing.

> **Implementation note:** This repository documents the implemented project configuration. It does not include a dedicated GPS module, GSM module, or dedicated tap sensor.

## 🎯 Objectives

- Build an IoT-based emergency alert system using an ESP32.
- Read motion data from an MPU6050 through I2C.
- Implement SAFE, ARMED, and DANGER operating modes.
- Provide a 5-second emergency cancellation window.
- Integrate Firebase for remote monitoring.
- Send emergency notifications through Telegram.
- Share location using smartphone GPS.

## ⚙️ Hardware

| Component | Purpose |
|---|---|
| ESP32 DevKit V1 (30-pin) | Main controller and Wi-Fi connectivity |
| MPU6050 / GY-521 | Accelerometer and gyroscope for motion sensing |
| Push buttons | User input and system control |
| Smartphone | GPS location acquisition and sharing |

### Hardware details

- **Controller:** ESP32 DevKit V1
- **Motion sensor:** MPU6050 (GY-521)
- **Communication:** I2C
- **MPU6050 I2C address:** `0x68`
- **Wireless connectivity:** Wi-Fi
- **Location source:** Smartphone GPS

## 🔄 Operating Modes

### SAFE
Normal system state.

### ARMED
The system monitors motion-related activity for potential emergency conditions.

### DANGER
When an emergency condition is detected, the system starts a 5-second cancellation countdown.

### Emergency cancellation
The user can cancel the alert during the countdown.

### Emergency notification
If the alert is not cancelled, the system sends a Telegram notification and updates the Firebase-based monitoring system.

## 🧠 System Architecture

```text
                 MPU6050
                    |
                   I2C
                    |
             ESP32 DevKit V1
                    |
             Motion Monitoring
                    |
          SAFE / ARMED / DANGER
                    |
            Emergency Detection
                    |
             5-Second Timer
                    |
             Cancellation Check
                    |
           Emergency Confirmation
                    |
              Wi-Fi Network
                    |
           +--------+--------+
           |                 |
        Firebase          Telegram
           |
    Monitoring Dashboard

       Smartphone GPS
             |
       Location Sharing
```

## 🛠️ Technologies

- Embedded C / Arduino framework
- ESP32
- MPU6050
- I2C communication
- Wi-Fi
- Firebase
- Telegram Bot API
- Smartphone GPS integration

## 🧪 Implementation & Testing

The system was developed using the ESP32 DevKit V1 and MPU6050.

### Implemented

- MPU6050 initialization and I2C communication
- Accelerometer data acquisition
- SAFE, ARMED, and DANGER operating modes
- 5-second emergency cancellation countdown
- Firebase integration
- Telegram notification integration
- Monitoring dashboard
- Smartphone GPS-based location sharing

The MPU6050 was tested over I2C using address `0x68`.

## ✨ Key Features

- Motion-based emergency monitoring
- Three operating modes
- 5-second cancellation window
- Wi-Fi-based communication
- Firebase integration
- Telegram emergency notifications
- Smartphone GPS location sharing
- Remote monitoring dashboard

## 📁 Repository Structure

```text
Silent-SOS-Emergency-Alert-System/
├── README.md
├── firmware/
│   └── ESP32 source code
├── hardware/
│   ├── circuit_diagram/
│   └── hardware_connections.md
├── documentation/
│   ├── project_report.pdf
│   └── project_presentation.pdf
└── images/
    ├── hardware_setup.jpg
    ├── system_architecture.png
    └── dashboard.png
```

*Add the folders and files above as you publish your actual project materials.*

## 🚀 Future Improvements

- Add a dedicated GPS module.
- Add GSM-based emergency communication.
- Improve motion detection and emergency classification.
- Add battery monitoring and low-power operation.
- Improve reliability during Wi-Fi disconnection.
- Develop a dedicated mobile application.

## 👨‍💻 Project Information

| Field | Details |
|---|---|
| Project | Silent SOS – IoT-Based Emergency Alert System |
| Domain | Embedded Systems, IoT, Wireless Communication |
| Controller | ESP32 DevKit V1 |
| Sensor | MPU6050 (GY-521) |
| Status | Completed |

---

## 🔐 Security

Before making the repository public, remove:

- Wi-Fi credentials
- Firebase private credentials or service-account files
- Telegram bot tokens
- Personal phone numbers and private location information

Use placeholders or a separate configuration file for secrets. Never commit credentials to GitHub.

---

*Developed as an embedded systems and IoT project focused on emergency monitoring, alert communication, and remote monitoring.*
