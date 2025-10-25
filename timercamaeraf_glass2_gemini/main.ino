#include "M5TimerCAM.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "mbedtls/base64.h"
#include <M5UnitGLASS2.h>
#include <ArduinoJson.h>
#include <Wire.h>  
#include "esp_heap_caps.h"
#include "config.h"
#define M5UNITGLASS2_ADDR 0x3C

const char* HOST = "generativelanguage.googleapis.com";
const int   PORT = 443;
const int GLASS2_SDA = 32;     // ← Grove の SDA
const int GLASS2_SCL = 33;     // ← Grove の SCL
 const uint8_t GLASS2_ADDR = 0x3C;

WiFiClientSecure client;
M5UnitGLASS2 glass2;        


#include "esp_heap_caps.h"

void print_mem(const char* tag) {
  Serial.printf("[mem] %s: heap=%u, largest=%u, psram=%u, ps_largest=%u\n",
    tag,
    heap_caps_get_free_size(MALLOC_CAP_8BIT),
    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
    heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
    heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  for (int i = 0; WiFi.status() != WL_CONNECTED && i < 40; ++i) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP=");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("WiFi connect failed");
  return false;
}

void setupCameraLowRes() {
  if (!TimerCAM.Camera.begin()) {
    Serial.println("Camera Init Fail");
    while (1) delay(1000);
  }
  sensor_t* s = TimerCAM.Camera.sensor;
  s->set_pixformat(s, PIXFORMAT_JPEG);
  s->set_framesize(s, FRAMESIZE_QQVGA); // 320x240 (小さくしてメモリ節約)
  s->set_quality(s, 30);               // 数字大きい=圧縮強い→ファイル小さい（20〜40あたりで調整）
  s->set_vflip(s, 1);
  s->set_hmirror(s, 0);
  Serial.println("Camera low-res configured");
}

void setupGlass2() {
  Wire.begin(GLASS2_SDA, GLASS2_SCL, 400000);  // I2C port0
  if (!glass2.begin()) {                       // ← 引数なしの begin()
    Serial.println("GLASS2 begin failed");
    return;
  }
  glass2.setTextSize(2);
  glass2.clear();
  glass2.setCursor(0, 0);
  glass2.print("GLASS2 OK");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("[BOOT] start");

  // ── Camera (I2C port1) ──
  setupCameraLowRes();
  Serial.println("[CAM] ok");

  // ── Wi-Fi ──
  Serial.println("[WIFI] connect...");
  if (!connectWiFi()) {
    Serial.println("[WIFI] FAIL");
    while (1) delay(1000);
  }
  Serial.println("[WIFI] ok");

  // ── GLASS2 (I2C port0=Wire) ──
  Serial.println("[GLASS2] Wire.begin()");
  setupGlass2();

  // ── setup 完了 ──
  Serial.println("set up done");
}

static inline char b64c(uint8_t x) {
  static const char tbl[] PROGMEM =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  return tbl[x & 0x3F];
}

// data を Base64 にしながら client へ逐次書き込み（改行なし）
size_t streamBase64(WiFiClientSecure &c, const uint8_t* data, size_t len) {
  char out[1024];          // 4の倍数ならOK。小さくてOK
  size_t op = 0, written = 0;

  for (size_t i = 0; i < len; i += 3) {
    uint32_t v = (uint32_t)data[i] << 16;
    bool has2 = (i + 1 < len), has3 = (i + 2 < len);
    if (has2) v |= (uint32_t)data[i + 1] << 8;
    if (has3) v |= data[i + 2];

    out[op++] = b64c((v >> 18) & 0x3F);
    out[op++] = b64c((v >> 12) & 0x3F);
    out[op++] = has2 ? b64c((v >> 6) & 0x3F) : '=';
    out[op++] = has3 ? b64c(v & 0x3F) : '=';

    if (op >= sizeof(out) - 4) {           // ある程度貯まったら送る
      c.write((const uint8_t*)out, op);
      written += op;
      op = 0;
    }
  }
  if (op) {
    c.write((const uint8_t*)out, op);
    written += op;
  }
  return written;
}

// ── ヘッダ解析 → Body抽出（chunked対応・gzipは回避） ──
String readHttpBody(WiFiClientSecure &c) {
  String status = c.readStringUntil('\n'); // e.g. "HTTP/1.1 200 OK\r"
  status.trim();
  Serial.println(status);

  bool chunked = false;
  long contentLength = -1;
  String contentEncoding;

  while (true) {
    String h = c.readStringUntil('\n'); // "Header: value\r\n"
    if (h == "\r" || h.length() <= 1) break; // 空行でヘッダ終わり
    String low = h; low.trim(); low.toLowerCase();
    if (low.startsWith("content-length:")) {
      int i = low.indexOf(':'); if (i >= 0) contentLength = low.substring(i+1).toInt();
    } else if (low.startsWith("transfer-encoding:") && low.indexOf("chunked") >= 0) {
      chunked = true;
    } else if (low.startsWith("content-encoding:")) {
      contentEncoding = low;
    }
    Serial.print("> "); Serial.print(h); // デバッグ
  }

  // gzip だとボディは圧縮バイナリになるので、回避できていない時は注意
  if (contentEncoding.indexOf("gzip") >= 0) {
    Serial.println("[warn] gzip圧縮のレスポンスです（Accept-Encoding: identity を送って回避推奨）");
  }

  String body;

  if (chunked) {
    while (true) {
      String line = c.readStringUntil('\n'); // "size-in-hex\r\n"
      line.trim();
      int sc = line.indexOf(';'); if (sc >= 0) line = line.substring(0, sc);
      long chunkSize = strtol(line.c_str(), nullptr, 16);
      if (chunkSize <= 0) {
        // trailerを読み飛ばし
        c.readStringUntil('\n');
        break;
      }
      // まとめて読む（PoC簡易版：1バイトずつでOK）
      while (chunkSize-- > 0) {
        int ch = c.read(); if (ch < 0) break;
        body += (char)ch;
      }
      // CRLF
      c.read(); c.read();
    }
  } else if (contentLength >= 0) {
    body.reserve(contentLength);
    while ((int)body.length() < contentLength) {
      int ch = c.read(); if (ch < 0) break;
      body += (char)ch;
    }
  } else {
    // フォールバック
    while (c.connected() || c.available()) {
      int ch = c.read(); if (ch < 0) break;
      body += (char)ch;
    }
  }
  return body;
}

// 最初の text だけ抜く（省メモリ用フィルタ）
String extractFirstTextFiltered(const String& body) {
  StaticJsonDocument<256> filter;
  filter["candidates"][0]["content"]["parts"][0]["text"] = true;

  DynamicJsonDocument doc(2048);
  auto err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (err) { Serial.print("JSON error: "); Serial.println(err.c_str()); return ""; }
  return String(doc["candidates"][0]["content"]["parts"][0]["text"] | "");
}
// Base64を逐次で流す関数はそのまま使う（streamBase64）

String sendToGemini_stream_local(const uint8_t* jpg, size_t jpg_len) {
  const char* HOST = "generativelanguage.googleapis.com";
  const int   PORT = 443;

  // 事前にContent-Lengthだけ算出
  size_t b64_len = 4 * ((jpg_len + 2) / 3);
  const char* prefix =
    "{\"contents\":[{\"parts\":[{\"inline_data\":{"
    "\"mime_type\":\"image/jpeg\",\"data\":\"";
  const char* suffix =
    "\"}},{\"text\":\"Explain pic within 5 words\"}]}]}";
  size_t content_len = strlen(prefix) + b64_len + strlen(suffix);

  WiFiClientSecure c;             // ★ ローカルに確保（都度破棄）
  c.setInsecure();                // PoC用
  c.setTimeout(20000);
  static const char* alpn[] = {"http/1.1", nullptr};   // ★ h2を使わない
  c.setAlpnProtocols(alpn);

  Serial.println("Connecting to Gemini...");
  bool ok = false;
  for (int i = 0; i < 50 && !ok; ++i) {                 // リトライ
    if (c.connect(HOST, PORT)) ok = true;
    else { Serial.println("TLS connect failed (retry)"); delay(100); }
  }
  if (!ok) { Serial.println("TLS connect failed"); return ""; }

  // ヘッダ送信
  String path = "/v1beta/models/gemini-2.5-flash-lite:generateContent?key=" + String(GEMINI_API_KEY);
  c.println(String("POST ") + path + " HTTP/1.1");
  c.print  ("Host: "); c.println(HOST);
  c.println("User-Agent: TimerCAM-PoC/stream/1.0");
  c.println("Accept: application/json");
  c.println("Accept-Encoding: identity");
  c.println("Content-Type: application/json; charset=utf-8");
  c.print  ("Content-Length: "); c.println(content_len);
  c.println("Connection: close");
  c.println();

  // ボディは逐次
  c.print(prefix);
  streamBase64(c, jpg, jpg_len);
  c.print(suffix);

  // レスポンス
  Serial.println("Reading response...");
  String body = readHttpBody(c);
  c.stop();

  Serial.println("---- BODY ----");
  Serial.println(body);
  Serial.println("--------------");

  String text = extractFirstTextFiltered(body);
  Serial.println("---- TEXT ----");
  Serial.println(text);
  Serial.println("--------------");
  return text;
}
// String sendToGemini_stream(const uint8_t* jpg, size_t jpg_len) {
//   const char* HOST = "generativelanguage.googleapis.com";
//   const int   PORT = 443;
//   extern WiFiClientSecure client;

//   client.setTimeout(20000);
//   static const char* alpn[] = {"h2", "http/1.1", nullptr};
//   client.setAlpnProtocols(alpn);

//   // Base64の最終長だけ先に算出（エンコード自体は後で逐次）
//   size_t b64_len = 4 * ((jpg_len + 2) / 3);

//   // JSONの前後（固定文字列）
//   const char* prefix =
//     "{\"contents\":[{\"parts\":[{\"inline_data\":{"
//     "\"mime_type\":\"image/jpeg\",\"data\":\"";
//   const char* suffix =
//     "\"}},{\"text\":\"Caption this image.\"}]}]}";

//   size_t content_len = strlen(prefix) + b64_len + strlen(suffix);

//   // まず TLS 接続（ここが一番メモリを要する）
//   Serial.println("Connecting to Gemini...");
//   bool ok = false;
//   for (int i = 0; i < 3 && !ok; ++i) {       // 3回トライ
//     if (client.connect(HOST, PORT)) ok = true;
//     else { Serial.println("TLS connect failed (retry)"); delay(500); }
//   }
//   if (!ok) { Serial.println("TLS connect failed"); return ""; }

//   // リクエストライン/ヘッダ
//   String path = "/v1beta/models/gemini-2.5-flash-lite:generateContent?key=" + String(GEMINI_API_KEY);
//   client.println(String("POST ") + path + " HTTP/1.1");
//   client.print  ("Host: "); client.println(HOST);
//   client.println("User-Agent: TimerCAM-PoC/stream/1.0");
//   client.println("Accept: application/json");
//   client.println("Accept-Encoding: identity");
//   client.println("Content-Type: application/json; charset=utf-8");
//   client.print  ("Content-Length: "); client.println(content_len);
//   client.println("Connection: close");
//   client.println();

//   // ボディ（大きなStringは作らず、逐次で）
//   client.print(prefix);
//   streamBase64(client, jpg, jpg_len);
//   client.print(suffix);

//   // レスポンス
//   Serial.println("Reading response...");
//   String body = readHttpBody(client);
//   client.stop();

//   Serial.println("---- BODY ----");
//   Serial.println(body);
//   Serial.println("--------------");

//   String text = extractFirstTextFiltered(body);
//   Serial.println("---- TEXT ----");
//   Serial.println(text);
//   Serial.println("--------------");
//   return text;
// }

void loop() {
  if (!TimerCAM.Camera.get()) {
    Serial.println("Capture failed");
    delay(3000);
    return;
  }
  size_t jpg_len = TimerCAM.Camera.fb->len;
  uint8_t* jpg_buf = TimerCAM.Camera.fb->buf;

  print_mem("after capture");

  // ---- PSRAMに退避してfbを返す ----
  uint8_t* jpg_ps = (uint8_t*)ps_malloc(jpg_len);
  if (!jpg_ps) {
    Serial.println("ps_malloc failed");
    TimerCAM.Camera.free();
    delay(3000);
    return;
  }
  memcpy(jpg_ps, jpg_buf, jpg_len);
  TimerCAM.Camera.free();                  // ★ TLS前に返す
  print_mem("after fb return");

  // 送信（下の “② ローカルクライアント化 + ALPN” 版を使う）
  String modelText = sendToGemini_stream_local(jpg_ps, jpg_len);

  free(jpg_ps);

  glass2.clear();
  glass2.setCursor(0, 0);
  glass2.print(modelText.length() ? modelText : "(no text)");
  delay(10000);
}
