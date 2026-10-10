// Pantalla OLED 0.96" SSD1306 (128x64, I2C) con datos en vivo de la
// microestación. Lee la última medición de /api/latest cada minuto.
//
// Conexión (ESP32 DevKit clásico -> OLED):
//   OLED VCC  -> 3V3
//   OLED GND  -> GND
//   OLED SDA  -> GPIO21
//   OLED SCL  -> GPIO22
//   Dirección I2C habitual: 0x3C
//
// Configura tu WiFi y la URL en `config.h` (copia de config.h.example).

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#if __has_include("config.h")
#include "config.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "TU_WIFI"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "TU_CLAVE"
#endif
#ifndef API_URL
#define API_URL "https://tecrural-ms-pantalla.vercel.app/api/latest"
#endif
#ifndef REFRESH_MS
#define REFRESH_MS 60000UL
#endif
#ifndef STATION_ID
#define STATION_ID "esp32c3-01"
#endif
#ifndef MAIN_HEALTH_URL
#define MAIN_HEALTH_URL "https://tecrural-microestacion.vercel.app/health"
#endif
#ifndef OLED_SDA
#define OLED_SDA 21
#endif
#ifndef OLED_SCL
#define OLED_SCL 22
#endif
#ifndef OLED_I2C_ADDR
#define OLED_I2C_ADDR 0x3C
#endif

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
unsigned long lastFetch = 0;
unsigned long lastPage = 0;
unsigned long lastDiagnostic = 0;
bool diagnosticPage = false;
bool databaseOK = false;
bool hasReading = false;
int diagnosticHTTP = 0;
int mainHTTP = 0;
int latestHTTP = 0;
long receptionAge = -1;
long measurementAge = -1;
String receptionState = "unknown";
uint8_t readingFrame[SCREEN_WIDTH * SCREEN_HEIGHT / 8];

JsonObject selectStation(JsonArray stations) {
  for (JsonObject station : stations) {
    if (String(station["id"] | "") == STATION_ID) return station;
  }
  // Nunca sustituir la estación buscada por otra y presentar su medida como propia.
  return JsonObject();
}

void renderDiagnostics() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(WiFi.status() == WL_CONNECTED ? "WiFi OK " : "WiFi NO ");
  display.print(WiFi.RSSI());
  display.print(" dBm");
  display.setCursor(0, 10);
  display.print("Pant API "); display.print(diagnosticHTTP);
  display.print(databaseOK ? " BD OK" : " BD ?");
  display.setCursor(0, 20);
  display.print("Web/BD HTTP "); display.print(mainHTTP);
  unsigned long elapsed = (millis() - lastDiagnostic) / 1000;
  display.setCursor(0, 30);
  if (!databaseOK) display.print("Recepcion sin verificar");
  else if (receptionAge < 0) display.print("Sin recepcion registrada");
  else {
    display.print("Recibido "); display.print((receptionAge + elapsed) / 60); display.print(" min");
  }
  display.setCursor(0, 40);
  if (databaseOK && measurementAge >= 0) {
    display.print("Medida "); display.print((measurementAge + elapsed) / 60); display.print(" min");
  } else display.print("Medida sin verificar");
  display.setCursor(0, 50);
  display.print(!databaseOK ? "Consulta fallo"
    : receptionState == "recent" ? "Recepcion reciente"
    : receptionState == "delayed" ? "Recepcion retrasada" : "Sin datos estacion");
  display.display();
}

void fetchDiagnostics() {
  databaseOK = false;
  receptionAge = -1;
  measurementAge = -1;
  diagnosticHTTP = 0;
  mainHTTP = 0;
  if (WiFi.status() != WL_CONNECTED) return;
  String url(API_URL);
  url = url.substring(0, url.lastIndexOf('/') + 1) + "diagnostics";
#ifdef DIAGNOSTICS_URL
  url = DIAGNOSTICS_URL;
#endif
  WiFiClientSecure client;
  client.setInsecure();  // Mismo transporte del firmware existente.
  HTTPClient http;
  if (http.begin(client, url)) {
    http.setTimeout(10000);
    http.setConnectTimeout(10000);
    http.addHeader("Cache-Control", "no-cache");
    diagnosticHTTP = http.GET();
    if (diagnosticHTTP == HTTP_CODE_OK) {
      JsonDocument doc;
      if (!deserializeJson(doc, http.getString())) {
        databaseOK = String(doc["database"] | "") == "ok";
        JsonObject station = selectStation(doc["stations"].as<JsonArray>());
        if (!station.isNull()) {
          if (!station["receptionAgeSeconds"].isNull()) receptionAge = station["receptionAgeSeconds"].as<long>();
          if (!station["measurementAgeSeconds"].isNull()) measurementAge = station["measurementAgeSeconds"].as<long>();
          receptionState = station["receptionState"].as<String>();
        } else receptionState = "never";
      }
    }
    http.end();
  }
  lastDiagnostic = millis();
  // /health en la app principal ejecuta SELECT 1: comprueba la otra API y su BD,
  // pero no inserta medidas ni actualiza el contacto de la estación.
  if (http.begin(client, MAIN_HEALTH_URL)) {
    http.setTimeout(10000);
    http.setConnectTimeout(10000);
    mainHTTP = http.GET();
    http.end();
  }
  Serial.printf("[diagnostico] pantalla HTTP=%d BD=%s; web/BD HTTP=%d; recepcion=%ld s, estado=%s\n",
                diagnosticHTTP, databaseOK ? "OK" : "NO VERIFICADA", mainHTTP,
                receptionAge, receptionState.c_str());
}

void drawStatusLine(const String &text) {
  display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  if (text.length() > 0) display.print(text);
  display.display();
}

void truncateTo(char *buf, const char *src, size_t maxChars) {
  size_t len = strlen(src);
  size_t take = len > maxChars ? maxChars : len;
  memcpy(buf, src, take);
  buf[take] = '\0';
}

String formatFreshness(long seconds) {
  if (seconds < 0) seconds = 0;
  if (seconds < 90) return String("hace 0 min");
  long minutes = seconds / 60;
  if (minutes < 60) return String("hace ") + String(minutes) + String(" min");
  long hours = minutes / 60;
  long rest = minutes % 60;
  if (rest == 0) return String("hace ") + String(hours) + String(" h");
  return String("hace ") + String(hours) + String(" h ") + String(rest) + String(" min");
}

void renderReading(const String &name, bool hasTemp, float temperatureC,
                   float humidityPct, float pressureHpa, long sequence,
                   const String &freshness) {
  display.clearDisplay();

  char buf[17];
  truncateTo(buf, name.c_str(), 16);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(buf);

  display.setCursor(0, 16);
  display.setTextSize(2);
  if (hasTemp) {
    display.print(String(temperatureC, temperatureC >= 100 || temperatureC <= -10 ? 0 : 1));
  } else {
    display.print("--");
  }
  display.setTextSize(1);
  display.setCursor(76, 22);
  display.print("C");

  display.setCursor(0, 36);
  display.print((humidityPct >= 0) ? String("Hum ") + String(humidityPct, 0) + String(" %")
                                   : "Hum -- %");

  display.setCursor(0, 46);
  display.print((pressureHpa > 0) ? String("Pres ") + String(pressureHpa, 1) + String(" hPa")
                                  : "Pres -- hPa");

  display.setCursor(0, 56);
  display.print(freshness);
  if (sequence > 0) {
    display.setCursor(78, 56);
    display.print(String("seq ") + String(sequence));
  }

  display.display();
  memcpy(readingFrame, display.getBuffer(), sizeof(readingFrame));
  hasReading = true;
}

void fetchLatest() {
  latestHTTP = 0;
  if (WiFi.status() != WL_CONNECTED) {
    drawStatusLine("Sin WiFi");
    WiFi.reconnect();
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, API_URL)) {
    drawStatusLine("Error al iniciar HTTP");
    return;
  }
  http.setTimeout(10000);
  http.setConnectTimeout(10000);

  int code = http.GET();
  latestHTTP = code;
  Serial.printf("[lectura] HTTP=%d\n", code);
  if (code != HTTP_CODE_OK) {
    drawStatusLine("Error HTTP " + String(code));
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  if (payload.length() == 0) {
    drawStatusLine("Respuesta vacia");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    drawStatusLine("JSON invalido");
    return;
  }

  JsonArray stations = doc["stations"].as<JsonArray>();
  JsonObject st = selectStation(stations);

  if (st.isNull()) {
    drawStatusLine("Sin estaciones");
    return;
  }

  long freshness = st["freshnessSeconds"].isNull() ? -1 : st["freshnessSeconds"].as<long>();
  if (freshness < 0) freshness = -1;

  renderReading(
    st["name"].as<String>(),
    !st["temperatureC"].isNull(),
    st["temperatureC"].isNull() ? NAN : st["temperatureC"].as<float>(),
    st["humidityPct"].isNull() ? -1 : st["humidityPct"].as<float>(),
    st["pressureHpa"].isNull() ? 0 : st["pressureHpa"].as<float>(),
    st["sequence"].as<long>(),
    freshness >= 0 ? formatFreshness(freshness) : "dato antiguo");
}

void setup() {
  Serial.begin(115200);
  lastFetch = millis() - REFRESH_MS;

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("OLED no encontrado en la direccion 0x3C");
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("TecRural MS");
  display.setCursor(0, 16);
  display.print("Conectando WiFi...");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long startWait = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startWait < 15000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP ");
    Serial.println(WiFi.localIP());
  } else {
    drawStatusLine("Sin WiFi");
    return;
  }

  lastFetch = millis() - REFRESH_MS;  // primera comprobación inmediata
}

void loop() {
  if (millis() - lastFetch >= REFRESH_MS) {
    lastFetch = millis();
    fetchLatest();
    fetchDiagnostics();
    diagnosticPage = true;
    lastPage = millis();
    renderDiagnostics();
  }
  if (millis() - lastPage >= 5000) {
    lastPage = millis();
    diagnosticPage = !diagnosticPage;
    if (diagnosticPage || !hasReading || latestHTTP != HTTP_CODE_OK) renderDiagnostics();
    else {
      memcpy(display.getBuffer(), readingFrame, sizeof(readingFrame));
      display.display();
    }
  }
  delay(100);
}
