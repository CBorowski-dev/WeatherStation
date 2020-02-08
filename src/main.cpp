/*
  Display (ST7735 controller) connection to ESP8266 (D1 mini)

  Display SDO/MISO   to NodeMCU pin D6 = MISO = D6 (or leave disconnected if not reading TFT)
  Display LED        to NodeMCU pin VIN (or 5V, see below)
  Display SCK        to NodeMCU pin D5 = SCLK = D5
  Display SDI/MOSI   to NodeMCU pin D7 = MOSI = D7
  Display DC (RS/AO) to NodeMCU pin D3 = GPIO0 = D3
  Display RESET      to NodeMCU pin D4 = GPIO2 = D4 (or RST, see below)
  Display CS         to NodeMCU pin D8 = CS = D8 (or GND, see below)
  Display GND        to NodeMCU pin GND (0V)
  Display VCC        to NodeMCU 5V or 3.3V

  OpenWeatherMap

  Call: http://api.openweathermap.org/data/2.5/weather?q=<city>,de&units=metric&appid=<own_appid>

  Libraries needed:
  - ESP8266WiFi
  - TFT_eSPI
  - ArduinoJson
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include <ArduinoJson.h>

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

// SSID/Password for WLAN
const char* ssid = "";
const char* password = "";

WiFiClient client;
IPAddress ipAddress;

// Name address for Open Weather Map API
const char* server = "api.openweathermap.org";

// Unique URL resource
const char* resource = "/data/2.5/weather?q=<city>,de&units=metric&appid=<own_appid>";

const unsigned long HTTP_TIMEOUT = 10000;  // max respone time from server

// The type of data that we want to extract from the page
struct clientData {
  float temp;
  int8_t humidity;
  int16_t pressure;
  float windSpeed;
  int16_t windDeg;
};

void initDisplay() {
  tft.init();
  tft.setRotation(1);
}

// Open connection to the HTTP server
bool connect(const char* hostName) {
  Serial.print("Connect to ");
  Serial.println(hostName);

  bool ok = client.connect(hostName, 80);

  Serial.println(ok ? "Connected" : "Connection Failed!");
  return ok;
}

// Send the HTTP GET request to the server
bool sendRequest(const char* host, const char* resource) {
  Serial.print("GET ");
  Serial.println(resource);

  client.print("GET ");
  client.print(resource);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(host);
  client.println("Connection: close");
  client.println();

  Serial.print("Request sent");

  return true;
}

// Skip HTTP headers so that we are at the beginning of the response's body
bool skipResponseHeaders() {
  // HTTP headers end with an empty line
  char endOfHeaders[] = "\r\n\r\n";

  client.setTimeout(HTTP_TIMEOUT);
  bool ok = client.find(endOfHeaders);

  if (!ok) {
    Serial.println("No response or invalid response!");
  }
  Serial.println("Response headers skipped");
  return ok;
}

bool readReponseContent(struct clientData* clientData) {
  // Compute optimal size of the JSON buffer according to what we need to parse.
  // See https://bblanchon.github.io/ArduinoJson/assistant/
  const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 
      2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 
      JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 390;

  DynamicJsonDocument jsonDoc(bufferSize);
  DeserializationError error = deserializeJson(jsonDoc, client);
  if (error) {
    Serial.println("JSON parsing failed!");
    return false;
  }

  // Copy the strings we're interested in using to the struct data
  clientData->temp = jsonDoc["main"]["temp"].as<float>();
  clientData->humidity = jsonDoc["main"]["humidity"].as<int8_t>();
  clientData->pressure = jsonDoc["main"]["pressure"].as<int16_t>();
  clientData->windSpeed = jsonDoc["wind"]["speed"].as<float>();
  clientData->windDeg = jsonDoc["wind"]["deg"].as<int16_t>();

  return true;
}

// Display the data extracted from the JSON
void printClientData(const struct clientData* clientData) {
  // on Display
  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  tft.drawString("Wetter in Paderborn", 15, 0, 2);

  tft.drawString("Temperatur", 0, 30, 2);
  char temp[6 ]; // Buffer big enough for 7-character float
  dtostrf(clientData->temp, 5, 1, temp);
  tft.drawRightString(temp, 135, 30, 2);
  tft.drawString("`C", 140, 30, 2);

  tft.drawString("Luftfeuchtigkeit", 0, 50, 2);
  char humidity[4];
  sprintf(humidity, "%d", clientData->humidity);
  tft.drawRightString(humidity, 135, 50, 2);
  tft.drawString("%", 140, 50, 2);

  tft.drawString("Luftdruck", 0, 70, 2);
  char pressure[4];
  sprintf(pressure, "%d", clientData->pressure);
  tft.drawRightString(pressure, 135, 70, 2);
  tft.drawString("hPa", 140, 70, 2);

  tft.drawString("Windstaerke", 0, 90, 2);
  char windspeed[5]; // Buffer big enough for 7-character float
  dtostrf(clientData->windSpeed, 4, 1, windspeed);
  tft.drawRightString(windspeed, 135, 90,2);
  tft.drawString("Bft", 140, 90, 2);

  tft.drawString("Windrichtung", 0, 110, 2);
  char windDeg[4];
  sprintf(windDeg, "%d", clientData->windDeg);
  tft.drawRightString(windDeg, 135, 110, 2);
  tft.drawString("`", 140, 110, 2);

  // Print the data to the serial port
  Serial.print("Temp = ");
  Serial.println((float) clientData->temp);
  Serial.print("Humidity = ");
  Serial.println((int8_t) clientData->humidity);
  Serial.print("Pressure = ");
  Serial.println((int16_t) clientData->pressure);
  Serial.print("Win speed = ");
  Serial.println((float) clientData->windSpeed);
  Serial.print("Wind degree = ");
  Serial.println((int16_t) clientData->windDeg);
}

// Close the connection with the HTTP server
void disconnect() {
  Serial.println("Disconnect");
  client.stop();
}

// Pause for a 1 minute
void wait() {
  Serial.println("Wait 15 minutes");
  delay(900000);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  ipAddress = WiFi.localIP();
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(ipAddress);
}

// ARDUINO entry point #1: runs once when you press reset or power the board
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;  // wait for serial port to initialize
  }
  Serial.println("Serial ready");
  initDisplay();
  setup_wifi();
  delay(1000);
}

// ARDUINO entry point #2: runs over and over again forever
void loop() {
  if(connect(server)) {
    if(sendRequest(server, resource) && skipResponseHeaders()) {
      clientData clientData;
      if(readReponseContent(&clientData)) {
        printClientData(&clientData);
      }
    }
  }
  disconnect();
  wait();
}
