#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <DHT.h>

// Ganti dengan SSID dan password WiFi Anda
const char* ssid = "Luk34bdl";
const char* password = "12345678";

// Pin relay (pin baru agar tidak bertabrakan dengan LCD I2C)
const int relayPins[3] = {19, 18, 5};  // Relay 1, 2, 3

// Status relay, LOW = relay ON (aktif LOW), HIGH = relay OFF
bool relayStates[3] = {HIGH, HIGH, HIGH};

// Pin sensor soil moisture (analog input)
const int soilMoisturePin = 34;  // Contoh GPIO 34 (ADC1 channel)

// Pin sensor DHT11
#define DHTPIN 4          // Sesuaikan dengan pin data DHT11 yang Anda gunakan
#define DHTTYPE DHT11     // Tipe sensor DHT11

DHT dht(DHTPIN, DHTTYPE);

float temperature = 0.0;
float humidity = 0.0;

// LCD I2C address dan ukuran (biasanya 0x27 atau 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ThingSpeak API keys dan URL
const char* thingspeakWriteAPIKey = "EXTIZBCLAH15P0LZ";
const char* thingspeakReadAPIKey = "YKNJYTL0MLPWFEKV";
const char* thingspeakServer = "http://api.thingspeak.com/update";

WiFiServer server(80);

unsigned long lastThingSpeakUpdate = 0;
const unsigned long thingSpeakInterval = 15000; // 15 detik

unsigned long lastDHTRead = 0;
const unsigned long dhtReadInterval = 2000; // 2 detik

void setup() {
  Serial.begin(115200);

  // Setup relay pins
  for (int i = 0; i < 3; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], relayStates[i]); // Matikan relay awalnya
  }

  // Setup LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Menghubungkan");
  lcd.setCursor(0, 1);
  lcd.print("ke WiFi...");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Terhubung ke WiFi dengan IP: ");
  Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  dht.begin();  // Inisialisasi sensor DHT11

  server.begin();
}

void loop() {
  // Baca sensor soil moisture
  int soilValue = analogRead(soilMoisturePin);

  // Baca sensor DHT11 setiap 2 detik
  if (millis() - lastDHTRead > dhtReadInterval) {
    lastDHTRead = millis();
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Gagal membaca sensor DHT11!");
      // Jika gagal baca, tetap gunakan nilai sebelumnya
    } else {
      Serial.print("Suhu: ");
      Serial.print(temperature);
      Serial.print(" °C, Kelembaban: ");
      Serial.print(humidity);
      Serial.println(" %");
    }
  }

  // Kontrol otomatis relay 1 berdasarkan nilai soil moisture
  if (soilValue >= 3000) {
    // Kekeringan, aktifkan relay 1 (aktif LOW)
    if (relayStates[0] != LOW) {
      relayStates[0] = LOW;
      digitalWrite(relayPins[0], LOW);
      Serial.println("Relay 1 ON (Otomatis - Kekeringan)");
    }
  } else {
    // Lembab, matikan relay 1
    if (relayStates[0] != HIGH) {
      relayStates[0] = HIGH;
      digitalWrite(relayPins[0], HIGH);
      Serial.println("Relay 1 OFF (Otomatis - Lembab)");
    }
  }

  // Tampilkan nilai soil moisture dan DHT11 di LCD
  lcd.setCursor(0, 1);
  lcd.print("Soil:");
  lcd.print(soilValue);
  lcd.print(" ");

  // Tampilkan suhu dan kelembaban di LCD baris kedua jika muat
  // Jika tidak muat, bisa ganti tampilan secara bergantian (opsional)
  if (!isnan(temperature) && !isnan(humidity)) {
    lcd.print("T:");
    lcd.print(temperature, 1);
    lcd.print("C");
    lcd.print("H:");
    lcd.print(humidity, 1);
    lcd.print("%");
  } else {
    lcd.print("T:--C H:--%");
  }

  // Kirim data ke ThingSpeak setiap 15 detik
  if (millis() - lastThingSpeakUpdate > thingSpeakInterval) {
    sendDataToThingSpeak(soilValue, temperature, humidity);
    lastThingSpeakUpdate = millis();
  }

  WiFiClient client = server.available();
  if (!client) {
    delay(100);
    return;
  }

  Serial.println("Client terhubung");
  String currentLine = "";
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      Serial.write(c);
      if (c == '\n') {
        if (currentLine.length() == 0) {
          // Kirim halaman web
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println();

          client.println("<!DOCTYPE html><html>");
          client.println("<head><title>Kontrol Relay 3 Channel ESP32</title></head>");
          client.println("<body><h1>Kontrol Relay 3 Channel</h1>");

          for (int i = 0; i < 3; i++) {
            client.print("<p>Relay ");
            client.print(i + 1);
            client.print(" saat ini: <strong>");
            client.print((relayStates[i] == LOW) ? "ON" : "OFF");
            client.println("</strong></p>");
            // Relay 1 tidak bisa dikontrol manual karena otomatis, tampilkan status saja
            if (i == 0) {
              client.println("<p><em>Relay 1 dikontrol otomatis berdasarkan sensor soil moisture</em></p>");
            } else {
              client.print("<p><a href=\"/relay/");
              client.print(i + 1);
              client.print("/on\"><button>ON</button></a> ");
              client.print("<a href=\"/relay/");
              client.print(i + 1);
              client.print("/off\"><button>OFF</button></a></p>");
            }
          }

          client.print("<p>Soil Moisture Sensor Value: <strong>");
          client.print(soilValue);
          client.println("</strong></p>");

          if (!isnan(temperature) && !isnan(humidity)) {
            client.print("<p>Suhu: <strong>");
            client.print(temperature);
            client.print(" °C</strong></p>");
            client.print("<p>Kelembaban: <strong>");
            client.print(humidity);
            client.println(" %</strong></p>");
          } else {
            client.println("<p>Gagal membaca sensor DHT11</p>");
          }

          client.println("</body></html>");
          break;
        } else {
          // Cek permintaan URL untuk relay 2 dan 3 saja
          if (currentLine.startsWith("GET /relay/")) {
            int relayNum = currentLine.charAt(11) - '0'; // Ambil nomor relay (1,2,3)
            if (relayNum >= 2 && relayNum <= 3) { // Relay 1 otomatis, tidak bisa dikontrol manual
              if (currentLine.indexOf("/on") > 0) {
                relayStates[relayNum - 1] = LOW; // Relay ON
                digitalWrite(relayPins[relayNum - 1], LOW);
                Serial.printf("Relay %d ON\n", relayNum);
              } else if (currentLine.indexOf("/off") > 0) {
                relayStates[relayNum - 1] = HIGH; // Relay OFF
                digitalWrite(relayPins[relayNum - 1], HIGH);
                Serial.printf("Relay %d OFF\n", relayNum);
              }
            }
          }
          currentLine = "";
        }
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }
  delay(1);
  client.stop();
  Serial.println("Client terputus");
}

void sendDataToThingSpeak(int soilValue, float temperature, float humidity) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(thingspeakServer) + "?api_key=" + thingspeakWriteAPIKey +
                 "&field1=" + String(soilValue) +
                 "&field2=" + String(temperature) +
                 "&field3=" + String(humidity);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("Data terkirim ke ThingSpeak, response code: %d\n", httpCode);
    } else {
      Serial.printf("Gagal mengirim data ke ThingSpeak, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("WiFi tidak terhubung, gagal mengirim data ke ThingSpeak");
  }
}