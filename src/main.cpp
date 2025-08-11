#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <vector> // Thêm thư viện để lưu danh sách ID đã gửi

const char* ssid = "HOANG WIFI";
const char* password = "hhhhhhh1";

String apiGetUrl = "https://sms-api-ncid.onrender.com/roitai";
String apiPostUrl = "https://sms-api-ncid.onrender.com/update-sms";

HardwareSerial sim800(1);
#define SIM800_TX 17
#define SIM800_RX 16
#define SIM800_BAUD 115200

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 60000;

// ====== Danh sách ID đã gửi SMS trong phiên hiện tại ======
std::vector<String> sentIDs;

bool hasSentBefore(String id) {
  for (String sid : sentIDs) {
    if (sid == id) return true;
  }
  return false;
}

void markAsSent(String id) {
  sentIDs.push_back(id);
  if (sentIDs.size() > 200) sentIDs.erase(sentIDs.begin()); // Giữ tối đa 200 ID
}

// ====== Hàm loại bỏ dấu tiếng Việt ======
String removeVietnameseAccents(String str) {
  const char* find[] = {"á","à","ả","ã","ạ","ă","ắ","ằ","ẳ","ẵ","ặ","â","ấ","ầ","ẩ","ẫ","ậ",
                        "đ",
                        "é","è","ẻ","ẽ","ẹ","ê","ế","ề","ể","ễ","ệ",
                        "í","ì","ỉ","ĩ","ị",
                        "ó","ò","ỏ","õ","ọ","ô","ố","ồ","ổ","ỗ","ộ","ơ","ớ","ờ","ở","ỡ","ợ",
                        "ú","ù","ủ","ũ","ụ","ư","ứ","ừ","ử","ữ","ự",
                        "ý","ỳ","ỷ","ỹ","ỵ",
    "Á","À","Ả","Ã","Ạ","Ă","Ắ","Ằ","Ẳ","Ẵ","Ặ","Â","Ấ","Ầ","Ẩ","Ẫ","Ậ",
    "Đ",
    "É","È","Ẻ","Ẽ","Ẹ","Ê","Ế","Ề","Ể","Ễ","Ệ",
    "Í","Ì","Ỉ","Ĩ","Ị",
    "Ó","Ò","Ỏ","Õ","Ọ","Ô","Ố","Ồ","Ổ","Ỗ","Ộ","Ơ","Ớ","Ờ","Ở","Ỡ","Ợ",
    "Ú","Ù","Ủ","Ũ","Ụ","Ư","Ứ","Ừ","Ử","Ữ","Ự",
    "Ý","Ỳ","Ỷ","Ỹ","Ỵ"
                        };
  const char* repl[] = {"a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a",
                        "d",
                        "e","e","e","e","e","e","e","e","e","e","e",
                        "i","i","i","i","i",
                        "o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o",
                        "u","u","u","u","u","u","u","u","u","u","u",
                        "y","y","y","y","y",
    "A","A","A","A","A","A","A","A","A","A","A","A","A","A","A","A","A",
    "D",
    "E","E","E","E","E","E","E","E","E","E","E",
    "I","I","I","I","I",
    "O","O","O","O","O","O","O","O","O","O","O","O","O","O","O","O","O",
    "U","U","U","U","U","U","U","U","U","U","U",
    "Y","Y","Y","Y","Y"
                        };
  for (int i = 0; i < sizeof(find) / sizeof(find[0]); i++) {
    str.replace(find[i], repl[i]);
  }
  return str;
}

// ====== Hàm format tiền ======
String formatMoney(long money) {
  String s = String(money);
  String res = "";
  int len = s.length();
  int count = 0;
  for (int i = len - 1; i >= 0; i--) {
    res = s.charAt(i) + res;
    count++;
    if (count == 3 && i != 0) {
      res = "." + res;
      count = 0;
    }
  }
  return res;
}

// ====== Hàm gửi SMS ======
bool sendSMS(String phoneNumber, String message) {
  sim800.println("AT+CMGF=1"); // Chế độ text
  delay(500);
  sim800.print("AT+CMGS=\"");
  sim800.print(phoneNumber);
  sim800.println("\"");
  delay(500);
  sim800.print(message);
  delay(500);
  sim800.write(26); // Ctrl+Z
  delay(5000);

  String response = "";
  unsigned long start = millis();
  while (millis() - start < 10000) { // Chờ tối đa 10 giây
    while (sim800.available()) {
      char c = sim800.read();
      response += c;
    }
    if (response.indexOf("+CMGS:") != -1) {
      Serial.println("[Phan hoi Module 4G] " + response);
      return true; // Gửi thành công
    }
    if (response.indexOf("ERROR") != -1) {
      Serial.println("[Module 4G] " + response);
      return false; // Lỗi gửi
    }
  }
  Serial.println("[Module 4G] " + response);
  return false; // Hết thời gian chờ
}


// ====== Hàm update trạng thái SMS ======
void updateSMSStatus(String id, String status) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiPostUrl);
    http.addHeader("Content-Type", "application/json");
    String jsonPayload = "{\"id\":\"" + id + "\",\"status\":\"" + status + "\"}";
    int httpCode = http.POST(jsonPayload);
    Serial.println("📌 [Update SMS] Ma HTTP: " + String(httpCode));
    String payload = http.getString();
    Serial.println("📌 [Update SMS] Phan hoi: " + payload);
    http.end();
  }
}

// ====== Hàm kiểm tra & gửi SMS ======
void checkAndSendSMS() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiGetUrl);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println("📥 [Phản hồi API] " + payload);

      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error && doc.is<JsonArray>()) {
        for (JsonObject obj : doc.as<JsonArray>()) {
          String id = String((int)obj["id"]);
          String name = obj["name"].as<String>();
          String phone = obj["phone"].as<String>();
          String iphone = obj["iphone"].as<String>();
          String imei = obj["imei"].as<String>();
          String loi = obj["loi"].as<String>();
          String tienText = obj["tienText"].as<String>();
          String thanhtoan = obj["thanhtoan"].as<String>();
          String thoigian = obj["thoigian"].as<String>();
          String totalDebt = String((int)obj["totalDebt"]);
          int soLuongNo = obj["soLuongNo"];

          tienText.replace("₫", "VND");
          tienText = removeVietnameseAccents(tienText);

          loi.replace("₫", "VND");
          loi = removeVietnameseAccents(loi);

          long totalDebtValue = totalDebt.toInt();
          String smsContent;

          String smsStatus = obj["sms"].as<String>();
          String thanhToanStatus = thanhtoan;

          if (smsStatus == "Send" && thanhToanStatus == "TT") {
            smsContent = "TB: " + name + "\n" +
                         "Ban da " + loi + "\n";
            if (totalDebtValue == 0) {
              smsContent += "Time: " + thoigian + "\n";
            } else {
              smsContent += "Time: " + thoigian + "\n";
              smsContent += "TONG NO: " + formatMoney(totalDebtValue) + " VND\n";
            }
            smsContent += "https://hoanglsls.web.app";
          } else if (thanhToanStatus == "Ok" && smsStatus == "Yes") {
            if (totalDebtValue == 0) {
              smsContent =
                "ID:" + id + " " + name + "\n" +
                "Thanh toan thanh cong\n" +
                "May: " + iphone + "\n" +
                "Imei: " + imei + "\n" +
                "Loi: " + loi + "\n" +
                "So tien: " + tienText + "\n" +
                "Thanh toan: " + thanhtoan + "\n" +
                "Time: " + thoigian + "\n";
            } else {
              smsContent =
                "ID:" + id + " " + name + "\n" +
                "May: " + iphone + "\n" +
                "Imei: " + imei + "\n" +
                "Loi: " + loi + "\n" +
                "So tien: " + tienText + "\n" +
                "Thanh toan: " + thanhtoan + "\n" +
                "Time: " + thoigian + "\n" +
                "Tong no (" + String(soLuongNo) + " may): " + formatMoney(totalDebtValue) + " VND\n";
            }
          } else {
            smsContent =
              "ID:" + id + " " + name + "\n" +
              "May: " + iphone + "\n" +
              "Imei: " + imei + "\n" +
              "Loi: " + loi + "\n" +
              "So tien: " + tienText + "\n" +
              "Thanh toan: " + thanhtoan + "\n" +
              "Time: " + thoigian + "\n" +
              "Tong no (" + String(soLuongNo) + " may): " + formatMoney(totalDebtValue) + " VND";
          }

          smsContent = removeVietnameseAccents(smsContent);
          Serial.println("📌 [View SMS]\n" + smsContent);

          if (!hasSentBefore(id)) {
            bool success = sendSMS(phone, smsContent);
            if (success) {
              markAsSent(id);
              Serial.println("✅ Da gui SMS toi: " + phone);
              updateSMSStatus(id, "Done");
            } else {
              Serial.println("❌ Gui SMS that bai tai: " + phone);
              updateSMSStatus(id, "Error");
            }
          } else {
            Serial.println("⏩ ID " + id + " Da gui SMS truoc do roi. Thu update backend lai...");
            updateSMSStatus(id, "Done");
          }

          delay(5000);
        }
      }
    } else {
      Serial.println("❌ [Lỗi] API GET that bai, Code: " + String(httpCode));
    }
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("📥 Dang ket noi WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" ✅ Da ket noi Wifi!");

  sim800.begin(SIM800_BAUD, SERIAL_8N1, SIM800_RX, SIM800_TX);
  Serial.println("📌 Module 4G da khoi tao");
}

void loop() {
  if (millis() - lastCheckTime > checkInterval) {
    lastCheckTime = millis();
    checkAndSendSMS();
  }
}
