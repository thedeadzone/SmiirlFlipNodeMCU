#include <Preferences.h>
#include <DNSServer.h>
#include <Wire.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include <ArduinoJson.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

Preferences preferences;

DNSServer dnsServer;
AsyncWebServer server(80);

String networkName;
String password;
String apiUrl;

String page;
String allnetworks;

int oldCount = -1;
int newCount = -1;
int oldVal, newVal, oldRem, newRem;

bool values_received = false;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smiirl Connect</title>
    <style>
        body {display: flex;flex-direction: column;align-items: center;}
        h1 {font-size: 32px;}
        input,select {margin-bottom:15px;}
        input, select, label {width:250px;display: block;}
        button {padding:10px;}
    </style>
</head>
<body>
    <h1>Smiirl Connect</h1>
    <form action="/get">
        <label for="network">Wi-Fi Network</label>
        <select id="network" name="network">
            <option value="" selected>Select...</option>)rawliteral";

const char footer_html[] PROGMEM = R"rawliteral(
        </select>
        
        <label for="password">Password</label>
        <input type="password" id="password" name="password">
        <label for="apiUrl">API URL</label>
        <input type="text" id="apiUrl" name="apiUrl">
        <button type="submit">Save</button>
    </form>
</body>
</html>)rawliteral";

const int deviceAddresses[] = { 0x10, 0x11, 0x12, 0x13, 0x14 }; // look up your own addresses using an I2C Scanning program

void getSavedPrefs() {
  if (!preferences.begin("smiirl", false)) {
    Serial.println("Failed to initialize preferences");
    return;
  }

  // Manual clear
  // preferences.clear();

  if (preferences.isKey("network")) {
    networkName = preferences.getString("network");
    Serial.print("Network found: ");
    Serial.println(networkName);
  } else {
    Serial.println("Network not found in preferences.");
    networkName = "";
  }

  if (preferences.isKey("password")) {
    password = preferences.getString("password");
    Serial.print("Password found: ");
    Serial.println(password);
  } else {
    Serial.println("Password not found in preferences.");
    password = "";
  }

  if (preferences.isKey("apiUrl")) {
    apiUrl = preferences.getString("apiUrl");
    Serial.print("API Url found: ");
    Serial.println(apiUrl);
  } else {
    Serial.println("API Url not found in preferences.");
    apiUrl = "";
  }

  preferences.end();
}

void attemptConnection() {
  if (networkName == "") {
    Serial.println("No saved network.");
    return;
  }

  int numNetworks = WiFi.scanNetworks();
  bool networkFound = false;
  int networkType = 0;  // 0 for nothing, 1 for open and 2 for WPA2-PSK

  for (int i = 0; i < numNetworks; i++) {
    String ssid = WiFi.SSID(i);
    int encryptionType = WiFi.encryptionType(i);
    if (ssid == networkName) {
      networkFound = true;
      if (encryptionType == AUTH_OPEN) {
        Serial.println("1");
        networkType = 1;
      } else {
        networkType = 2;
        Serial.println("2");
      }
      break;
    }
  }

  if (!networkFound) {
    Serial.println("Saved network not found in available networks.");
    return;
  }

  if (networkType == 1) {
    // Open WiFi
    Serial.print("Connecting to open WiFi: ");
    Serial.println(networkName);
    WiFi.disconnect(true);  // Disconnect if already connected
    WiFi.begin(networkName.c_str());
  } else if (networkType == 2) {
    // WPA2-PSK
    Serial.print("Connecting to WPA2-PSK WiFi: ");
    Serial.println(networkName);
    WiFi.disconnect(true);  // Disconnect if already connected
    WiFi.begin(networkName.c_str(), password.c_str());
  } 

  int retries = 5;

  while (WiFi.status() != WL_CONNECTED && retries-- > 0) {
    delay(2000);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    setBlanks();
  } else {
    Serial.println("\nFailed to connect to WiFi.");
  }
}

String getWiFiNetworks() {
  String options;
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    options += "<option value=\"" + String(WiFi.SSID(i)) + "\">" + WiFi.SSID(i) + "</option>";
  }
  return options;
}

void setServerDetails() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    page = FPSTR(index_html) + allnetworks + FPSTR(footer_html);
    request->send_P(200, "text/html", page.c_str());
    Serial.println("Client Connected");
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("network")) {
      networkName = request->getParam("network")->value();
      Serial.println(networkName);
    }
    if (request->hasParam("password")) {
      password = request->getParam("password")->value();
      Serial.println(password);
    }
    if (request->hasParam("apiUrl")) {
      apiUrl = request->getParam("apiUrl")->value();
      Serial.println(apiUrl);
    }
    if (!preferences.begin("smiirl", false)) {
      Serial.println("Failed to initialize preferences");
    } else {
      preferences.putString("network", networkName);
      preferences.putString("password", password);
      preferences.putString("apiUrl", apiUrl);
    }
    values_received = true;
    request->send(200, "text/html", "The values entered by you have been successfully sent to the device. Closing access point and attempting connection...");
  });
}

void startServer() {
  Serial.println("StartServer");
  WiFi.mode(WIFI_STA);
  allnetworks = getWiFiNetworks();
  delay(1000);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Smiirl Connect");
  setServerDetails();
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.begin();
  waitForDetails(1);
  dnsServer.stop();
  server.end();
  WiFi.mode(WIFI_STA);
}

void waitForDetails(int tries) {
  if (!values_received && tries <= 9) {  //Give user 45 seconds to connect and set new values
    dnsServer.processNextRequest();
    delay(5000);
    waitForDetails(tries + 1);
  }
}

void setLines() {
  Serial.println("Lines");
  for (int i = 4; i >= 0; i--) {
    Wire.beginTransmission(deviceAddresses[i]);
    Wire.write(0x02); // DEFAULT
    Wire.write(0xB); // Value
    Wire.write(0x1); // Wat is deze?
    Wire.write(0x0); // DEFAULT
    Wire.write(0x1); // DEFAULT
    Wire.endTransmission();
    delay(4000);
  }

}

void setBlanks() {
  Serial.println("Blanks");
  for (int i = 0; i < 5; i++) {
    Wire.beginTransmission(deviceAddresses[i]);
    Wire.write(0x02); // DEFAULT
    Wire.write(0xA); // Value
    Wire.write(0x0); // Wat is deze?
    Wire.write(0x0); // DEFAULT
    Wire.write(0x1); // DEFAULT
    Wire.endTransmission();
    delay(3000);
  }
}

void getNewCounts() {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, apiUrl);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println("Response: " + payload);
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
      // {"count": 123}
      newCount = doc["count"];
      Serial.print("Count: ");
      Serial.println(newCount);
    } else {
      Serial.print("JSON Deserialization Error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

void updateCounter() {
  Serial.print("Updating counter: ");
  Serial.print(oldCount);
  Serial.print(" -> ");
  Serial.println(newCount);
  if (oldCount != newCount) {
    oldVal = oldCount;
    newVal = newCount;
    for (int i = 0; i < 5; i++) {
      oldRem = oldVal % 10;
      newRem = newVal % 10;
      if (oldRem != newRem) {
        bool cycle = false;
        if (newRem < oldRem) {
          cycle = true;
        }
        setFlipper(i, newRem, cycle);
        delay(3500);
      }
      oldVal /= 10;
      newVal /= 10;
    }
  }
  oldCount=newCount;
}

void setFlipper(int pos, int val, bool cycle) {
  Wire.beginTransmission(deviceAddresses[pos]);
  Wire.write(0x02); // DEFAULT
  Wire.write(val); // Value
  if (cycle) { // Decides if all numbers should cycle
    Wire.write(0x1);
  } else {
    Wire.write(0x0);
  }
  Wire.write(0x0); // DEFAULT
  Wire.write(0x1); // DEFAULT
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);  // For debugging
  Wire.begin();  // Initialize I2C communication
  delay(1000);
  getSavedPrefs();  // Get saved network details
  setLines();
  if (networkName != "")  // If there is a saved network
    attemptConnection();
}

void getRandomCount() {
  newCount = random(11111, 99999);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // Remove comment to get data from an API
    // getNewCounts();

    // For debugging, random number every 10 seconds
    Serial.println("Getting some random numbers");
    getRandomCount();
    updateCounter();
    delay(10000);
  } else {
    setLines();
    attemptConnection();  // Try again with previous details
    if (WiFi.status() != WL_CONNECTED) {
      startServer();  // Start server and AP to set new creds
      delay(1000);
      attemptConnection();
      if (WiFi.status() != WL_CONNECTED) {  // If still not connected after attempting through portal
        ESP.restart();
      }
    }
  }
}
