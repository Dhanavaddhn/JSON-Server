#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// กำหนดค่าต่างๆ
const char* wifiSSID = "PADHAN";
const char* wifiPassword = "123456789";
const char* serverURL = "http://192.168.96.116:3000/sensors";

// ตัวแปรสำหรับเก็บค่าอุณหภูมิและความชื้น
float temperature;
float humidity;

// ตัวแปรสำหรับนับจำนวนการส่งข้อมูล
unsigned long count = 1;

// อินสแตนซ์ของอุปกรณ์ DHT11
DHT dht11(D4, DHT11);

// อินสแตนซ์ของ WiFi
WiFiClient client;

// อินสแตนซ์ของ HTTPClient
HTTPClient http;

// อินสแตนซ์ของ NTPClient
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 0);

// ฟังก์ชันสำหรับอ่านค่าอุณหภูมิและความชื้น
void readDHT11() {
  temperature = dht11.readTemperature();
  humidity = dht11.readHumidity();
}

// ฟังก์ชันสำหรับรับค่าวันที่และเวลาที่จัดรูปแบบ
String getFormattedDateTime() {
  time_t now = timeClient.getEpochTime() + 6 * 3600;  // ปรับเวลาในโซนเวลาท้องถิ่น
  struct tm *tmStruct = localtime(&now);

  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
           tmStruct->tm_year + 1900, tmStruct->tm_mon + 1, tmStruct->tm_mday,
           tmStruct->tm_hour, tmStruct->tm_min, tmStruct->tm_sec);

  return String(buffer);
}

void setup() {
  Serial.begin(115200);
  
  // ตั้งค่าโหมด WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  
  // รอการเชื่อมต่อ WiFi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  
  // เริ่มต้น NTPClient
  timeClient.begin();
  
  // เริ่มต้น DHT11
  dht11.begin();
}

void loop() {
  static unsigned long lastTime = 0;
  unsigned long timerDelay = 15000;  // หน่วยเป็นมิลลิวินาที

  // ตรวจสอบว่าเวลาผ่านไปพอดีหรือไม่
  if ((millis() - lastTime) > timerDelay) {
    // อ่านค่าอุณหภูมิและความชื้น
    readDHT11();
    
    // อัปเดตเวลาจาก NTPClient
    timeClient.update();
    
    // รับค่าวันที่และเวลาที่จัดรูปแบบ
    String currentDateTime = getFormattedDateTime();

    // ตรวจสอบว่าค่าที่ได้ไม่ได้รับหรือไม่
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor");
    } else {
      // แสดงค่าอุณหภูมิและความชื้นที่ได้
      Serial.printf("Humidity: %.2f%%\n", humidity);
      Serial.printf("Temperature: %.2f°C\n", temperature);

      // สร้าง JSON document
      StaticJsonDocument<200> jsonDocument;
      jsonDocument["humidity"] = humidity;
      jsonDocument["temperature"] = temperature;
      jsonDocument["dateTime"] = currentDateTime;

      // แปลง JSON document เป็น String
      String jsonData;
      serializeJson(jsonDocument, jsonData);

      // ส่งข้อมูลไปยังเซิร์ฟเวอร์
      http.begin(client, serverURL);
      http.addHeader("Content-Type", "application/json");
      int httpResponseCode = http.POST(jsonData);

      // ตรวจสอบการส่งข้อมูล
      if (httpResponseCode > 0) {
        Serial.println("HTTP Response code: " + String(httpResponseCode));
        String payload = http.getString();
        Serial.println("Returned payload:");
        Serial.println(payload);
        count += 1;
      } else {
        Serial.println("Error on sending POST request: " + String(httpResponseCode));
      }
      
      // ปิดการเชื่อมต่อ HTTP
      http.end();
    }

    // อัปเดตเวลาล่าสุด
    lastTime = millis();
  }
}
