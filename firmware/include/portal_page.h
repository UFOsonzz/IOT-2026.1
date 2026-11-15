#pragma once

#include <Arduino.h>

const char PORTAL_HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>GSP ESP32 NETWORK SETUP</title>
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
    <h3>CẤU HÌNH MẠNG GSP ESP32</h3>
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