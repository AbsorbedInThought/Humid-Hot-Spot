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
#define deviceID 999  /***ENTER DEVICE ID HERE***/
#define readingInterval 45000 /***In milliseconds. Time between successsive readings ***/
#define noOfNetworks 11 /***One for Setup Mode & 10 for Wi-Fi connection ***/
#define noOfObjects 34 /*** Adjust number of JSON objects to send per request. Keep stack overflow in mind. ***/
String URLforAPI = "http://yourApiURLhere/log.php"; /***ENTER API TO SEND DATA TO***/
String URLforCredentials = "http://yourURLhere.com/credentials.php";   /***CHANGE URL ACCORDINGLY TO YOUR NEEDS***/
String URLforAcknowledge = "http://yourURLhere.com/ack.php/deviceID=999";   /***CHANGE DEVICE ID ACCORDINGLY***/
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
String ssids[noOfNetworks];
String passwords[noOfNetworks];

//Returns 0 for setup mode.
//Returns -1 if no connection is found.
//Returns a positive value if connection is found.
int connectToWiFi()
{
  int numberOfNetworks = WiFi.scanNetworks();

//Configure Your Desired Setup Network Here
  ssids[0] = "ESP8266Setup";  
  passwords[0] = "setup@esp";

// Reading Wi-Fi Credentials From File
  f = SPIFFS.open("/credentials.txt", "r"); 
  for (int i = 1; i < noOfNetworks; ++i)
  {
    String ssid = f.readStringUntil('\r');
    f.readStringUntil('\n');
    String pass = f.readStringUntil('\r');
    f.readStringUntil('\n');
    ssids[i] = ssid;
    passwords[i] = pass;
  }

//Setup Mode Check! Needs to be checked individually to avoid connecting to other networks
  for (int i = 0; i < numberOfNetworks; ++i) 
    if (WiFi.SSID(i) == ssids[0])
    {
      WiFi.begin(ssids[0], passwords[0]);
      return 0;
    }

//Searching Available Networks To Connect To
  for (int i = 0; i < numberOfNetworks; ++i)
    for (int j = 1; j < noOfNetworks ; ++j) // 0 index has Setup credentials
      if (WiFi.SSID(i) == ssids[j])
      {
        WiFi.begin(ssids[j], passwords[j]);
        wifiLink = true;                     //Connection Found Flag
        return j;
      }
  return -1;
}

/*** Fetches Updated URL from URLforUpdation for API to which data is sent (Called in Setup Mode) ***/
void getUpdatedURL()
{
  HTTPClient http;

  http.begin();
  int httpCode = http.GET(URLforUpdation); //URL that is used to fetch the API URL
  String payload = http.getString(); //Get URL in payload
  http.end();

  if (httpCode == 200 && payload != "")
  {
    //If URL is successfully fetched, Write it to url.txt file
    f = SPIFFS.open("/url.txt", "w");
    f.println(payload);
    f.close();
  }
}

/*** Fetches noOfNetworks amount of SSID's & passwords from provided URL ***/
/*** Writes to file with SSIDS and passwords seperated by '\n' ***/
void getUpdatedCredentials()
{
  while (WiFi.status() != WL_CONNECTED )
    delay(50);

  HTTPClient http;

  http.begin(URLforCredentials);
  int httpCode = http.GET();
  String payload = http.getString(); //Payload contains noOfNetworks amount of SSID's and passwords, each seperated by '~'
  http.end();

  if (httpCode == 200 && payload != "" )
  {
    f = SPIFFS.open("/credentials.txt", "w");

    //String Parsing
    int j = 0;  
    while (payload[j] != '\0')
    {
      String id = "" , pass = "";

      for (; payload[j] != '~'; ++j) //Parsing SSID
        id += payload[j];

      f.println(id); //Writing SSID to file
      j++;

      for ( ; payload[j] != '~' ; ++j) //Parsing Password
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

      f.println(pass); //Writing Password to file
      j++;
    }
    /*** comment the below 3 lines if acknowldgement is not required ***/
    http.begin(URLforAcknowledge); //Acknowledge successful updation of credentials
    http.GET();
    http.end();
  }
}

/*** Takes a string containing an array of JSON objects as argument
and uploads it to the provided API URL ***/
void uploadData(String obj)
{
  int timer = millis();
  while (WiFi.status() != WL_CONNECTED && millis() < timer + 8000) //Wait for Wi-Fi (8 secs max)
    delay(50);

  HTTPClient http;    //Declare object of class HTTPClient

  http.begin(URLforAPI);      //Specify request destination
  http.addHeader("Content-Type", "application/JSON");  //Specify content-type header (JSON)

  int httpCode = http.POST(obj);   //Send the request
  success = httpCode;              //Flag used to erase sent data

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
        /*** Reading Data from log.txt 
        and creating a packet that contains JSON objects with a size of noOfObjects ***/
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();
        JsonArray& readings = root.createNestedArray("readings");

        for (int packetSize = 0; packetSize < noOfObjects; ++packetSize)
        {
          //Remove the values which are not needed.
          JsonObject& reading = readings.createNestedObject();
          reading["deviceId"] = deviceID;
          reading["lat"] = f.readStringUntil('\r');
          f.readStringUntil('\n');
          reading["long"] = f.readStringUntil('\r');
          f.readStringUntil('\n');
          reading["temperature"] = f.readStringUntil('\r'); //DS-18
          f.readStringUntil('\n');
          reading["temperature2"] = f.readStringUntil('\r'); //DHT-22 
          f.readStringUntil('\n');
          reading["humidity"] = f.readStringUntil('\r');
          f.readStringUntil('\n');
          reading["deviceTime"] = f.readStringUntil('\r');
          f.readStringUntil('\n');
          reading["deviceVersion"] = "1";
          reading["appVersion"] = "1";

          if (!f.available()) //End of file! All data processed.
            break;
        }

        String JSONmessageBuffer;
        /*** //uncomment the following lines to print JSON string output to Serial monitor 
        root.prettyPrintTo(JSONmessageBuffer);
        Serial.println(JSONmessageBuffer);    
        ***/
        
        root.printTo(JSONmessageBuffer); //Writing JSON Objects To String
        uploadData(JSONmessageBuffer); //Upload Data to API
        
        //Checks if previous data was sucessfully uploaded, only then it continues.
        if (success != 200)
          goto networkError; //No Connection. Stop further uploading.
      }
    }
networkError:
    f.close();
  }
}

/*** Writes data of current reading to log.txt ***/
void writeData()
{
  f = SPIFFS.open("/log.txt", "a"); //Append to log.txt.

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

/*** Tries to get GPS data until timeout 
GPS sometimes takes some time to communicate ***/
void getGPSReadings()
{
  while (ss.available() > 0) //GPS receieved data
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

        //Date & Time parsing
        //Format : "YY-MM-DD HH:MM:SS"
        
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
  timeout++; //Increment Timer
}

void setup()
{
  Serial.begin(115200);
  ss.begin(9600);
  SPIFFS.begin();
  dht.setup(DHTpin, DHTesp::DHT22);
 
  starttime = millis(); //For calculating time between next interval

  int setupMode = connectToWiFi(); //Returns 0 if in Setup Mode
  
  f = SPIFFS.open("/url.txt", "r"); //Reads API URL from url.txt 
  URLforAPI = f.readStringUntil('\r');

  if (setupMode == 0) //Device going into Setup Mode
  {
    getUpdatedCredentials();
    getUpdatedURL();
    goto endSetup; // Finish Setup Mode
  }

  //DS-18 (Comment if not required and change writeData() and readData() accordingly)
  sensors.requestTemperatures(); 
  temperature = sensors.getTempCByIndex(0);

  //DHT22 (Comment if not required and change writeData and readData accordingly)
  temperature2 = dht.getTemperature(); //DHT22
  humidity = dht.getHumidity(); //DHT22

  while (timeout != 20 && latitude == 0.0) //Tries to get GPS data 20 times
    getGPSReadings();

  writeData(); //Writes observed readings to file

  if (wifiLink == true) //Wi-Fi connection was found. Upload data.
  {
    readData();
    if (success == 200) //If uploads were successful
      f = SPIFFS.open("/log.txt", "w"); //Erases data inside log.txt
  }

  endtime = millis();  
  // Sleep time = Reading Interval - Processing Time
  endtime = readingInterval - (endtime - starttime); //Subtract endtime from start time to get processing time

  if (endtime < 100) //Avoiding Negative Sleep Times.
    endtime = 1000; 

/* uncomment for light sleep */
  //WiFi.forceSleepBegin(45e6);
  //delay(endtime);

/* Deep Sleep For Some Interval */
  ESP.deepSleep(endtime);
 
//Sleep after setup (Requires restart)
endSetup:
  ESP.deepSleep(600e6);

}

void loop() {}
