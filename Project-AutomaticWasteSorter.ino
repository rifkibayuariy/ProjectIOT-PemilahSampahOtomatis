#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SoftwareSerial.h>


// Pin Declaration
#define inductivePin 12

#define servoLogamPin 11
#define servoNonLogamPin 10

#define uObjectTrigPin 9
#define uObjectEchoPin 8

#define uHeightLogamTrigPin 7
#define uHeightLogamEchoPin 6

#define uHeightNonLogamTrigPin 5
#define uHeightNonLogamEchoPin 4

#define ledLogam 3
#define ledNonLogam 2


// Object Declaration
LiquidCrystal_I2C lcd(0x27, 20, 4);

Servo servoLogam;
Servo servoNonLogam;

SoftwareSerial espSerial(2, 3);


// Variable Declaration
bool logamFull = false;
bool nonLogamFull = false;

unsigned long previousMillis = 0;

// Wi-Fi Credentials
String ssid = "SSID";
String password = "Password";

String apiUrl = "http://server.com/automatic-waste-sorter/api";


// Function Declaration
float readUltrasonic(int trigger, int echo) {
  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);

  int duration = pulseIn(echo, HIGH);

  return duration / 58;
}

float readObject() {
  return readUltrasonic(uObjectTrigPin, uObjectEchoPin);
}

float readHeightLogam() {
  return readUltrasonic(uHeightLogamTrigPin, uHeightLogamEchoPin);
}

float readHeightNonLogam() {
  return readUltrasonic(uHeightNonLogamTrigPin, uHeightNonLogamEchoPin);
}

void lcdClearArea(int columnStart, int columnEnd, int rowStart, int rowEnd) {
  for (int row = rowStart; row <= rowEnd; row++) {
    for (int column = columnStart; column <= columnEnd; column++) {
      lcd.setCursor(column, row);
      lcd.write(0x20);
    }
  }
}

String sendCommand(String command, int timeout) {
  String response = "";
  espSerial.println(command);
  long int time = millis();
  while ((time + timeout) > millis()) {
    while (espSerial.available()) {
      char c = espSerial.read();
      response += c;
    }
  }
  Serial.println(response);
  return response;
}

void connectWiFi(String ssid, String password) {
  sendCommand("AT+CWJAP=\"" + ssid + "\",\"" + password + "\"", 5000);
}

String parseHost(String url) {
  int idx = url.indexOf("/", 7);
  if (idx == -1) return url.substring(7);
  return url.substring(7, idx);
}

String parsePath(String url) {
  int idx = url.indexOf("/", 7);
  if (idx == -1) return "/";
  return url.substring(idx);
}

void sendData(String url, String data) {
  String cmd = "AT+CIPSTART=\"TCP\",\"" + parseHost(url) + "\",80";
  sendCommand(cmd, 2000);
  
  String httpRequest = "POST " + parsePath(url) + " HTTP/1.1\r\n";
  httpRequest += "Host: " + parseHost(url) + "\r\n";
  httpRequest += "Content-Type: application/x-www-form-urlencoded\r\n";
  httpRequest += "Content-Length: " + String(data.length()) + "\r\n";
  httpRequest += "\r\n" + data;

  // Send Command to start Sending Data
  cmd = "AT+CIPSEND=" + String(httpRequest.length());
  sendCommand(cmd, 1000);
  
  // Send data HTTP request
  sendCommand(httpRequest, 2000);
  
  // Close Connection
  sendCommand("AT+CIPCLOSE", 1000);
}


void setup() {
  Serial.begin(9600);
  espSerial.begin(115200);

  lcd.init();
  lcd.backlight();

  servoLogam.attach(servoLogamPin);
  servoNonLogam.attach(servoNonLogamPin);

  pinMode(inductivePin, INPUT);

  pinMode(uObjectTrigPin, OUTPUT);
  pinMode(uObjectEchoPin, INPUT);

  pinMode(uHeightLogamTrigPin, OUTPUT);
  pinMode(uHeightLogamEchoPin, INPUT);

  pinMode(uHeightNonLogamTrigPin, OUTPUT);
  pinMode(uHeightNonLogamEchoPin, INPUT);

  pinMode(ledLogam, OUTPUT);
  pinMode(ledNonLogam, OUTPUT);
  
  // Reset ESP-01
  sendCommand("AT+RST", 2000);
  delay(1000);
  
  // Mode Station
  sendCommand("AT+CWMODE=1", 1000);
  
  // Connet to WiFI
  connectWiFi(ssid, password);
}

void loop() {
  unsigned long currentMillis = millis();

  servoLogam.write(0);
  servoNonLogam.write(0);

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
    if (readHeightLogam() <= 5) {
      if (!logamFull) {
        lcdClearArea(7, 19, 2, 2);
        String url = apiUrl + "/sendNotification";
        String data = "type=logam&status=full";
        sendData(url, data);
      }
      logamFull = true;
    } else {
      if (logamFull) lcdClearArea(7, 19, 2, 2);
      logamFull = false;
    }
    
    if (readHeightNonLogam() <= 5) {
      if (!nonLogamFull) {
        lcdClearArea(7, 19, 3, 3);
        String url = apiUrl + "/sendNotification";
        String data = "type=non-logam&status=full";
        sendData(url, data);
      }
      nonLogamFull = true;
    } else {
      if (nonLogamFull) lcdClearArea(7, 19, 3, 3);
      nonLogamFull = false;
    }
  }

  if (readObject() < 30) {
    lcdClearArea(0, 19, 1, 3);
    lcd.setCursor(4, 2);
    lcd.print("Mendeteksi ...");

    bool logam = digitalRead(12) == 0 ? true : false;

    if (logam) {
      lcdClearArea(0, 19, 1, 3);
      lcd.setCursor(7, 2);
      lcd.print("LOGAM");
      delay(2000);

      if (logamFull) {
        lcdClearArea(0, 19, 1, 3);
        lcd.setCursor(0, 2);
        lcd.print("Tempat Sampah Penuh!");
        delay(2000);
      } else {
        servoLogam.write(90);
        String url = apiUrl + "/count";
        String data = "type=logam";
        sendData(url, data);
        delay(3000);
      }
    } else if (!logam) {
      lcdClearArea(0, 19, 1, 3);
      lcd.setCursor(5, 2);
      lcd.print("NON LOGAM");
      delay(2000);

      if (nonLogamFull) {
        lcdClearArea(0, 19, 1, 3);
        lcd.setCursor(0, 2);
        lcd.print("Tempat Sampah Penuh!");
        delay(2000);
      } else {
        servoNonLogam.write(90);
        String url = apiUrl + "/count";
        String data = "type=non-logam";
        sendData(url, data);
        delay(3000);
      }
    }
    lcd.clear();
  }
}
