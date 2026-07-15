#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

// WiFi credentials in separate file for security
//#include "secrets.h"
const char* ssid="Olga_und_Michael";
const char* password="2240527575954629";


// Constants
const unsigned long SCAN_INTERVAL_MS = 10000; // Non-blocking scan delay
const double LOCAL_LAT = 49.0735;
const double LOCAL_LON = 8.7466;
const char* AIRCRAFT_URL = "http://fr24feed2/dump1090/data/aircraft.json";

// State variables
unsigned long lastScanMillis = 0;
int NumberOfAircraftSeen = 0;
double minDist = 999;
double maxDist = 0;
JsonDocument doc(4096);  // Allocate memory for JSON document
JsonDocument aircraftLookupDoc(2048);
JsonDocument routeLookupDoc(2048);

// Graphics and networking
LGFX tft;

String performHttpGet(const char* url);
void drawWiFiIcon(int x, int y, bool connected);
double distanceMeters(double lat1, double lon1, double lat2, double lon2);
bool readAircraftJson();
JsonVariant fetchJsonAndDeserialize(const char* url, JsonDocument& targetDoc);
void connectToWiFi();
void printClosestAircraft();
void showMessage(uint16_t bgColor, const char* line1, const char* line2 = "");

// Aircraft lookup helpers
JsonVariant lookupAircraftByHex(const char* hex);
JsonVariant lookupRouteByCallsign(const char* callsign);

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.invertDisplay(false);
  tft.setRotation(1);
  connectToWiFi();
}

void loop() {
  unsigned long currentMillis = millis();

  // Only perform operations when time has passed
  if (currentMillis - lastScanMillis >= SCAN_INTERVAL_MS) {
    lastScanMillis = currentMillis;

    if (readAircraftJson()) {
      tft.setTextSize(2);
      tft.drawString("Aircraft: " + String(NumberOfAircraftSeen) + " (" + String(minDist, 0) + "-" + String(maxDist, 0) + " km)", 10, 10);
      tft.drawLine(5, 30, 315, 30, TFT_WHITE);
      drawWiFiIcon(290, 3, true);
    } else {
      tft.drawString("Feeder not found.", 10, 10);
      tft.drawLine(5, 30, 315, 30, TFT_WHITE);
      connectToWiFi();
    }

    printClosestAircraft();
    tft.drawLine(5, 238, 320, 238, TFT_BLACK);
    lastScanMillis = millis();
  }
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("fr24monitor");
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    drawWiFiIcon(290, 3, false);
  }

  if (WiFi.status() == WL_CONNECTED) {
    drawWiFiIcon(290, 3, true);
  } else {
    drawWiFiIcon(290, 3, false);
  }
}

double distanceMeters(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0;
  double phi1 = radians(lat1);
  double phi2 = radians(lat2);
  double dPhi = radians(lat2 - lat1);
  double dLambda = radians(lon2 - lon1);

  double a = sin(dPhi / 2) * sin(dPhi / 2) +
             cos(phi1) * cos(phi2) * 
             sin(dLambda / 2) * sin(dLambda / 2);

  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

bool readAircraftJson() {
  minDist = 999;
  maxDist = 0;
  
  String payload = performHttpGet(AIRCRAFT_URL);
  if (payload.isEmpty()) return false;

  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    return false;
  }

  JsonArray aircraftArray = doc["aircraft"].as<JsonArray>();
  char uniqueHex[300][7];
  int uniqueCount = 0;

  for (JsonObject aircraft : aircraftArray) {
    if (aircraft.containsKey("lat") && aircraft.containsKey("lon")) {
      double lat = aircraft["lat"].as<double>();
      double lon = aircraft["lon"].as<double>();
      double distance_km = distanceMeters(LOCAL_LAT, LOCAL_LON, lat, lon) / 1000.0;

      if (distance_km > maxDist) maxDist = distance_km;
      if (distance_km < minDist) minDist = distance_km;
      Serial.printf("Aircraft | Flight | Dist: %.1f km\n", distance_km);
    }
  }

  NumberOfAircraftSeen = uniqueCount;
  return true;
}

JsonVariant lookupAircraftByHex(const char* hex) {
  String url = "https://api.adsbdb.com/v0/aircraft/" + String(hex);
  return fetchJsonAndDeserialize(url.c_str(), aircraftLookupDoc);
}

JsonVariant lookupRouteByCallsign(const char* callsign) {
  String url = "https://api.adsbdb.com/v0/callsign/" + String(callsign);
  return fetchJsonAndDeserialize(url.c_str(), routeLookupDoc);
}

JsonVariant fetchJsonAndDeserialize(const char* url, JsonDocument& targetDoc) {
  targetDoc.clear();
  String payload = performHttpGet(url);
  if (payload.isEmpty()) return JsonVariant();

  DeserializationError error = deserializeJson(targetDoc, payload);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return JsonVariant();
  }
  return targetDoc.as<JsonVariant>();
}

String performHttpGet(const char* url) {
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    return http.getString();
  }

  Serial.printf("HTTP error %d for URL: %s\n", httpCode, url);
  return "";
}

void printClosestAircraft() {
  // Fill in with logic to find and display the closest aircraft
}

void drawWiFiIcon(int x, int y, bool connected) {
  uint16_t color = connected ? TFT_GREEN : TFT_RED;
  tft.fillRect(x, y, 28, 24, TFT_BLACK);
  tft.drawArc(x + 14, y + 20, 14, 12, 220, 320, color);
  tft.drawArc(x + 14, y + 20, 10, 8, 225, 315, color);
  tft.drawArc(x + 14, y + 20, 6, 4, 235, 305, color);
  tft.fillCircle(x + 14, y + 19, 2, color);
}