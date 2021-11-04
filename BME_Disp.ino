#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

/* Search and Change Values
SSID
Passw0rd
SCREEN_ADDRESS ->if needed
Wemos_Name
SSL_HEX
SrvIP
/arduino/ ->if needed
MQTT_IP
MQTT_User
MQTT_PW
OTA_PW
*/

//Network
#ifndef STASSID
#define STASSID "SSID"
#define STAPSK  "Passw0rd"
#endif

//Sensor
#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme; // I2C
//Adafruit_BME280 bme(BME_CS); // hardware SPI
//Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI

//Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin -1 if sharing Arduino reset pin
#define SCREEN_ADDRESS 0x3C // See datasheet for Address 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Local
String loc = "Wemos_Name"; //Name for MQTT and Firmware (Firmware = Name+devID)
const char* VERSION = "1.0.0";
const char* MODEL = "2";

String getDevID()
{
  unsigned long chipID = ESP.getChipId();
  String devID = String(chipID, HEX);
  return devID;
}

//WiFiClient client;                          // Use WiFiClient class to create TCP connections
BearSSL::WiFiClientSecure sslclient;
const int httpPort = 443;
const char* SSLcert = "SSL_HEX";

const char* host = "SrvIP"; //IP of Server with firmware
const char* fwURLLoc = "/arduino/"; //path to firmware

const char* ssid = STASSID;
const char* password = STAPSK;
String WiFiName = loc + "_WemosSensor"; //Hostname

const char* mqtt_server = "MQTT_IP"; //IP of MQTT Server
const char* mqttUser = "MQTT_User"; //MQTT User
const char* mqttPassword = "MQTT_PW"; //MQTT Password
String mqttName = loc + "_Wemos_Sensor"; //MQTT Name
String mqtt_temp = loc + "/Wemos_temp"; //Topic of temp
String mqtt_humid = loc + "/Wemos_humid"; //Topic of humidity
String mqtt_diag = loc + "/Wemos_diag"; //Topic of diagnostic
const char* otaPassword = "OTA_PW"; //OTA Password

long lastMsg = 0;
long LASTMSG = 0;
long timeto = 60;
long starter = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  Serial.println();
  if (String(topic) == mqtt_temp) {
    Serial.print(message);
  }
  if (String(topic) == mqtt_humid) {
    Serial.print(message);
  }
  Serial.println();
}

WiFiClient espClient;
PubSubClient client(espClient);
//long lastMsg = 0;
float temp = 0;
float humid = 0;

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  Serial.println("Booting");
  display.println("Booting");
  display.display();
  String devID = getDevID();
  Serial.print( "chipID: " );
  Serial.println( devID );
  Serial.print( "Location: " );
  Serial.println( loc );
  display.println(loc + devID);
  display.println(VERSION);
  display.display();
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WiFiName.c_str());
  WiFi.begin(ssid, password);
  display.println("Connecting WLAN");
  display.display();
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Current Version Number: ");
  Serial.println(VERSION);

  //OTA
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(WiFiName.c_str());
  // No authentication by default
  ArduinoOTA.setPassword(otaPassword);
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883);
  if (!client.connected()) {
    Serial.println("MQTT not connected-reconnect");
    Serial.println();
    reconnect();
  }
  checkForUpdates();

  bool status;

  status = bme.begin(0x76);
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    display.clearDisplay();
    display.println("NO SENSOR");
    display.display();
    client.publish(String(mqtt_diag).c_str(), String("Could not find a valid BME280 sensor, check wiring!").c_str(), true);
    while (1);
  }

  Serial.println();

}

void checkForUpdates()
{
  String devID = getDevID();
  String fwURL = String ("https://") + host + fwURLLoc + loc + devID;
  String fwVersionURL = fwURL;
  fwVersionURL.concat( ".ver" );

  Serial.println( "Checking for firmware updates." );
  Serial.print( "chipID: " );
  Serial.println( devID );
  Serial.print( "Firmware version URL:      " );
  Serial.println( fwVersionURL );
  client.publish(String(mqtt_diag).c_str(), String("check for Update").c_str(), true);
  client.publish(String(mqtt_diag).c_str(), String(devID).c_str(), true);
  String FWVersion = VERSION;
  client.publish(String(mqtt_diag).c_str(), String("FW Version: " + FWVersion).c_str(), true);

  HTTPClient httpsClient;
  sslclient.setFingerprint(SSLcert);
  httpsClient.begin(sslclient, fwVersionURL);

  int httpCode = httpsClient.GET();
  Serial.print("httpCode ");
  Serial.println(httpCode);
  if ( httpCode == 200 )
  {
    Serial.print( "Current Model Number: ");
    Serial.println( MODEL );
    Serial.print( "Current firmware version: " );
    Serial.println( VERSION );

    String verFileContents = httpsClient.getString();
    Serial.print( "Available model/firmware version from File: " );
    Serial.println( verFileContents);
    //}
    int lengthData = verFileContents.length();
    lengthData = lengthData - 1;
    char testIt = verFileContents.charAt(lengthData - 2);

    if (testIt == 13)
    {
      lengthData = lengthData - 1;
      Serial.println("testIt-true");
    }
    Serial.print( "String Length: " );
    Serial.println(lengthData);

    int comma = verFileContents.indexOf(",");
    String newModel = verFileContents.substring(0, comma);
    String newVersion = verFileContents.substring(comma + 1, lengthData);

    Serial.print( "New Model Number: ");
    Serial.println( newModel );

    Serial.print( "New Version: ");
    Serial.println( newVersion );

    if (  newVersion != VERSION )
    {
      Serial.println( newVersion + " is newer than " + VERSION);
      Serial.println( "Preparing to update" );
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 10);
      display.println("Updating...");
      display.display();
      client.publish(String(mqtt_diag).c_str(), String(newVersion + " is newer than " + VERSION).c_str(), true);
      // Constuct URL for new firmware
      String fwImageURL = fwURL;
      fwImageURL.concat( ".bin" );
      Serial.print( "Firmware Image File URL: ");
      Serial.println(fwImageURL);

      // Update the firmware
      t_httpUpdate_return ret = ESPhttpUpdate.update(sslclient, fwImageURL);

      // Error handling
      switch (ret)
      {
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          Serial.println(" ");
          client.publish(String(mqtt_diag).c_str(), String("HTTP_UPDATE_FAILED Error ").c_str(), true);
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("HTTP_UPDATE_NO_UPDATES");
          break;
      } // end of switch
    } // end of: if ( ! newVersion.equals( VERSION ))

    else    // newVersion.equals ( VERSION )
    {
      Serial.println( "Already on latest version" );
      client.publish(String(mqtt_diag).c_str(), String("Already on latest version").c_str(), true);
    }
  } //end of:  if ( httpCode == 200 )
  else  // httpCode !== 200
  {
    Serial.print( "Firmware version check failed, HTTP response code: " );
    client.publish(String(mqtt_diag).c_str(), String("check failed, HTTP response code: " + httpCode).c_str(), true);
    Serial.println( httpCode );
  }
  httpsClient.end();

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    display.clearDisplay();
    display.setCursor(0, 10);
    display.print("Connecting to MQTT");
    display.display();
    // Attempt to connect
    if (client.connect(String(mqttName).c_str(), mqttUser, mqttPassword)) {
      Serial.println("connected");
      client.publish(String(mqtt_diag).c_str(), String("MQTT connected").c_str(), true);
      display.clearDisplay();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {

  if (!client.connected()) {
    Serial.println("MQTT not connected-reconnect");
    Serial.println();
    reconnect();
  }
  client.loop();

  ArduinoOTA.handle();

  long now = millis();
  if (now - lastMsg > 60000) {
    lastMsg = now;

    temp = bme.readTemperature();
    humid = bme.readHumidity();

    client.publish(String(mqtt_temp).c_str(), String(temp).c_str(), true);
    client.publish(String(mqtt_humid).c_str(), String(humid).c_str(), true);
    printValues();
    checkForUpdates();
  }
  if ( starter == 0) {
    starter = 1;
    lastMsg = now;
    temp = bme.readTemperature();
    humid = bme.readHumidity();
    client.publish(String(mqtt_temp).c_str(), String(temp).c_str(), true);
    client.publish(String(mqtt_humid).c_str(), String(humid).c_str(), true);
    printValues();
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.print(temp);
  display.println("*C");
  display.print(humid);
  display.println("%");
  display.setTextSize(1);
  display.println();
  if (now - LASTMSG > 1000) {
    LASTMSG = now;
    timeto = ((((now - lastMsg) / 1000) - 60) * (-1));
  }
  display.println(timeto);
  display.display();

}

void printValues() {
  Serial.print("Temperature = ");
  Serial.print(temp);
  Serial.println(" *C");

  // Convert temperature to Fahrenheit
  /*Serial.print("Temperature = ");
    Serial.print(1.8 * bme.readTemperature() + 32);
    Serial.println(" *F");*/

  Serial.print("Pressure = ");
  Serial.print(bme.readPressure() / 100.0F);
  Serial.println(" hPa");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.print("Humidity = ");
  Serial.print(humid);
  Serial.println(" %");

  Serial.println();
}
