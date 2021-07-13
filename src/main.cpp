#include <Arduino.h>
#include <ArduinoJson.h>

#include "esp_camera.h"
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include "camera_code.h"

#include <UniversalTelegramBot.h>

#include "secrets.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>

//define pins
#define MOVEMENT_SENSOR_PIN 14
#define LIGHT_SENSOR_PIN 15
#define FLASH_LED_PIN 4
#define INTERNAL_LED 33

//def time
const unsigned long BOT_MTBS = 1000;
unsigned long bot_lasttime = millis();

// Telegram BOT Token (Get from Botfather)
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

//imagebuffer
camera_fb_t *fb = NULL;

//settings
bool dataAvailable = false;
bool isActive = true;
bool useFlash = false;
bool flashAuto = true;

//methods
void bot_setup();
byte* getNextBuffer();
int getNextBufferLen();
void handleNewMessages(int);
bool isMoreDataAvailable();
void sendImage();

void setup() {
  //serial
  Serial.begin(115200);

  //pinmodes
  pinMode(MOVEMENT_SENSOR_PIN,INPUT);
  pinMode(LIGHT_SENSOR_PIN,INPUT);
  pinMode(FLASH_LED_PIN, OUTPUT);
  pinMode(INTERNAL_LED, OUTPUT);

  //turn on internal led
  digitalWrite(INTERNAL_LED,0);
  delay(1000);
  digitalWrite(INTERNAL_LED,1);

  //setup cammera
  if (!setupCamera()){
    Serial.println("Camera Setup Failed!");
    while (true){
      delay(100);
    }
  }

  //connect to networke
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Add root certificate for api.telegram.org
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP().toString());

  //set commands
  bot_setup();

  //send start message
  bot.sendMessage(CHAT_ID, "Bot started up with IP: "+ WiFi.localIP().toString(), ""); 
  
  digitalWrite(INTERNAL_LED,0);
}

void loop() {
  //check if movement
  if(isActive){
    if(digitalRead(MOVEMENT_SENSOR_PIN)){
      sendImage();
      delay(500);
    }
  }

  //handel message
  if (millis() - bot_lasttime > BOT_MTBS){
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages){
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
}

void bot_setup(){
  const String commands = F("["
                            "{\"command\":\"help\",  \"description\":\"Get bot usage help\"},"
                            "{\"command\":\"start\", \"description\":\"Start image captureing on movement\"},"
                            "{\"command\":\"stop\",  \"description\":\"Stop image captureing on movement\"},"
                            "{\"command\":\"flashon\", \"description\":\"Use flash when taking an image\"},"
                            "{\"command\":\"flashoff\", \"description\":\"Don't use flash when taking an image\"},"
                            "{\"command\":\"flashauto\", \"description\":\"Automatically use flash if needed\"},"
                            "{\"command\":\"brightness\", \"description\":\"Tells you if it is bright enough\"},"
                            "{\"command\":\"image\", \"description\":\"Take an image\"},"
                            "]");
  Serial.println("commands: "+ bot.setMyCommands(commands) ? "true":"false");
}

bool isMoreDataAvailable(){
  if (dataAvailable){
    dataAvailable = false;
    return true;
  }else{
    return false;
  }
}

byte* getNextBuffer(){
  if (fb){
    return fb->buf;
  }else{
    return nullptr;
  }
}

int getNextBufferLen(){
  if (fb){
    return fb->len;
  }else{
    return 0;
  }
}

void sendImage(){
  //check if flash is needed
  if(useFlash | (flashAuto & !digitalRead(LIGHT_SENSOR_PIN))){
    digitalWrite(FLASH_LED_PIN,1);
  }

  // Take Picture with Camera
  fb = NULL;
  fb = esp_camera_fb_get();
  //process image
  if (!fb){
    Serial.println("Camera capture failed");
    bot.sendMessage(CHAT_ID, "Camera capture failed", "");
    return;
  }
  //data are available
  dataAvailable = true;
  Serial.println("Sending");
  bot.sendPhotoByBinary(CHAT_ID, "image/jpeg", fb->len, isMoreDataAvailable, nullptr, getNextBuffer, getNextBufferLen);

  //prepare for next use
  esp_camera_fb_return(fb);

  //trun flash off
  digitalWrite(FLASH_LED_PIN,0);
  Serial.println("done!");
}

void handleNewMessages(int numNewMessages){
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);
  
  String answer;
  for (int i = 0; i < numNewMessages; i++)
  {
    telegramMessage &msg = bot.messages[i];
    Serial.println("Received " + msg.text+"\"");
    if (msg.text == "/help"){
      answer = "start, stop, flashon, flashoff, flashauto, brightness, image";
      bot.sendMessage(CHAT_ID, answer, "");
    }else if (msg.text == "/start"){
      isActive = true;
      answer = "Camera is active!";
      bot.sendMessage(CHAT_ID, answer, "");
      digitalWrite(INTERNAL_LED,0);
    }else if (msg.text == "/stop"){
      isActive = false;
      answer = "Camera deactivated!";
      bot.sendMessage(CHAT_ID, answer, "");
      digitalWrite(INTERNAL_LED,1);
    }else if (msg.text == "/flashon"){
      useFlash = true;
      flashAuto = false;
      answer = "Flash will be used";
      bot.sendMessage(CHAT_ID, answer, "");
    }else if (msg.text == "/flashoff"){
      useFlash = false;
      flashAuto = false;
      answer = "Flash won't be used";
      bot.sendMessage(CHAT_ID, answer, "");
    }else if (msg.text == "/flashauto"){
      flashAuto = true;
      useFlash = false;
      answer = "Automatically use flash";
      bot.sendMessage(CHAT_ID, answer, "");
    }else if (msg.text == "/brightness"){
      answer = "Brightness: " + !digitalRead(LIGHT_SENSOR_PIN);
      bot.sendMessage(CHAT_ID, answer, "");
    }else if (msg.text == "/image"){
      sendImage();
    }else{
      answer = "Say what?";
      bot.sendMessage(CHAT_ID, answer, "");
    }
  }
}