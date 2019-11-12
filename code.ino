/*** Importing Required Packages ***/
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <DHTesp.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "FS.h"

/****************CUSTOMIZATION OF NETWORK************/
#define deviceID 4  /*ENTER DEVICE ID HERE*/
String URLforAPI = "http://yourApiURLhere/log.php"; /***ENTER API TO SEND DATA TO***/
String URLforCredentials = "http://yourURLhere.com/credentials.php";   /***CHANGE URL ACCORDINGLY TO YOUR NEEDS***/
String URLforAcknowledge = "http://yourURLhere.com/ack.php";   /***CHANGE ACCORDINGLY***/
String URLforUpdation = "http://yourURLhere/getbaseURL.php" /***CHANGE CREDENTIALS URL HERE***/
/****************END************/

#define DHTpin 14 //GPIO-14 or D5
TinyGPSPlus  gps; // Tiny GPS object
DHTesp dht; //For DHT22 sensor
SoftwareSerial  ss(4, 5); //Pins D1 and D2

/***Configuring DS-18 Temperature Sensor***/
const int oneWireBus = 0;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

/***GPS Variables***/
unsigned long starttime, endtime;
unsigned short hour, minute, second, day, month, year;
String time_str = "", date_str = "00-00-0000 00:00:00";
float humidity, temperature, temperature2 = 0, latitude = 0, longitude = 0;

bool wifiLink = 0;
word timeout = 0, success = 0;
File f;
String ssids[11];
String passwords[11];

//Returns 0 for setup mode.
//Returns -1 if no connection is found.
//Returns a positive value if connection is found.
int connectToWiFi()
{
  int numberOfNetworks = WiFi.scanNetworks();

  ssids[0] = "ESP8266Setup";
  passwords[0] = "setup@esp";

// Reading Wi-Fi Credentials From File
  f = SPIFFS.open("/credentials.txt", "r"); 
  for (int i = 1; i < 11; ++i)
  {
    String ssid = f.readStringUntil('\r');
    f.readStringUntil('\n');
    String pass = f.readStringUntil('\r');
    f.readStringUntil('\n');
    ssids[i] = ssid;
    passwords[i] = pass;
  }

//Setup Mode Check
  for (int i = 0; i < numberOfNetworks; ++i) 
    if (WiFi.SSID(i) == ssids[0])
    {
      WiFi.begin(ssids[0], passwords[0]);
      return 0;
    }

//Searching Available Networks To Connect To
  for (int i = 0; i < numberOfNetworks; ++i)
    for (int j = 0; j < 11 ; ++j)
      if (WiFi.SSID(i) == ssids[j])
      {
        WiFi.begin(ssids[j], passwords[j]);
        wifiLink = true;                     //Connection Found Flag
        return j;
      }
  return -1;
}

void getUpdatedURL()
{
  HTTPClient http;

  http.begin();
  int httpCode = http.GET(URLforUpdation);
  String payload = http.getString();
  http.end();

  if (httpCode == 200 && payload != "")
  {
    f = SPIFFS.open("/url.txt", "w");
    f.println(payload);
    f.close();
  }
}

void getUpdatedCredentials()
{
  while (WiFi.status() != WL_CONNECTED )
    delay(50);

  HTTPClient http;

  http.begin(URLforCredentials);
  int httpCode = http.GET();
  String payload = http.getString();
  http.end();

  if (httpCode == 200 && payload != "" )
  {
    f = SPIFFS.open("/credentials.txt", "w");
    int j = 0;  //String Parsing
    while (payload[j] != '\0')
    {
      String id = "" , pass = "";

      for ( ; payload[j] != '~'; ++j) //SSID
        id += payload[j];

      f.println(id);
      j++;

      for ( ; payload[j] != '~' ; ++j) // Password
      {
        if (payload[j] == '\0')
        {
          f.println(pass);
          f.close();

          return;
        }
        else
          pass += payload[j];
      }

      f.println(pass);
      j++;
    }
    http.begin(URLforAcknowledge);
    http.GET();
    http.end();
  }
}

void uploadData(String obj)
{
  int timer = millis();
  while (WiFi.status() != WL_CONNECTED && millis() < timer + 9000) // wait for wifi (9 secs max)
    delay(50);

  HTTPClient http;    //Declare object of class HTTPClient

  http.begin(URLforAPI);      //Specify request destination
  http.addHeader("Content-Type", "application/JSON");  //Specify content-type header

  int httpCode = http.POST(obj);   //Send the request
  success = httpCode;

  http.end();  //Close connection
}

void readData()
{
  f = SPIFFS.open("/log.txt", "r");

  if (!f)
    ;
  else
  {
    while (f.available())
    {
      {
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();
        JsonArray& readings = root.createNestedArray("readings");

        for (int packetSize = 0; packetSize < 34; ++packetSize)
        {
          JsonObject& reading = readings.createNestedObject();
          reading["deviceId"] = deviceID;
          reading["lat"] = f.readStringUntil('\r');
          f.readStringUntil('\n');
          reading["long"] = f.readStringUntil('\r');
          f.readStringUntil('\n');
          reading["temprature"] = f.readStringUntil('\r');
          f.readStringUntil('\n');
          reading["temprature2"] = f.readStringUntil('\r');
          f.readStringUntil('\n');
          reading["humidity"] = f.readStringUntil('\r');
          f.readStringUntil('\n');
          reading["deviceTime"] = f.readStringUntil('\r');
          f.readStringUntil('\n');
          reading["deviceVersion"] = "1";
          reading["appVersion"] = "1";

          if (!f.available())
            break;
        }

        String JSONmessageBuffer;
        root.printTo(JSONmessageBuffer);
        uploadData(JSONmessageBuffer);
        if (success != 200)
          goto networkError;
      }
    }
networkError:
    f.close();
  }
}

void writeData()
{
  f = SPIFFS.open("/log.txt", "a");

  if (!f)
    ;
  else
  {
    f.println(latitude, 6);
    f.println(longitude, 6);
    f.println(temperature);
    f.println(temperature2);
    f.println(humidity);
    f.println(date_str);
    f.close();
  }
}

void getGPSReadings()
{
  while (ss.available() > 0)
    if (gps.encode(ss.read()))
    {
      if (gps.location.isValid())
      {
        latitude = gps.location.lat();
        longitude = gps.location.lng();
      }

      if (gps.date.isValid())
      {
        date_str = "";
        day = gps.date.day();
        month = gps.date.month();
        year = gps.date.year();


        if (year < 10)
          date_str += '0';
        date_str += String(year);

        date_str += "-";

        if (month < 10)
          date_str += '0';
        date_str += String(month);

        date_str += "-";

        if (day < 10)
          date_str += '0';
        date_str += String(day);
      }

      if (gps.time.isValid())
      {
        time_str = "";
        hour = gps.time.hour();
        minute = gps.time.minute();
        second = gps.time.second();

        if (hour < 10)
          time_str = '0';
        time_str += String(hour);

        time_str += ":";

        if (minute < 10)
          time_str += '0';
        time_str += String(minute);

        time_str += ":";

        if (second < 10)
          time_str += '0';
        time_str += String(second);

        date_str += " " + time_str;
      }
    }
  delay(100);
  timeout++;
}

void setup()
{
  Serial.begin(115200);
  ss.begin(9600);
  SPIFFS.begin();
  dht.setup(DHTpin, DHTesp::DHT22);
 
  starttime = millis();

  int setupMode = connectToWiFi();


  f = SPIFFS.open("/url.txt", "r");
  URLforAPI = f.readStringUntil('\r');

  if (setupMode == 0)
  {
    getUpdatedCredentials();
    getUpdatedURL();
    goto endloop;
  }

  //DS-18
  sensors.requestTemperatures(); 
  temperature = sensors.getTempCByIndex(0);

  //DHT22
  temperature2 = dht.getTemperature(); //DHT22
  humidity = dht.getHumidity(); //DHT22

  while (timeout != 20 && latitude == 0.0)
    getGPSReadings();

  writeData();

  if (wifiLink == true)
  {
    readData();
    if (success == 200)
      f = SPIFFS.open("/log.txt", "w");
  }

  endtime = millis();
  endtime = 45000 - (endtime - starttime);

  if (endtime < 100)
    endtime = 1000;

/* uncomment for light sleep */
  //WiFi.forceSleepBegin(45e6);
  //delay(endtime);

/* Deep Sleep For Some Interval */
  ESP.deepSleep(endtime);
 
//Sleep after setup (Requires restart)
endloop:
  ESP.deepSleep(600e6);

}

void loop() {}
