#include <WiFi.h>
//#include <WiFiServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <HttpClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>
#include <Bounce2.h>

// Config WiFi
const char *ssid = "QiQi";
const char *password = "UyenLe2401";

const char *targetIP = "172.20.10.5"; // Địa chỉ IP của ESP32 nhận dữ liệu
const int targetPort = 80;            // Cổng mà ESP32 nhận dữ liệu

// Frame pushlish data to a field in a channel: air_temperature, air_humidity, soil_moisture, light_intensity
#define SECRET_MQTT_USERNAME "IDEXOQwODy4KGzAqCiE8ExI"
#define SECRET_MQTT_CLIENT_ID "IDEXOQwODy4KGzAqCiE8ExI"
#define SECRET_MQTT_PASSWORD "75k6qhVaX8PxOY5ZKZcytz0U"

#define MQTT_PUBLISH_TOPIC_AIR_TEMPERATURE "channels/2174698/publish/fields/field1"
#define MQTT_PUBLISH_TOPIC_AIR_HUMIDITY "channels/2174698/publish/fields/field2"
#define MQTT_PUBLISH_TOPIC_SOIL_MOISTURE "channels/2174698/publish/fields/field3"
#define MQTT_PUBLISH_TOPIC_LIGHT_INTENSITY "channels/2174698/publish/fields/field4"

// Frame pushlish data to a field in a channel: fan, water_pump, light
#define MQTT_PUBLISH_TOPIC_FAN "channels/2174698/publish/fields/field6"
#define MQTT_PUBLISH_TOPIC_WATER_PUMP "channels/2174698/publish/fields/field7"
#define MQTT_PUBLISH_TOPIC_LIGHT "channels/2174698/publish/fields/field8"

// Frame subscribe to a field in a channel: fan, water_pump, light
#define MQTT_SUBSCRIBE_TOPIC_FAN "channels/2174698/subscribe/fields/field6"
#define MQTT_SUBSCRIBE_TOPIC_WATER_PUMP "channels/2174698/subscribe/fields/field7"
#define MQTT_SUBSCRIBE_TOPIC_LIGHT "channels/2174698/subscribe/fields/field8"

// MQTT Broker information
const char *mqttServer = "mqtt3.thingspeak.com";
const int mqttPort = 1883;
const char *mqttUsername = SECRET_MQTT_USERNAME;
const char *mqttPassword = SECRET_MQTT_PASSWORD;
const char *clientId = SECRET_MQTT_CLIENT_ID;
const char *channelId = "2174698";

// MQTT connection status
bool mqttConnected = false;

// Config REST API
AsyncWebServer server(80);
float temp, humi, mois, light;

// Config OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// MQTT client
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Button pin
const int buttonPin_Fan = 35;
const int buttonPin_Pump = 39;
const int buttonPin_Light = 34;

// Button debounce
Bounce debouncer = Bounce();
Bounce debouncer2 = Bounce();
Bounce debouncer3 = Bounce();
enum Button
{
  Fan,
  Pump,
  Light
};

// Button state variables
int fanState = 0;
int pumpState = 0;
int lightState = 0;

void handleButtonPress(Button button);
void connectToMQTTBroker();
void handlePostRequest(AsyncWebServerRequest *request);
void reconnectToMQTTBroker();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void restAPI_send_data_actutor();
void screenDislay();

void setup()
{
  Serial.begin(115200);
  // WiFi init
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  server.on("/postsensor", HTTP_POST, handlePostRequest);
  server.begin();

  Serial.println("Server started");
  Serial.println("Server started");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Initialize OLED SSD1306
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;
  }

  // Initialize MQTT client
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);

  // Connect to MQTT broker
  connectToMQTTBroker();
  // Subscribe to MQTT topics
  mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC_FAN);
  mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC_WATER_PUMP);
  mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC_LIGHT);

  // Setup button pin
  pinMode(buttonPin_Fan, INPUT_PULLUP);
  debouncer.attach(buttonPin_Fan);
  pinMode(buttonPin_Pump, INPUT_PULLUP);
  debouncer2.attach(buttonPin_Pump);
  pinMode(buttonPin_Light, INPUT_PULLUP);
  debouncer3.attach(buttonPin_Light);
}

void loop()
{
  // Handle MQTT events
  if (!mqttConnected)
  {
    reconnectToMQTTBroker();
  }
  mqttClient.loop();

  debouncer.update();
  debouncer2.update();
  debouncer3.update();

  if (debouncer.fell())
  {
    handleButtonPress(Button::Fan);
  }

  if (debouncer2.fell())
  {
    handleButtonPress(Button::Pump);
  }

  if (debouncer3.fell())
  {
    handleButtonPress(Button::Light);
  }
}

void handleButtonPress(Button button)
{
  switch (button)
  {
  case Button::Fan:
    Serial.println("Button Fan pressed"); // Clear display
    fanState = (fanState == 0) ? 1 : 0;
    restAPI_send_data_actutor();
    mqtt_publish_pro(MQTT_PUBLISH_TOPIC_FAN, String(fanState).c_str());
    // delay(500);
    break;

  case Button::Pump:
    Serial.println("Button Pump pressed");
    pumpState = (pumpState == 0) ? 1 : 0;
    restAPI_send_data_actutor();
    mqtt_publish_pro(MQTT_PUBLISH_TOPIC_WATER_PUMP, String(pumpState).c_str());
    // delay(500);
    break;

  case Button::Light:
    Serial.println("Button Light pressed");
    lightState = (lightState == 0) ? 1 : 0;
    restAPI_send_data_actutor();
    mqtt_publish_pro(MQTT_PUBLISH_TOPIC_LIGHT, String(lightState).c_str());
    // delay(500);
    break;

  default:
    break;
  }
}

void handlePostRequest(AsyncWebServerRequest *request)
{
  if (request->method() == HTTP_POST)
  {
    // Đọc dữ liệu từ yêu cầu POST
    String jsonData = request;

    DynamicJsonDocument json(1024);
    int jsonStart = jsonData.indexOf("{");
    DeserializationError error = deserializeJson(json, jsonData.substring(jsonStart));

    if (error)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
    else
    { 
      JsonObject jsonObject = jsonBuffer.as<JsonObject>();
      // Get values from JSON keys and save to variables
      temp = json["temp"].as<String>().toFloat();
      humi = json["humi"].as<String>().toFloat();
      mois = json["mois"].as<String>().toFloat();
      light = json["light"].as<String>().toFloat();

      // Print received values to the serial monitor
      Serial.print("Temp: ");
      Serial.println(temp);
      Serial.print("Humi: ");
      Serial.println(humi);
      Serial.print("Mois: ");
      Serial.println(mois);
      Serial.print("Light: ");
      Serial.println(light);

      char airTemp[10]; // Mảng char để lưu giá trị đã ép kiểu
      char airHumi[10];
      char soilMois[10];
      char lightInt[10];
      sprintf(airTemp, "%.2f", temp); // Ép kiểu và lưu vào mảng char
      sprintf(airHumi, "%.2f", humi);
      sprintf(soilMois, "%.2f", mois);
      sprintf(lightInt, "%.2f", light);

      // Publish values to MQTT topics
      mqtt_publish_pro(MQTT_PUBLISH_TOPIC_AIR_TEMPERATURE, airTemp);
      delay(2000);
      mqtt_publish_pro(MQTT_PUBLISH_TOPIC_AIR_HUMIDITY, airHumi);
      delay(2000);
      mqtt_publish_pro(MQTT_PUBLISH_TOPIC_SOIL_MOISTURE, soilMois);
      delay(2000);
      mqtt_publish_pro(MQTT_PUBLISH_TOPIC_LIGHT_INTENSITY, lightInt);
      delay(2000);

      // Gửi phản hồi HTTP cho client
      request->send(200, "text/plain", "POST request handled!");
    }
  }
}

void connectToMQTTBroker()
{
  while (!mqttClient.connected())
  {
    Serial.println("Connecting to MQTT broker...");
    if (mqttClient.connect(clientId, mqttUsername, mqttPassword))
    {
      Serial.println("Connected to MQTT broker");
      mqttConnected = true;
    }
    else
    {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void reconnectToMQTTBroker()
{
  if (!mqttClient.connected())
  {
    connectToMQTTBroker();
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  // Convert the payload to a string
  String message;
  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }
  // Handle different topics
  if (strcmp(topic, MQTT_SUBSCRIBE_TOPIC_FAN) == 0)
  {
    // Handle fan topic
    fanState = message.toInt();
    Serial.print("Received fan state: ");
    Serial.println(fanState);
  }
  else if (strcmp(topic, MQTT_SUBSCRIBE_TOPIC_WATER_PUMP) == 0)
  {
    // Handle water pump topic
    pumpState = message.toInt();
    Serial.print("Received water pump state: ");
    Serial.println(pumpState);
  }
  else if (strcmp(topic, MQTT_SUBSCRIBE_TOPIC_LIGHT) == 0)
  {
    // Handle light topic
    lightState = message.toInt();
    Serial.print("Received light state: ");
    Serial.println(lightState);
  }
  
  restAPI_send_data_actutor();
}

void restAPI_send_data_actutor()
{
  // Create a JSON object
  DynamicJsonDocument json(256);
  json["fan"] = String(fanState);
  json["pump"] = String(pumpState);
  json["light"] = String(lightState);

  // Serialize the JSON object to a string
  String jsonString;
  serializeJson(json, jsonString);

  // Send the JSON data as a POST request to the target ESP32
  WiFiClient http;
  if (http.connect(targetIP, targetPort))
  {
    Serial.println("Connected to Rest API server");
    // Make a POST request
    http.println("POST /postactutor HTTP/1.1");
    http.println("Host: 172.20.10.5");
    http.println("Content-Type: application/json");
    http.println("Connection: close");
    http.print("Content-Length: ");
    http.println(jsonString.length());
    http.println();
    http.println(jsonString);

    // Wait for the server to respond
    while (http.connected())
    {
      // Serial.println("Connected to Rest API");
      if (http.available())
      {
        String line = http.readStringUntil('\r');
        Serial.println(line);
      }
    }

    // Disconnect from the server
    http.stop();
    Serial.println("Disconnected from Rest API server");
  }
  else
  {
    Serial.println("Failed to connect to Rest API server");
  }
  delay(500); // Delay 60 seconds before sending the next request
}
void mqtt_publish_pro(const char *topic, const char *data)
{
  if (mqttClient.connected())
  {
    mqttClient.publish(topic, data);
    Serial.println("Data publish to broker successfull");
  }
  else
  {
    connectToMQTTBroker();
    delay(500);
    return mqtt_publish_pro(topic, data);
  }
}

void screenDislay() {
  // Clear display
  display.clearDisplay();
  // Display values on OLED
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(" ");
  display.print("Temp: ");
  display.println(temp);
  display.print("Humi: ");
  display.println(humi);
  display.print("mois: ");
  display.println(mois);
  display.print("light: ");
  display.println(light);
  display.print("Fan state: ");
  display.println((fanState == 1) ? "ON" : "OFF");
  display.print("Pump state: ");
  display.println((pumpState == 1) ? "ON" : "OFF");
  display.print("Light state: ");
  display.println((lightState == 1) ? "ON" : "OFF");
  display.display();
}
