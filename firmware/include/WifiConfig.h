#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Ticker.h>

// Định danh thiết bị và cổng dịch vụ
#define DEVICE_ID           "GSP-TRUCK-01"
#define MQTT_PORT           1883
#define UDP_DISCOVERY_PORT  8888

// Chân phần cứng phục vụ trạng thái mạng và nút Reset cấu hình Wi-Fi
#define PIN_STATUS_LED      38    // LED trạng thái trên bo mạch
#define PIN_WIFI_RESET_BTN  0     // Nút BOOT trên mạch, giữ 5 giây để xóa Wi-Fi
#define PUSHTIME            5000

WebServer webServer(80);
Ticker blinker;
Preferences prefs;

String ssid = "";
String password = "";
String connMode = "opt1";         // "opt1": UDP Broadcast (Mạng chung), "opt2": Gateway IP (máy tính phát Wi-Fi)
String discoveredServerIP = "";   // Lưu cache IP Server tìm được qua UDP

unsigned long lastTimePress = 0;
unsigned long blinkTime = 0;
int wifiMode = 0;                 // 0: AP Mode (Option 3 & Cấu hình), 1: STA Connected, 2: Đang kết nối hoặc mất mạng
volatile bool g_alertLedOverride = false; // Khi Core 0 báo động GSP, nhường quyền sáng cố định LED cho Core 0

const char html[] PROGMEM = R"html( 
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>GSP ESP32-S3 NETWORK SETUP</title>
  <style type="text/css">
    body{display:flex;justify-content:center;align-items:center;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#f0f2f5;margin:0;padding:16px;}
    .card{background:white;padding:22px;border-radius:12px;box-shadow:0 6px 18px rgba(0,0,0,0.08);max-width:360px;width:100%;}
    h3{text-align:center;margin:0 0 14px 0;color:#1a1a1a;font-size:18px;}
    .opt3-banner{font-size:12.5px;color:#0c5460;background:#d1ecf1;border:1px solid #bee5eb;padding:10px;border-radius:8px;margin-bottom:16px;line-height:1.4;}
    .section-title{font-size:13.5px;font-weight:700;color:#333;margin-bottom:8px;display:block;}
    .mode-grid{display:flex;flex-direction:column;gap:8px;margin-bottom:12px;}
    .mode-card{display:flex;align-items:flex-start;gap:10px;padding:10px 12px;border:2px solid #e1e4e8;border-radius:8px;cursor:pointer;transition:all 0.2s;background:#fafbfc;}
    .mode-card.active{border-color:#0066cc;background:#f0f7ff;}
    .mode-card input{margin-top:3px; accent-color:#0066cc;}
    .mode-title{font-size:13.5px;font-weight:700;color:#1a1a1a;display:block;}
    .mode-desc{font-size:12px;color:#555;line-height:1.35;margin-top:2px;display:block;}
    .mode-summary{font-size:12px;padding:8px 10px;border-radius:6px;background:#fff8e6;border-left:4px solid #f59f00;color:#5c4400;margin-bottom:14px;line-height:1.4;}
    input[type="password"], select{margin-bottom:12px;width:100%;height:38px;font-size:14px;box-sizing:border-box;padding:0 10px;border:1px solid #ccd0d5;border-radius:6px;background:white;}
    .btn-group{display:flex;gap:10px;margin-top:6px;}
    button{flex:1;height:40px;border:none;border-radius:6px;cursor:pointer;font-weight:700;font-size:13.5px;}
    .btn-save{background:#0066cc;color:white;}
    .btn-restart{background:#e4e6eb;color:#1a1a1a;}
  </style>
</head>
<body>
  <div class="card">
    <h3>CẤU HÌNH MẠNG GSP ESP32-S3</h3>
    
    <div class="opt3-banner">
      <b>Đang chạy Option 3 (AP Direct):</b><br>
      Nếu máy Mac đang bắt trực tiếp WiFi này của ESP32, bạn <b>không cần điền gì cả</b> (dữ liệu MQTT đang tự bắn về <code>192.168.4.2:1883</code>).
    </div>

    <span class="section-title">Bước 1: Chọn kịch bản mạng muốn chuyển sang</span>
    <div class="mode-grid">
      <label class="mode-card active" id="card_opt1" onclick="selectMode('opt1')">
        <input type="radio" name="mode" value="opt1" checked>
        <div>
          <span class="mode-title">Option 1: Mạng chung (Router / Hotspot ĐT)</span>
          <span class="mode-desc">Cả Mac và ESP32 cùng bắt vào 1 mạng WiFi hoặc Hotspot điện thoại.</span>
        </div>
      </label>

      <label class="mode-card" id="card_opt2" onclick="selectMode('opt2')">
        <input type="radio" name="mode" value="opt2">
        <div>
          <span class="mode-title">Option 2: WiFi do máy Mac phát ra</span>
          <span class="mode-desc">ESP32 bắt trực tiếp vào mạng WiFi phát ra từ Internet Sharing của Mac.</span>
        </div>
      </label>
    </div>

    <div id="mode_explain" class="mode-summary">
      <b>Cơ chế Option 1:</b> ESP32 tự động phát sóng <b>UDP Broadcast (cổng 8888)</b> để tìm IP của MQTT Broker trên máy Mac mà không cần nhập IP thủ công.
    </div>

    <span class="section-title">Bước 2: Nhập thông tin WiFi (2.4GHz)</span>
    <p id="info" style="margin:0 0 8px 0; color:#0066cc; font-size:12.5px;">Đang quét danh sách WiFi xung quanh...</p>
    
    <select id="ssid">
      <option value="">-- Chọn tên mạng WiFi (SSID) --</option>
    </select>
    
    <input id="password" type="password" placeholder="Nhập mật khẩu WiFi">

    <div class="btn-group">
      <button class="btn-save" onclick="saveWifi()">LƯU CẤU HÌNH</button>
      <button class="btn-restart" onclick="reStart()">KHỞI ĐỘNG LẠI</button>
    </div>
  </div>

  <script type="text/javascript">
    window.onload = function(){ scanWifi(); };
    
    function selectMode(m){
      document.getElementById("card_opt1").className = (m === "opt1") ? "mode-card active" : "mode-card";
      document.getElementById("card_opt2").className = (m === "opt2") ? "mode-card active" : "mode-card";
      var exp = document.getElementById("mode_explain");
      if(m === "opt1"){
        exp.innerHTML = "<b>Cơ chế Option 1:</b> ESP32 tự động phát sóng <b>UDP Broadcast (cổng 8888)</b> để tìm IP của MQTT Broker trên máy Mac mà không cần nhập IP thủ công.";
      } else {
        exp.innerHTML = "<b>Cơ chế Option 2:</b> ESP32 lấy trực tiếp <b>IP Gateway</b> của mạng WiFi làm địa chỉ MQTT Broker trên máy Mac (không cần dò UDP).";
      }
    }

    function scanWifi(){
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function(){
        if(xhttp.readyState == 4 && xhttp.status == 200){
          document.getElementById("info").innerHTML = "Quét hoàn tất! Hãy chọn mạng 2.4GHz:";
          var list = JSON.parse(xhttp.responseText);
          var select = document.getElementById("ssid");
          select.length = 1;
          for(var i = 0; i < list.length; ++i){
            select[select.length] = new Option(list[i], list[i]);
          }
        }
      };
      xhttp.open("GET", "/scanWifi", true);
      xhttp.send();
    }

    function saveWifi(){
      var s = encodeURIComponent(document.getElementById("ssid").value);
      var p = encodeURIComponent(document.getElementById("password").value);
      var m = document.querySelector('input[name="mode"]:checked').value;
      if(!s){ alert("Vui lòng chọn tên WiFi!"); return; }
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function(){
        if(xhttp.readyState == 4 && xhttp.status == 200){ alert(xhttp.responseText); }
      };
      xhttp.open("GET", "/saveWifi?ssid=" + s + "&pass=" + p + "&mode=" + m, true);
      xhttp.send();
    }

    function reStart(){
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function(){
        if(xhttp.readyState == 4 && xhttp.status == 200){ alert(xhttp.responseText); }
      };
      xhttp.open("GET", "/reStart", true);
      xhttp.send();
    }
  </script>
</body>
</html>
)html";

inline void blinkLed(uint32_t t) {
    if (millis() - blinkTime > t) {
        digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
        blinkTime = millis();
    }
}

inline void ledControl() {
    if (g_alertLedOverride) {
        digitalWrite(PIN_STATUS_LED, HIGH); // Ưu tiên sáng cố định khi đang có cảnh báo vượt ngưỡng GSP
        return;
    }
    if (digitalRead(PIN_WIFI_RESET_BTN) == LOW) {
        if (millis() - lastTimePress < PUSHTIME) blinkLed(1000);
        else blinkLed(50);
    } else {
        if (wifiMode == 0) blinkLed(100);       // Nháy nhanh: Option 3 (AP Mode)
        else if (wifiMode == 1) blinkLed(3000); // Nháy chậm: Đã kết nối WiFi (Option 1 & 2)
        else if (wifiMode == 2) blinkLed(400);  // Nháy vừa: Đang kết nối WiFi
    }
}

inline void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("[WIFI] Đã kết nối STA! IP: %s | Gateway: %s\n", WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str());
            discoveredServerIP = ""; // Xóa cache để dò lại IP mới nhất của Server
            wifiMode = 1;
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("[WIFI] Mất kết nối WiFi! Đang tự động kết nối lại...");
            wifiMode = 2;
            discoveredServerIP = "";
            WiFi.reconnect();
            break;
        default:
            break;
    }
}

inline void setupWifi() {
    if (ssid != "") {
        Serial.printf("[WIFI] Khởi động chế độ STA -> SSID: [%s] | Mode: [%s]\n", ssid.c_str(), connMode.c_str());
        wifiMode = 2;
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false); // Tắt ngủ WiFi để bắt gói tin UDP phản hồi nhanh và nhạy nhất
        WiFi.onEvent(WiFiEvent);
        WiFi.begin(ssid.c_str(), password.c_str());
    } else {
        WiFi.mode(WIFI_AP_STA);
        WiFi.setSleep(false);
        uint8_t macAddr[6];
        WiFi.softAPmacAddress(macAddr);
        String ssid_ap = "GSP-ESP32-" + String(macAddr[4], HEX) + String(macAddr[5], HEX);
        ssid_ap.toUpperCase();
        WiFi.softAP(ssid_ap.c_str());

        Serial.printf("[WIFI AP] Đang phát WiFi: %s | Web Cấu hình: http://%s\n", ssid_ap.c_str(), WiFi.softAPIP().toString().c_str());
        Serial.println("[WIFI AP] Option 3 sẵn sàng: Kết nối Mac vào WiFi này, MQTT tự bắn về 192.168.4.2:1883"); // [XÓA KHI CHẠY ỔN]
        wifiMode = 0;
    }
}

inline void setupWebServer() {
    webServer.on("/", []() {
        webServer.send(200, "text/html", html);
    });

    webServer.on("/scanWifi", []() {
        int wifi_nets = WiFi.scanNetworks(false, false);
        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();
        for (int i = 0; i < wifi_nets; ++i) {
            array.add(WiFi.SSID(i));
        }
        String response = "";
        serializeJson(doc, response);
        WiFi.scanDelete();
        webServer.send(200, "application/json", response);
    });

    webServer.on("/saveWifi", []() {
        String ssid_temp = webServer.arg("ssid");
        String password_temp = webServer.arg("pass");
        String mode_temp = webServer.arg("mode");
        if (mode_temp != "opt1" && mode_temp != "opt2") mode_temp = "opt1";

        prefs.begin("wifi_cfg", false);
        prefs.putString("ssid", ssid_temp);
        prefs.putString("pass", password_temp);
        prefs.putString("mode", mode_temp);
        prefs.end();

        Serial.printf("[WEB CONFIG] Đã lưu SSID: %s | Chế độ: %s\n", ssid_temp.c_str(), mode_temp.c_str());
        String modeName = (mode_temp == "opt1") ? "Option 1 (UDP Auto-Discovery)" : "Option 2 (Mac Gateway)";
        webServer.send(200, "text/plain", "Đã lưu cấu hình " + modeName + "! Hãy bấm nút KHỞI ĐỘNG LẠI.");
    });

    webServer.on("/reStart", []() {
        webServer.send(200, "text/plain", "ESP32-S3 đang khởi động lại...");
        delay(1000);
        ESP.restart();
    });

    webServer.begin();
}

inline void checkButton() {
    if (digitalRead(PIN_WIFI_RESET_BTN) == LOW) {
        if (millis() - lastTimePress > PUSHTIME) {
            Serial.println("\n[WIFI RESET] Đã giữ nút BOOT 5s -> Xóa cấu hình WiFi, đưa về Option 3 (AP Mode)...");
            prefs.begin("wifi_cfg", false);
            prefs.clear();
            prefs.end();
            delay(500);
            ESP.restart();
        }
    } else {
        lastTimePress = millis();
    }
}

inline String discoverServerViaUDP() {
    WiFiUDP udp;
    udp.begin(0);

    IPAddress subnetBroadcast = (uint32_t)WiFi.localIP() | ~((uint32_t)WiFi.subnetMask());
    IPAddress globalBroadcast(255, 255, 255, 255);

    Serial.printf("[UDP DISCOVERY] Đang phát tín hiệu tìm GSP Server tới %s:%d...\n", subnetBroadcast.toString().c_str(), UDP_DISCOVERY_PORT); // [XÓA KHI CHẠY ỔN]

    for (int attempt = 1; attempt <= 2; attempt++) {
        udp.beginPacket(subnetBroadcast, UDP_DISCOVERY_PORT);
        udp.print("WHO_IS_IOT_SERVER");
        udp.endPacket();

        udp.beginPacket(globalBroadcast, UDP_DISCOVERY_PORT);
        udp.print("WHO_IS_IOT_SERVER");
        udp.endPacket();

        unsigned long startWait = millis();
        while (millis() - startWait < 700) {
            int packetSize = udp.parsePacket();
            if (packetSize > 0) {
                char buf[64];
                int len = udp.read(buf, sizeof(buf) - 1);
                buf[len] = '\0';
                if (String(buf) == "IOT_SERVER_HERE") {
                    String foundIP = udp.remoteIP().toString();
                    Serial.printf("[UDP DISCOVERY] Tìm thấy GSP Server tại IP: %s\n", foundIP.c_str());
                    udp.stop();
                    return foundIP;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    Serial.println("[UDP DISCOVERY] Chưa thấy phản hồi UDP cổng 8888 từ Backend/Mac."); // [XÓA KHI CHẠY ỔN]
    udp.stop();
    return "";
}

class Config {
public:
    void begin() {
        pinMode(PIN_STATUS_LED, OUTPUT);
        pinMode(PIN_WIFI_RESET_BTN, INPUT_PULLUP);
        blinker.attach_ms(50, ledControl);

        prefs.begin("wifi_cfg", false);
        ssid = prefs.getString("ssid", "");
        password = prefs.getString("pass", "");
        connMode = prefs.getString("mode", "opt1");
        prefs.end();

        Serial.printf("[NVS CONFIG] SSID đã lưu: [%s] | Chế độ: [%s]\n", ssid.c_str(), connMode.c_str()); // [XÓA KHI CHẠY ỔN]

        setupWifi();
        if (wifiMode == 0) setupWebServer();
    }

    void run() {
        checkButton();
        if (wifiMode == 0) webServer.handleClient();
    }

    void setAlertLedOverride(bool active) {
        g_alertLedOverride = active;
    }

    bool canSendData() {
        if (wifiMode == 1 && WiFi.status() == WL_CONNECTED) return true;
        if (wifiMode == 0 && WiFi.softAPgetStationNum() > 0) return true;
        return false;
    }

    void printWaitingStatus() {
        if (wifiMode == 0) {
            Serial.println("[CHỜ MẠNG] Đang ở Option 3 (AP Mode) - Chờ máy chủ kết nối vào WiFi ESP32..."); // [XÓA KHI CHẠY ỔN]
        } else if (wifiMode == 2) {
            Serial.printf("[CHỜ MẠNG] Đang kết nối vào WiFi [%s]... (Giữ nút BOOT 5s để xóa mạng)\n", ssid.c_str()); // [XÓA KHI CHẠY ỔN]
        }
    }

    String getTargetIP() {
        if (wifiMode == 0) {
            return "192.168.4.2";
        }
        if (connMode == "opt2") {
            return WiFi.gatewayIP().toString();
        }
        if (discoveredServerIP == "") {
            discoveredServerIP = discoverServerViaUDP();
        }
        return discoveredServerIP;
    }

    void invalidateCache() {
        if (wifiMode == 1 && connMode == "opt1" && discoveredServerIP != "") {
            Serial.println("[UDP CACHE] Đã xóa IP cũ để tự động dò lại IP mới của Server."); // [XÓA KHI CHẠY ỔN]
            discoveredServerIP = "";
        }
    }

    String getCurrentOptionName() {
        if (wifiMode == 0) return "Option 3 (AP Direct -> 192.168.4.2)";
        if (connMode == "opt2") return "Option 2 (Mac Gateway -> " + WiFi.gatewayIP().toString() + ")";
        return "Option 1 (UDP Auto-IP -> " + discoveredServerIP + ")";
    }
} wifiConfig;

#endif // WIFI_CONFIG_H