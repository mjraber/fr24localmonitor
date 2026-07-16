#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>


// Use C strings for WiFi functions compatibility
char ssid[] = "Olga_und_Michael";
char password[] = "2240527575954629";

// Delay between scans
const int seconds = 10;

// your locaL coordinates
const double local_lat = 49.0735;
const double local_lon = 8.7466;
const double local_alt_meter = 205;

// replace yb your feeder IP or name
const char* aircraftUrl =
  "http://fr24feed2/dump1090/data/aircraft.json";

int NumberOfAircraftSeen = 0;
double minDist = 999;
double maxDist = 0;
JsonObject closestAircraft;
JsonObject closestRoute;
String url_picture;

JsonDocument doc;  // Stores the aircrafts

/*
  Persistent JSON documents for the ADSBdb lookups.

  These must remain alive after lookupAircraftByHex() and
  lookupRouteByCallsign() return. Otherwise, the returned
  JsonVariant objects would point to destroyed local documents.
*/
JsonDocument aircraftLookupDoc;
JsonDocument routeLookupDoc;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_Parallel8 _bus;

public:
  LGFX(void) {
    {
      auto cfg = _bus.config();

      cfg.freq_write = 4000000;

      cfg.pin_wr = 26;  // brown
      cfg.pin_rd = -1;  // red, inactive
      cfg.pin_rs = 25;  // black

      cfg.pin_d0 = 16;  // yellow
      cfg.pin_d1 = 17;  // orange
      cfg.pin_d2 = 18;  // red
      cfg.pin_d3 = 19;  // brown
      cfg.pin_d4 = 21;  // black
      cfg.pin_d5 = 22;  // grey
      cfg.pin_d6 = 23;  // white
      cfg.pin_d7 = 15;  // green

      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    {
      auto cfg = _panel.config();

      cfg.pin_cs = 33;    // white
      cfg.pin_rst = 32;   // grey
      cfg.pin_busy = -1;  // inactive

      cfg.panel_width = 240;
      cfg.panel_height = 320;

      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;

      cfg.readable = false;
      cfg.rgb_order = false;

      cfg.dlen_16bit = false;
      cfg.bus_shared = false;

      _panel.config(cfg);
    }

    setPanel(&_panel);
  }
};

LGFX tft;

double distanceMeters(
  double lat1,
  double lon1,
  double lat2,
  double lon2) {
  const double R = 6371000.0;

  double phi1 = lat1 * DEG_TO_RAD;
  double phi2 = lat2 * DEG_TO_RAD;
  double dPhi = (lat2 - lat1) * DEG_TO_RAD;
  double dLambda = (lon2 - lon1) * DEG_TO_RAD;

  double a =
    sin(dPhi / 2) * sin(dPhi / 2) + cos(phi1) * cos(phi2) * sin(dLambda / 2) * sin(dLambda / 2);

  double c = 2 * atan2(sqrt(a), sqrt(1 - a));

  return R * c;
}

// =====================
// ADSBdb LOOKUPS
// =====================

String httpGetString(const String& url) {
  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    http.end();
    return payload;
  }

  if (httpCode == HTTP_CODE_NOT_FOUND) {
    String payload = http.getString();

    Serial.print("No ADSBdb record for URL: ");
    Serial.println(url);

    if (payload.length() > 0) {
      Serial.print("ADSBdb response: ");
      Serial.println(payload);
    }
    
    http.end();
    return "";
  }

  Serial.print("HTTP error ");
  Serial.print(httpCode);
  Serial.print(" for URL: ");
  Serial.println(url);

  http.end();
  return "";
}

JsonVariant lookupAircraftByHex(const String& hex) {
  aircraftLookupDoc.clear();
  if (hex.length() == 0) {
    return JsonVariant();
  }

  String url =
    "https://api.adsbdb.com/v0/aircraft/" + hex;

  String payload = httpGetString(url);

  if (payload.length() == 0) {
    Serial.println("Aircraft lookup failed.");
    return JsonVariant();
  }

  aircraftLookupDoc.clear();

  DeserializationError error =
    deserializeJson(aircraftLookupDoc, payload);

  if (error) {
    Serial.print("Aircraft JSON parse error: ");
    Serial.println(error.c_str());
    return JsonVariant();
  }

  JsonVariant aircraft =
    aircraftLookupDoc["response"]["aircraft"];

  String registration =
    aircraft["registration"] | "-";

  String type =
    aircraft["type"] | "-";

  String icaoType =
    aircraft["icao_type"] | "-";

  String manufacturer =
    aircraft["manufacturer"] | "-";

  String owner =
    aircraft["owner"] | "-";

  // Serial.print("Registration: ");
  // Serial.println(registration);

  // Serial.print("Type:         ");
  // Serial.println(type);

  // Serial.print("ICAO type:    ");
  // Serial.println(icaoType);

  // Serial.print("Manufacturer: ");
  // Serial.println(manufacturer);

  // Serial.print("Owner:        ");
  // Serial.println(owner);

  return aircraftLookupDoc["response"]["aircraft"];
}

JsonVariant lookupRouteByCallsign(const String& callsign) {
  routeLookupDoc.clear();
  if (callsign.length() == 0) {
    return JsonVariant();
  }

  String url =
    "https://api.adsbdb.com/v0/callsign/" + callsign;

  String payload = httpGetString(url);

  if (payload.length() == 0) {
    Serial.println("Route lookup failed.");
    return JsonVariant();
  }

  routeLookupDoc.clear();

  DeserializationError error =
    deserializeJson(routeLookupDoc, payload);

  if (error) {
    Serial.print("Route JSON parse error: ");
    Serial.println(error.c_str());
    return JsonVariant();
  }

  JsonVariant response =
    routeLookupDoc["response"];

  // Serial.println(
  //  routeLookupDoc["response"].as<String>());

  JsonVariant route =
    response["flightroute"];

  JsonVariant airline =
    route["airline"];

  String airlineName =
    airline["name"] | "-";

  String airlineIata =
    airline["iata"] | "-";

  String airlineIcao =
    airline["icao"] | "-";

  String routeCallsign =
    route["callsign"] | callsign;

  JsonVariant origin =
    route["origin"];

  JsonVariant dest =
    route["destination"];

  String originIata =
    origin["iata_code"] | "-";

  String originIcao =
    origin["icao_code"] | "-";

  String originName =
    origin["name"] | "-";

  String destIata =
    dest["iata_code"] | "-";

  String destIcao =
    dest["icao_code"] | "-";

  String destName =
    dest["name"] | "-";

  // Serial.print("Airline:      ");
  // Serial.print(airlineName);
  // Serial.print(" / ");
  // Serial.print(airlineIata);
  // Serial.print(" / ");
  // Serial.println(airlineIcao);

  // Serial.print("Flight:       ");
  // Serial.println(routeCallsign);

  // Serial.print("Origin:       ");
  // Serial.print(originIata);
  // Serial.print(" / ");
  // Serial.print(originIcao);
  // Serial.print(" / ");
  // Serial.println(originName);

  // Serial.print("Destination:  ");
  // Serial.print(destIata);
  // Serial.print(" / ");
  // Serial.print(destIcao);
  // Serial.print(" / ");
  // Serial.println(destName);

  return routeLookupDoc["response"];
}

void drawWiFiIcon(
  int x,
  int y,
  bool connected) {
  uint16_t color =
    connected ? TFT_GREEN : TFT_RED;

  tft.fillRect(
    x,
    y,
    28,
    24,
    TFT_BLACK);

  tft.drawArc(
    x + 14,
    y + 20,
    14,
    12,
    220,
    320,
    color);

  tft.drawArc(
    x + 14,
    y + 20,
    10,
    8,
    225,
    315,
    color);

  tft.drawArc(
    x + 14,
    y + 20,
    6,
    4,
    235,
    305,
    color);

  tft.fillCircle(
    x + 14,
    y + 19,
    2,
    color);

  if (!connected) {
    tft.drawLine(
      x + 4,
      y + 4,
      x + 24,
      y + 22,
      TFT_RED);

    tft.drawLine(
      x + 5,
      y + 4,
      x + 25,
      y + 22,
      TFT_RED);
  }
}

void showMessage(
  uint16_t bgColor,
  const String& line1,
  const String& line2 = "") {
  tft.fillScreen(bgColor);
  tft.setTextColor(TFT_WHITE, bgColor);
  tft.setTextSize(2);

  tft.drawString(line1, 10, 20);

  if (line2.length() > 0) {
    tft.drawString(line2, 10, 50);
  }
}

void connectToWiFi() {
  // Use WiFi.mode for compatibility with the WiFiClass API
  //WiFi..mode(WIFI_STA);
  // Disable WiFi sleep if supported (older cores may not have setSleep)
  #ifdef ARDUINO_ARCH_ESP8266
  WiFi.setSleep(false);
  #endif
  WiFi.begin(ssid, password);

  int attempts = 0;

  while (
    WiFi.status() != WL_CONNECTED && attempts < 100) {
    delay(500);
    Serial.print(".");
    attempts++;

    drawWiFiIcon(290, 3, false);
  }

  Serial.println();

  tft.init();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");

    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());

    drawWiFiIcon(290, 3, true);
  } else {
    Serial.println("WiFi connection failed");
    drawWiFiIcon(290, 3, false);
  }
}

bool readAircraftJson() {
  minDist = 999;
  maxDist = 0;
  closestAircraft = JsonObject();

  HTTPClient http;
  http.begin(aircraftUrl);
  http.setTimeout(5000);

  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print(
      "HTTP GET failed or non-OK response: ");
    Serial.println(httpCode);

    if (httpCode < 0) {
      Serial.println(
        http.errorToString(httpCode));
    }

    NumberOfAircraftSeen = 0;
    http.end();

    return false;
  }

  String payload = http.getString();
  http.end();

  DeserializationError error =
    deserializeJson(doc, payload);

  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());

    NumberOfAircraftSeen = 0;
    return false;
  }

  JsonArray aircraftArray =
    doc["aircraft"].as<JsonArray>();

  char uniqueHex[300][7];
  int uniqueCount = 0;

  for (JsonObject aircraft : aircraftArray) {
    const char* hexCode = aircraft["hex"];

    if (
      hexCode == nullptr || strlen(hexCode) == 0) {
      continue;
    }

    bool alreadyExists = false;

    for (int i = 0; i < uniqueCount; i++) {
      if (
        strcmp(uniqueHex[i], hexCode) == 0) {
        alreadyExists = true;
        break;
      }
    }

    if (
      !alreadyExists && uniqueCount < 300) {
      strncpy(
        uniqueHex[uniqueCount],
        hexCode,
        6);

      uniqueHex[uniqueCount][6] = '\0';
      uniqueCount++;
    }

    if (
      aircraft["lat"].is<double>() && aircraft["lon"].is<double>()) {
      double lat =
        aircraft["lat"].as<double>();

      double lon =
        aircraft["lon"].as<double>();

      double track =
        aircraft["track"].as<double>();

      double speed =
        aircraft["speed"].as<double>();

      String hexString =
        aircraft["hex"].as<String>();

      String flight =
        aircraft["flight"].as<String>();

      double distance_m =
        distanceMeters(
          local_lat,
          local_lon,
          lat,
          lon);

      double distance_km =
        distance_m / 1000.0;

      if (distance_km > maxDist) {
        maxDist = distance_km;
      }

      if (distance_km < minDist) {
        minDist = distance_km;
        closestAircraft = aircraft;
      }

      // Serial.print("Aircraft ");
      // Serial.print(hexString);

      // Serial.print("| Flight ");
      // Serial.print(flight);

      // Serial.print("|  Lat: ");
      // Serial.print(lat, 6);

      // Serial.print(" | Lon: ");
      // Serial.print(lon, 6);

      // Serial.print(" | Distance: ");
      // Serial.print(distance_km, 1);

      // Serial.print(" | Track: ");
      // Serial.print(track, 0);

      // Serial.print(" | Speed: ");
      // Serial.print(speed, 1);

      // Serial.println(" kt");
    }
  }

  NumberOfAircraftSeen = uniqueCount;

  Serial.print("Number of unique aircraft: ");
  Serial.println(NumberOfAircraftSeen);

  return true;
}

void printPicture(String _url_picture) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot load picture: WiFi not connected.");
    return;
  }

  if (_url_picture.length() == 0) {
    Serial.println("Cannot load picture: URL is empty.");
    return;
  }

  HTTPClient http;
  int httpCode = -1;

  /*
    HTTPS requires WiFiClientSecure.
    setInsecure() disables certificate validation.
  */
  WiFiClientSecure secureClient;
  WiFiClient normalClient;

  if (_url_picture.startsWith("https://")) {
    secureClient.setInsecure();

    if (!http.begin(secureClient, _url_picture)) {
      Serial.println("Unable to start HTTPS connection.");
      return;
    }
  } else {
    if (!http.begin(normalClient, _url_picture)) {
      Serial.println("Unable to start HTTP connection.");
      return;
    }
  }

  http.setTimeout(15000);
  http.setFollowRedirects(
    HTTPC_STRICT_FOLLOW_REDIRECTS);

  Serial.print("Loading picture: ");
  Serial.println(_url_picture);

  httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Picture HTTP error: ");
    Serial.println(httpCode);

    if (httpCode < 0) {
      Serial.println(
        HTTPClient::errorToString(httpCode));
    }

    http.end();
    return;
  }

  String contentType = http.header("Content-Type");

  Serial.print("Content type: ");
  Serial.println(contentType);

  /*
    Download the complete binary JPEG into RAM.

    Arduino String can contain binary data because length()
    is stored separately. Always pass length(), not strlen().
  */
  String jpgData = http.getString();

  http.end();

  if (jpgData.length() == 0) {
    Serial.println("Downloaded picture is empty.");
    return;
  }

  Serial.print("JPEG downloaded: ");
  Serial.print(jpgData.length());
  Serial.println(" bytes");

  const uint8_t* jpgBuffer =
    reinterpret_cast<const uint8_t*>(
      jpgData.c_str());

    // Clear previous aircraft text
  tft.fillRect(
     0,
     40,
     320,
     120,
     TFT_BLACK);
  
  bool result = tft.drawJpg(
    jpgBuffer,
    jpgData.length(),
    80, //150 for right bound
    36,
    320,
    120);

  if (result) {
    Serial.println("JPEG displayed successfully.");
  } else {
    Serial.println(
      "JPEG decoding or drawing failed.");
  }
}

void printClosestAircraft() {
    if (closestAircraft.isNull()) {
    Serial.println("No aircraft with valid position found.");

    tft.drawString(
      "No position data.   ",
      10,
      10);
    tft.drawLine(
      5,
      30,
      315,
      30,
      TFT_WHITE);
    return;
  }

  String hexString =
    closestAircraft["hex"].as<String>();

  Serial.println(
    "Closest Aircraft hex     : " + hexString);

  String flightString =
    closestAircraft["flight"].as<String>();

  flightString.trim();

  /*
    The lookup result uses a different variable name.
    The global closestAircraft remains the live feeder aircraft.
  */
  JsonVariant aircraftDetails =
    lookupAircraftByHex(hexString);

  JsonVariant closestRoute;

  if (
    flightString.length() > 0 &&
    flightString != "-" &&
    flightString != "null"
  ) {
    closestRoute = lookupRouteByCallsign(flightString);
  } else {
    routeLookupDoc.clear();
    Serial.println("No callsign transmitted by aircraft.");
  }
  double track =
    closestAircraft["track"] | 0.0;

  double speed =
    closestAircraft["speed"] | 0.0;

  double altitude =
    closestAircraft["altitude"] | 0.0;

  double longitude =
    closestAircraft["lon"] | 0.0;

  double latitude =
    closestAircraft["lat"] | 0.0;

  String speedString =
    String(speed, 0) + "kts";

  String altitudeString =
    String(altitude, 0) + "ft";

  String distanceString =
    "Distance: " + String(
      distanceMeters(
        latitude,
        longitude,
        local_lat,
        local_lon)
        / 1000.0,
      1)
    + "km";

  Serial.print("Aircraft details         : ");
  serializeJson(aircraftDetails, Serial);
  Serial.println();

  Serial.print("Route details            : ");
  serializeJson(closestRoute, Serial);
  Serial.println();

  String manufacturer = "";
  String aircraftType = "";
  String registration = "";
  String url_photo = "";

  if (!aircraftDetails.isNull()) {
    manufacturer =
      aircraftDetails["manufacturer"] | "";

    aircraftType =
      aircraftDetails["type"] | "";

    registration =
      aircraftDetails["registration"] | "";

    url_photo =
      aircraftDetails["url_photo_thumbnail"] | "";
  }

  Serial.println("Picture        : " + url_photo);
  String aircraftLine =
    manufacturer + " " + aircraftType;

tft.fillRect(
    0,
    35,
    320,
    240,
    TFT_BLACK);

  tft.drawString(
    distanceString + "    ",
    10,
    40);

  if (!closestRoute["flightroute"]["origin"]["icao_code"].isNull()) {
    tft.drawString(
      "from: " + closestRoute["flightroute"]["origin"]["icao_code"].as<String>(),
      10,
      65);
  }

  if (!closestRoute["flightroute"]["origin"]["name"].isNull()) {
    tft.drawString(
      closestRoute["flightroute"]["origin"]["name"].as<String>() + "           ",
      10,
      90);
  }

  if (!closestRoute["flightroute"]["destination"]["icao_code"].isNull()) {
    tft.drawString(
      "to:   " + closestRoute["flightroute"]["destination"]["icao_code"].as<String>(),
      10,
      115);
  }

  if (!closestRoute["flightroute"]["destination"]["name"].isNull()) {
    tft.drawString(
      closestRoute["flightroute"]["destination"]["name"].as<String>() + "             ",
      10,
      140);
  }

  tft.drawString(
    aircraftLine + "                          ",
    10,
    190);

  String trackText = String(track, 0);
  tft.drawString(String(track, 0) + "     ", 10, 165);
  int width = tft.textWidth(trackText);
  tft.drawCircle(
    10 + width + 5,
    165,
    3,
    TFT_WHITE);

  if (!closestRoute["flightroute"]["airline"]["name"].isNull()) {
    String airline = closestRoute["flightroute"]["airline"]["name"].as<String>();

   tft.drawString(
     (registration.isEmpty() ? "" : registration + " ") +
     airline +
     "             ",
     10,
     215
   );
  }

  tft.drawString(
    altitudeString + " " + speedString + "      ",
    70,
    165);

  delay(5000);
  printPicture(url_photo);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(
    TFT_WHITE,
    TFT_BLACK);

  tft.invertDisplay(false);
  tft.setRotation(1);
  delay(1000);
  connectToWiFi();
  delay(1000);
}

void loop() {
  bool ok = readAircraftJson();
  tft.setTextSize(2);

  if (ok) {
    tft.drawString(
      "Aircraft:" + String(NumberOfAircraftSeen) + " (" + String(minDist, 0) + "-" + String(maxDist, 0) + " km)",
      10,
      10);

    tft.drawLine(
      5,
      30,
      315,
      30,
      TFT_WHITE);

    drawWiFiIcon(
      290,
      3,
      true);
  } else {
    tft.drawString(
      "Feeder not found.",
      10,
      10);

    tft.drawLine(
      5,
      30,
      315,
      30,
      TFT_WHITE);

    connectToWiFi();
  }

  printClosestAircraft();

  tft.drawLine(
    5,
    238,
    320,
    238,
    TFT_BLACK);

  for (int i = 0; i <= seconds; i++) {
    tft.drawLine(
      5,
      238,
      10 + i * (300 / seconds),
      238,
      TFT_WHITE);

    delay(1000);
  }
}