#include <HCSR04.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>


// Inductive Proximity Sensor
#define inductivePin 34

// Servo Motors
#define servoLogamPin 26
#define servoNonLogamPin 27

// Ultrasonic Sensors
#define uObjectTrigPin 4
#define uObjectEchoPin 16

#define uHeightLogamTrigPin 17
#define uHeightLogamEchoPin 5

#define uHeightNonLogamTrigPin 18
#define uHeightNonLogamEchoPin 19

// LEDs
#define ledLogam 2
#define ledNonLogam 15


// Object Declaration
LiquidCrystal_I2C lcd(0x27, 20, 4);

Servo servoLogam;
Servo servoNonLogam;

HCSR04 objectUltrasonic(uObjectTrigPin, uObjectEchoPin);
HCSR04 heightLogamUltrasonic(uHeightLogamTrigPin, uHeightLogamEchoPin);
HCSR04 heightNonLogamUltrasonic(uHeightNonLogamTrigPin, uHeightNonLogamEchoPin);


// Variable Declaration
bool logamFull = false;
bool nonLogamFull = false;

unsigned long previousMillis = 0;

const char* ssid = "SSID";
const char* password = "Password";
const char* serverName = "rifkibayuariyan.vercel.app";


void lcdClearArea(int columnStart, int columnEnd, int rowStart, int rowEnd) {
  for (int row = rowStart; row <= rowEnd; row++) {
    for (int column = columnStart; column <= columnEnd; column++) {
      lcd.setCursor(column, row);
      lcd.write(0x20);
    }
  }
}

void sendData(String endpoint, String data) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    if (client.connect(serverName, 443)) {
      Serial.println("Terhubung!");

      client.print(String("POST ") + endpoint + " HTTP/1.1\r\n" +
                   "Host: " + serverName + "\r\n" +
                   "Content-Type: application/json\r\n" +
                   "Content-Length: " + data.length() + "\r\n" +
                   "Connection: close\r\n\r\n" +
                   data);

      while (client.connected() || client.available()) {
        if (client.available()) {
          String line = client.readStringUntil('\n');
          Serial.println(line);
        }
      }

      client.stop();
      Serial.println("Koneksi ditutup.");
    } else {
      Serial.println("Koneksi gagal.");
    }
  }
}

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();

  servoLogam.attach(servoLogamPin);
  servoNonLogam.attach(servoNonLogamPin);

  pinMode(ledLogam, OUTPUT);
  pinMode(ledNonLogam, OUTPUT);

  pinMode(uObjectTrigPin, OUTPUT);
  pinMode(uObjectEchoPin, INPUT);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    lcd.setCursor(3, 1);
    lcd.print("Menghubungkan");
    lcd.setCursor(5, 2);
    lcd.print("ke WiFi...");
  }

  lcd.clear();
  lcd.setCursor(5, 1);
  lcd.print("Terhubung");
  lcd.setCursor(6, 2);
  lcd.print("ke WiFi!");
  delay(2000);
  lcd.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  pinMode(inductivePin, INPUT);

  servoLogam.write(50);
  servoNonLogam.write(40);

  lcd.setCursor(0, 0);
  lcd.print("PEMILAH SAMPAH");
  lcd.setCursor(1, 2);
  lcd.print("Logam:");
  lcd.setCursor(7, 2);
  lcd.print(logamFull ? "Penuh" : "Belum Penuh");
  lcd.setCursor(1, 3);
  lcd.print("Non  :");
  lcd.setCursor(7, 3);
  lcd.print(nonLogamFull ? "Penuh" : "Belum Penuh");
  
  // indicator
  digitalWrite(ledLogam, logamFull ? HIGH : LOW);
  digitalWrite(ledNonLogam, nonLogamFull ? HIGH : LOW);

  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;
    if (heightLogamUltrasonic.dist() <= 5) {
      if (!logamFull) {
        lcdClearArea(7, 19, 2, 2);
        sendData("/automatic-waste-sorter/api/send-status", "{\"type\":\"logam\",\"status\":\"full\"}");
      }
      logamFull = true;
    } else {
      if (logamFull) {
        lcdClearArea(7, 19, 2, 2);
        sendData("/automatic-waste-sorter/api/send-status", "{\"type\":\"logam\",\"status\":\"not-full\"}");
      }
      logamFull = false;
    }
    
    if (heightNonLogamUltrasonic.dist() <= 5) {
      if (!nonLogamFull) {
        lcdClearArea(7, 19, 3, 3);
        sendData("/automatic-waste-sorter/api/send-status", "{\"type\":\"non-logam\",\"status\":\"full\"}");
      }
      nonLogamFull = true;
    } else {
      if (nonLogamFull) {
        lcdClearArea(7, 19, 3, 3);
        sendData("/automatic-waste-sorter/api/send-status", "{\"type\":\"non-logam\",\"status\":\"not-full\"}");
      }
      nonLogamFull = false;
    }
  }

  if (objectUltrasonic.dist() < 17) {
    lcdClearArea(0, 19, 1, 3);
    lcd.setCursor(4, 2);
    lcd.print("Mendeteksi ...");
    delay(1000);

    bool logam = digitalRead(inductivePin) == 0 ? true : false;

    if (logam) {
      lcdClearArea(0, 19, 1, 3);
      lcd.setCursor(7, 2);
      lcd.print("LOGAM");
      delay(1000);

      if (logamFull) {
        lcdClearArea(0, 19, 1, 3);
        lcd.setCursor(0, 2);
        lcd.print("Tempat Sampah Penuh!");
        delay(2000);
      } else {
        servoLogam.write(0);
        sendData("/automatic-waste-sorter/api/count", "{\"type\":\"logam\"}");
        delay(1000);
      }
    } else if (!logam) {
      lcdClearArea(0, 19, 1, 3);
      lcd.setCursor(5, 2);
      lcd.print("NON LOGAM");
      delay(1000);

      if (nonLogamFull) {
        lcdClearArea(0, 19, 1, 3);
        lcd.setCursor(0, 2);
        lcd.print("Tempat Sampah Penuh!");
        delay(2000);
      } else {
        servoNonLogam.write(90);
        sendData("/automatic-waste-sorter/api/count", "{\"type\":\"non-logam\"}");
        delay(1000);
      }
    }
    
    lcd.clear();
  }
}
