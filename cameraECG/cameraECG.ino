///////////////////////////////////////////////////////////////////////////////// includes
#include <WiFi.h>
#include "ESP32_OV5640_AF.h"
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_sleep.h"
#include <HTTPClient.h>
#include <Arduino.h>
#include <Arduino_JSON.h>
#include <WebServer.h>
#include <WiFiClient.h>

#define CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#define camTrig 13

OV5640 ov5640 = OV5640();

WebServer server(80);

////////////////////////////////////////////////////////////////////////////////////////////////// camera config

void configCam()
{

  delay(50);
  digitalWrite(12, HIGH);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 3;  //0-63 lower number means higher quality
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 3;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(100);
    ESP.restart();
  }

  sensor_t* sensor = esp_camera_sensor_get();
  ov5640.start(sensor);

  if (ov5640.focusInit() == 0) {
    Serial.println("OV5640_Focus_Init Successful!");
  }

  if (ov5640.autoFocusMode() == 0) {
    Serial.println("OV5640_Auto_Focus Successful!");
  }

  digitalWrite(12, LOW);
}

/////////////////////////////////////////////////////////////////////////////////////////////////// image uploading and patient details
int id = 0;
String serverName = "sensorlifeline.com";
String apiKeyValue = "tPmAT5Ab3j7F9";
String serverPath = "/uploads.php";
const char* getServerName = "http://sensorlifeline.com/get-esp.php";
String getPatientDetails = "";

const int serverPort = 80;

//const int flash = 1;
//const int flashInt = 250; // 0 - 2 ^ fleshRes; (0-255)
//const int flashRes = 8;
//const int flashFreq = 500;

long tim = 0;

WiFiClient client;

String sendPhoto(camera_fb_t * fb) {


  String getAll;
  String getBody;

  Serial.println("Connecting to server: " + serverName);

  digitalWrite(12, HIGH);
  if (client.connect(serverName.c_str(), serverPort)) {
    Serial.println("Connection successful!");
    //    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"" + String(id) + "_" + String(count[id]) + ".jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"" + String(id) + ".jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    //Serial.println(head);
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverName);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    client.println();
    client.print(head);

    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n = n + 1024) {
      if (n + 1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen % 1024 > 0) {
        size_t remainder = fbLen % 1024;
        client.write(fbBuf, remainder);
      }
    }
    client.print(tail);

    //count[id]++;

    int timoutTimer = 10000;
    long startTimer = millis();
    boolean state = false;

    while ((startTimer + timoutTimer) > millis()) {
      Serial.print(".");
      delay(100);
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (getAll.length() == 0) {
            state = true;
          }
          getAll = "";
        }
        else if (c != '\r') {
          getAll += String(c);
        }
        if (state == true) {
          getBody += String(c);
        }
        startTimer = millis();
      }
      if (getBody.length() > 0) {
        break;
      }
    }
    Serial.println();
    client.stop();
    Serial.println(getBody);
  }
  else {
    getBody = "Connection to " + serverName +  " failed.";
    Serial.println(getBody);
  }

  sendStreamLink(5, "");
  return getBody;

}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(client, serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void getPatientDetail()
{
  if (WiFi.status() == WL_CONNECTED) {

    getPatientDetails = httpGETRequest(getServerName);
    while (getPatientDetails.indexOf("login and try again")) {
      getPatientDetails = httpGETRequest(getServerName);

      if (getPatientDetails)
      {
        //Serial.println(getPatientDetails);
        break;
      }
      delay(200);
    }

    // Serial.println(getPatientDetails);

    JSONVar myObject = JSON.parse(getPatientDetails);

    if (JSON.typeof(myObject) == "undefined") {
      //Serial.println("Parsing input failed!");
      return;
    }

    // Serial.print("JSON object = ");
    //Serial.println(myObject);

    // myObject.keys() can be used to get an array of all the keys in the object
    JSONVar keys = myObject.keys();

    for (int i = 0; i < keys.length(); i++)
    {
      JSONVar value = myObject[keys[i]];

      if (i == 0)
      {
        String idStr = JSON.stringify(value);
        //Serial.println("length: " +String(idStr.length()));
        idStr = idStr.substring(1, idStr.length() - 1);
        id = idStr.toInt();
        //Serial.println(idStr);

      }
    }

    Serial.println("id: " + String(id));
  }
  else {
    Serial.println("WiFi Disconnected");
  }
}

void puttosleep()
{
  digitalWrite(4, LOW);
  digitalWrite(12, LOW);

  gpio_hold_en(GPIO_NUM_4);
  gpio_deep_sleep_hold_en();

  WiFi.disconnect();
  esp_deep_sleep_start();
}

/////////////////////////////////////////////////////////////////////////////////////// interrupt

RTC_DATA_ATTR boolean startFlag = false;
///////////////////////////////////////////////////////////////////////////////////////// connecting to wifi
//const char *ssid = "realme 3 Pro";   // your network SSID
//const char *password = "1234567890"; // your network password

//const char *ssid = "DESKTOP-AD153PA 0263";   // your network SSID
//const char *password = "1234567890"; // your network password

const char *ssid = "realme 3 Pro";   // your network SSID
const char *password = "1234567890"; // your network password

//IPAddress local_IP(192, 168, 164, 203); //194.147
//// Set your Gateway IP address
//IPAddress gateway(192, 168, 164, 138); //192.168.207.21
//
//IPAddress subnet(255, 255, 255, 0);
//IPAddress primaryDNS(8, 8, 8, 8);   //optional
//IPAddress secondaryDNS(8, 8, 4, 4); //optional

void connectWiFi()
{
  
//  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
//    Serial.println("STA Failed to configure");
//  }
  
  Serial.println("Connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  long tim ;
  tim = millis();
  int count = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(50);
    digitalWrite(12, HIGH);
    delay(50);
    digitalWrite(12, LOW);
    Serial.print(".");
    if (millis() - tim > 3000) {
      count ++;
      Serial.println(count);
      if (count >= 5)
        puttosleep();
      Serial.println("WiFi not found");
      WiFi.begin(ssid, password);
      tim = millis();
    }
  }

  Serial.println("");
  Serial.print("Connected to WiFi network in " + String(((millis() - tim) / 1000.0)) + " seconds with IP Address: ");
  Serial.println(WiFi.localIP());

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    http.begin(client, serverName);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  }
  else {
    Serial.println("WiFi Disconnected");
  }

  Serial.print("Stream Link: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/mjpeg/1");
  server.on("/mjpeg/1", HTTP_GET, handle_jpg_stream);
  //  server.on("/jpg", HTTP_GET, handle_jpg);
  server.onNotFound(handleNotFound);
  server.begin();
}
/////////////////////////////////////////////////////////////////////////////////////////////////// send ip address to db

void sendStreamLink(int type, String streamLink)
{
  String apiKeyValue = "tPmAT5Ab3j7F9";

//  IPAddress link = WiFi.localIP();
//  String streamLink = "http://" + String(link[0]) + String(".") +\
//  String(link[1]) + String(".") +\
//  String(link[2]) + String(".") +\
//  String(link[3]) +"/mjpeg/1";
  
//  Serial.println(streamLink);
//  Serial.println(streamLink.length());

//  int type = 4; //4
  
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    HTTPClient http;

    http.begin(client, "http://sensorlifeline.com/post-esp.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String httpRequestData = "api_key=" + apiKeyValue + "&id=" + id + "&type=" + type + "&value1=" + streamLink + "";
//    Serial.print("httpRequestData: ");
//    Serial.println(httpRequestData);

    int httpResponseCode;
    httpResponseCode = http.POST(httpRequestData);

    if (httpResponseCode > 0)
    {
//      Serial.print("HTTP Response code: ");
//      Serial.println(httpResponseCode);
    }
    else
    {
      for (int i = 2; i > 0; i++)
      {
        httpResponseCode = http.POST(httpRequestData);
        if (httpResponseCode == 200)
          break;
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      Serial.println("error while uploading");
    }

    http.end();
  }
  else
  {
    //Serial.println("WiFi Disconnected");
    connectWiFi();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////// streaming

const char HEADER[] = "HTTP/1.1 200 OK\r\n"
                      "Access-Control-Allow-Origin: *\r\n"
                      "Content-Type: multipart/x-mixed-replace; boundary=1234567890\r\n";
const char BOUNDARY[] = "\r\n--1234567890\r\n";
const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";
const int hdrLen = strlen(HEADER);
const int bdrLen = strlen(BOUNDARY);
const int cntLen = strlen(CTNTTYPE);


void handle_jpg_stream(void)
{
  char buf[32];
  int s;
  camera_fb_t * fb = NULL;
  WiFiClient client = server.client();
  client.write(HEADER, hdrLen);
  client.write(BOUNDARY, bdrLen);

  digitalWrite(4, HIGH);

  //    long time;

  while (true)
  {
    if (!client.connected()) {
      digitalWrite(4, LOW);
      break;
    }

    //    time = millis();

    fb = esp_camera_fb_get();
    s = fb->len;

    uint8_t * buffer = fb->buf;
    client.write(CTNTTYPE, cntLen);
    sprintf(buf, "%d\r\n\r\n", s);
    client.write(buf, strlen(buf));
    client.write((char *)buffer, s);
    client.write(BOUNDARY, bdrLen);

    if (!digitalRead(camTrig))
    {
      delay(100);
      if (!digitalRead(camTrig))
      {
        digitalWrite(4, LOW);
        sendPhoto(fb);
        esp_camera_fb_return(fb);

        puttosleep();
      }
    }
    esp_camera_fb_return(fb);
    //    Serial.println(String(millis() - time));
  }
}

void handleNotFound()
{
  String message = "Server is running!\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  server.send(200, "text / plain", message);
}

String urlencode(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (c == ' ')
    {
      encodedString += '+';
    }
    else if (isalnum(c))
    {
      encodedString += c;
    }
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
      {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9)
      {
        code0 = c - 10 + 'A';
      }
      code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
      // encodedString+=code2;
    }
    yield();
  }
  return encodedString;
}

/////////////////////////////////////////////////////////////////////////////////////////////////// setup

void setup()
{
  Serial.begin(115200);
  pinMode(12, OUTPUT);
  digitalWrite(12, HIGH);
  delay(100);
  digitalWrite(12, LOW);
  pinMode(4, OUTPUT);

  pinMode(camTrig, INPUT_PULLUP);

  //  ledcSetup(flash, flashFreq, flashRes);
  //  ledcAttachPin(4, flash);
  digitalWrite(4, LOW);
  gpio_hold_dis(GPIO_NUM_4);
  gpio_deep_sleep_hold_dis();
  pinMode(13, INPUT_PULLUP);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 0);
  if (startFlag == false)
  {
    digitalWrite(4, LOW);
    digitalWrite(12, LOW);

    gpio_hold_en(GPIO_NUM_4);
    gpio_deep_sleep_hold_en();
    //    configCam();
    Serial.println("Press Button");
    while (!digitalRead(13));
    startFlag = true;
    puttosleep();
  }

  Serial.println("cam config time: " + String((long)(millis() - tim) / 1000));

  connectWiFi();
  configCam();
  sensor_t * s = esp_camera_sensor_get();
  s->set_hmirror(s, 1);
  getPatientDetail();
  delay(100);

  /////////////////////////////////////////////////
  IPAddress link = WiFi.localIP();
  String streamLink = "http://" + String(link[0]) + String(".") +\
  String(link[1]) + String(".") +\
  String(link[2]) + String(".") +\
  String(link[3]) +"/mjpeg/1";
  sendStreamLink(4, streamLink);
  ////////////////////////////////////////////////
  //  delay(100);
  //  Serial.println("Cam Configured");
  //  connectWiFi();
  //  ov5640.autoFocusMode();
}

////////////////////////////////////////////////////////////////////////////////////////////////// loop

void loop()
{
  server.handleClient();
}
