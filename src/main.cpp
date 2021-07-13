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

#define DEBUG 

//define pins
#define FLASH_LED_PIN 4
#define MOVEMENT_SENSOR_PIN 14
#define LIGHT_SENSOR_PIN 15
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
  #ifdef DEBUG
    Serial.begin(115200);
  #endif

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
    #ifdef DEBUG
    Serial.println("Camera Setup Failed!");
    #endif
    while (true){
      delay(100);
      digitalWrite(FLASH_LED_PIN,0);
      delay(100);
     digitalWrite(FLASH_LED_PIN,1);
    }
  }

  //connect to networke
  #ifdef DEBUG
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  #endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Add root certificate for api.telegram.org
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED){
    #ifdef DEBUG
      Serial.print(".");
    #endif
    delay(500);
  }
  #ifdef DEBUG
    Serial.print("\nWiFi connected. IP address: ");
    Serial.println(WiFi.localIP().toString());
  #endif

  //set commands
  bot_setup();

  //send /activate message
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
      #ifdef DEBUG
        Serial.println("got response");
      #endif
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }
}

void bot_setup(){
  const String commands = F("["
                              "{\"command\":\"help\",  \"description\":\"\xE2\x9D\x94 Print short overview\"},"
                              "{\"command\":\"start\",  \"description\":\"\xE2\x9C\xA8 Print info and short manual\"},"
                              "{\"command\":\"status\",  \"description\":\"\xF0\x9F\x9A\xA5 Print status of the device (cammera, flash and brightness)\"},"
                              "{\"command\":\"activate\", \"description\":\"\xE2\x9A\xA0 Activate image capture on movement\"},"
                              "{\"command\":\"deactivate\",  \"description\":\"\xF0\x9F\x9A\xAB Deactivate image capture on movement\"},"
                              "{\"command\":\"flashon\", \"description\":\"\xE2\x9C\x94 Always use flash\"},"
                              "{\"command\":\"flashoff\", \"description\":\"\xE2\x9D\x8C Never use flash\"},"
                              "{\"command\":\"flashauto\", \"description\":\"\xF0\x9F\x91\x80 Automatically use flash if needed\"},"
                              "{\"command\":\"brightness\", \"description\":\"\xF0\x9F\x8C\x9E Tells you if auto mode will use flash\"},"
                              "{\"command\":\"image\", \"description\":\"\xF0\x9F\x93\xB7 Take an image\"}"
                            "]");
  #ifdef DEBUG
    Serial.println("commands: "+ bot.setMyCommands(commands) ? "true":"false");
  #else
    bot.setMyCommands(commands); 
  #endif
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
    #ifdef DEBUG
      Serial.println("Camera capture failed");
    #endif
    bot.sendMessage(CHAT_ID, "Camera capture failed", "");
    return;
  }
  //data are available
  dataAvailable = true;
  #ifdef DEBUG
    Serial.println("Sending");
  #endif
  bot.sendPhotoByBinary(CHAT_ID, "image/jpeg", fb->len, isMoreDataAvailable, nullptr, getNextBuffer, getNextBufferLen);

  //prepare for next use
  esp_camera_fb_return(fb);

  //trun flash off
  digitalWrite(FLASH_LED_PIN,0);
  #ifdef DEBUG
    Serial.println("done!");
  #endif
}

void handleNewMessages(int numNewMessages){
  #ifdef DEBUG
    Serial.print("handleNewMessages ");
    Serial.println(numNewMessages);
  #endif

  String answer;
  for (int i = 0; i < numNewMessages; i++)
  {
    telegramMessage &msg = bot.messages[i];
    if(msg.text.length() <= 0) return;
    #ifdef DEBUG
      Serial.println("Received " + msg.text+"\"");
    #endif
    if (msg.text == "/start"){
      answer = "*Huhu*, the camera \xF0\x9F\x93\xB7 is now active \xE2\x9A\xA0 and will send images when a movement is detected. "
              "You can change the state with /activate \xE2\x9A\xA0 and /deactivate \xF0\x9F\x9A\xAB. The red light, at the back shows, "
              "when the camera \xF0\x9F\x93\xB7  is active \xE2\x9A\xA0.\n\n "

              "The Flash \xF0\x9F\x92\xA1 will be used automatically. This can be adjust by using /flashon \xE2\x9C\x94, /flasfoff \xE2\x9D\x8C and /flashauto \xF0\x9F\x91\x80. "
              "To define when the automatic mode use the flash the LDR is used. By using the poti at the bottom, the brightness "
              "when to use the flash can be defined. To test this the button next to the poti can be used. "
              "Here, a blue LED \xF0\x9F\x94\xB5 show if the flash will be used (led off) or not (led on). "
              "Also /brightness \xF0\x9F\x8C\x9E returns information if the auto mode use the flash or not.\n\n "

              "By using the /image command a image \xF0\x9F\x93\xB7 can be toke manually.\n\n"

              "Use /help for a short overview over all commands. ";

      bot.sendMessage(CHAT_ID, answer, "Markdown");
    }else if (msg.text == "/help"){
      answer = F( "/help       - \xE2\x9D\x94 Print this message\n"
                  "/start      - \xE2\x9C\xA8 Print info and short manual\n"
                  "/status     - \xF0\x9F\x9A\xA5 Print status of the device (cammera, flash and brightness)"
                  "/activate   - \xE2\x9A\xA0 Activate image capture on movement\n"
                  "/deactivate - \xF0\x9F\x9A\xAB Deactivate image capture on movement\n"
                  "/flashon    - \xE2\x9C\x94 Always use flash\n"
                  "/flashoff   - \xE2\x9D\x8C Never use flash\n"
                  "/flashauto  - \xF0\x9F\x91\x80 Automatically use flash if needed\n"
                  "/brightness - \xF0\x9F\x8C\x9E Tells you if auto mode will use flash\n"
                  "/image      - \xF0\x9F\x93\xB7 Take an image");
      bot.sendMessage(CHAT_ID, answer, "Markdown");
    }else if (msg.text == "/status"){
      String status = "*deactive* \xF0\x9F\x9A\xAB";
      if(isActive){
        status = "*active* \xE2\x9A\xA0";
      }

      String flash = "*Auto* \xF0\x9F\x91\x80";
      if(!flashAuto){
        if(useFlash){
          flash = "*on* \xE2\x9C\x94";
        }else{
          flash = "*off* \xE2\x9D\x8C";
        }
      }

      String bright = "*dim*";
      if(digitalRead(LIGHT_SENSOR_PIN)){
        bright = "*bright*";
      }

      answer =  "\xF0\x9F\x9A\xA5 Status:\n"
        	      "\xF0\x9F\x93\xB7 Camera:\t\t"+status + "\n"
                "\xF0\x9F\x92\xA1 Flash:\t\t"+flash + "\n"
                "\xF0\x9F\x8C\x9E Brightness:\t"+bright;
      bot.sendMessage(CHAT_ID, answer, "Markdown");
    }else if (msg.text == "/activate"){
      isActive = true;
      answer = "Camera is *active* \xE2\x9A\xA0!";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
      digitalWrite(INTERNAL_LED,0);
    }else if (msg.text == "/deactivate"){
      isActive = false;
      answer = "Camera is *deactive* \xF0\x9F\x9A\xAB!";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
      digitalWrite(INTERNAL_LED,1);
    }else if (msg.text == "/flashon"){
      useFlash = true;
      flashAuto = false;
      answer = "Flash is *on* \xE2\x9C\x94";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
    }else if (msg.text == "/flashoff"){
      useFlash = false;
      flashAuto = false;
      answer = "Flash is *off* \xE2\x9D\x8C";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
    }else if (msg.text == "/flashauto"){
      flashAuto = true;
      useFlash = false;
      answer = "*Automatically* \xF0\x9F\x91\x80 use flash";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
    }else if (msg.text == "/brightness"){
      if(!digitalRead(LIGHT_SENSOR_PIN)){
        answer = "Flash is automatically set to *on* \xE2\x9C\x94";
      }else{
        answer = "Flash is automatically set to *off* \xE2\x9D\x8C";
      }
      bot.sendMessage(CHAT_ID, answer, "Markdown");
    }else if (msg.text == "/image"){
      sendImage();
    }else{
      answer = "\""+msg.text+ "\" is *unknown* \xF0\x9F\x98\x9F! Use /help for more information";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
    }
  }
}