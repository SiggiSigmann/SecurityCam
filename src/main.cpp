#include <Arduino.h>

//Json library for communication
#include <ArduinoJson.h>

//cammera
#include "esp_camera.h"
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include "camera_code.h"

//permanent storage
#include <EEPROM.h>

//telegram bot
#include <UniversalTelegramBot.h>

//real time clock
#include <RtcDS3231.h>
#include <RtcDateTime.h>
#include <Wire.h>

//wifi
#include <WiFiManager.h>
#include <WiFiClientSecure.h>

#include "secrets.h"

#define DEBUG 0

//define pins
#define FLASH_LED_PIN 4
#define MOVEMENT_SENSOR_PIN 14
#define LIGHT_SENSOR_PIN 15
#define INTERNAL_LED 33
#define SDA1 2
#define SCL1 13

//def time
const unsigned long CHECK_BREAK = 500; //time to wait between breaks
unsigned long lastTimeMessageCheck = millis();

//wifi and bot connection
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

//imagebuffer
camera_fb_t *fb = NULL;
bool dataAvailable = false;

//RTC
//Check if RTC is active
TwoWire myWire = TwoWire(0);
RtcDS3231<TwoWire> Rtc(myWire);
#define countof(a) (sizeof(a) / sizeof(a[0]))

//ieings falgs
bool isActive = true;
bool useFlash = false;
bool flashAuto = false;
bool nextSetTime = false;
bool nextSetAlarm = false;
bool isAlarmActive = true;
bool nextAlarmActivate = false;
bool nextAlarmDeactivate = false;

//when was laram last set
char lastUpdate = 0;

//store when to autoactivate the alarm for each day => 7 Days a week
struct AutoActivate{
  bool active;
  char time[7];
};
AutoActivate autoactivation[7];

//eeprom adresses
int const eeproSize = sizeof(isActive)+sizeof(useFlash)+sizeof(flashAuto)+sizeof(isAlarmActive)+sizeof(autoactivation);
int const addressIsActive = 0;
int const addressUseFlash = sizeof(isActive) + addressIsActive;
int const addressFlashAuto = sizeof(useFlash) + addressUseFlash;
int const addressIsAlarmActive= sizeof(flashAuto) + addressFlashAuto;
int const addressAutoActivate = sizeof(isAlarmActive) + addressIsAlarmActive;

//methods
void bot_setup();
void setupRTC();
void checkForMessanges();
void sendImage();
void handleNewMessages(int);
void help();
void start();
void status();
void activate();
void deactivate();
void flashon();
void flashoff();
void flashauto();
void brightness();
void image();
void settime();
void displayTime();
void changeTime(String);
void autoactivate();
void setalarm();
void changeAlarm(String);
void checkAutoActivation();
void setTime();
void showalarm();
void deactivatealarm();
void activatealarm();
bool isMoreDataAvailable();
void activatealarmforday();
void deactivatealarmforday();
void changeAlarmAktivityPerDay(String);
void changeAlarmDeaktivityPerDay(String);
void temperatur();
byte* getNextBuffer();
int getNextBufferLen();
void getRTCStatus();
void activateRTC();
void deactvateRTC();

//weekdaystrings
String weekdays[7] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

void setup() {
  #ifdef DEBUG
    Serial.begin(115200);
  #endif

  //pinmodes
  pinMode(MOVEMENT_SENSOR_PIN,INPUT);
  pinMode(LIGHT_SENSOR_PIN,INPUT);
  pinMode(FLASH_LED_PIN, OUTPUT);
  pinMode(INTERNAL_LED, OUTPUT);

  WiFiManager wm;
  //wm.resetSettings();                                //reset Wifimanager => credentials will be deleted
  wm.setClass("invert"); // dark theme
  bool res = wm.autoConnect("SecurityCam"); // password protected ap

  if(!res) {
    #ifdef DEBUG
      Serial.println("Failed to connect");
      // ESP.restart();
      #endif
  } 

  //turn on internal led
  digitalWrite(INTERNAL_LED,0);
  delay(1000);
  digitalWrite(INTERNAL_LED,1);

  //eeprom load
  EEPROM.begin(eeproSize);
  //load from mempory
  EEPROM.get(addressIsActive, isActive);
  EEPROM.get(addressUseFlash, useFlash);
  EEPROM.get(addressFlashAuto, flashAuto);
  EEPROM.get(addressIsAlarmActive, isAlarmActive);
  EEPROM.get(addressAutoActivate, autoactivation);

  //inital for storage
  //autoactivation
  //autoactivation[0] = AutoActivate{false, "000000"};
  //autoactivation[1] = AutoActivate{false, "000000"};
  //autoactivation[2] = AutoActivate{false, "000000"};
  //autoactivation[3] = AutoActivate{false, "000000"};
  //autoactivation[4] = AutoActivate{false, "000000"};
  //autoactivation[5] = AutoActivate{false, "000000"};
  //autoactivation[6] = AutoActivate{false, "000000"};
  //EEPROM.put(addressAutoActivate, autoactivation);

  //setup cammera
  if (!setupCamera()){
    #ifdef DEBUG
      Serial.println("Camera Setup Failed!");
    #endif
    while (true){
      delay(100);
      digitalWrite(INTERNAL_LED,0);
      delay(100);
      digitalWrite(INTERNAL_LED,1);
    }
  }

  //change cammera settings
  //sensor_t * s = esp_camera_sensor_get();
  //s->set_vflip(s, 1);
  //s->set_hmirror(s, 1);

  //rtc
  myWire.begin(SDA1,SCL1,400000);
  setupRTC();

  //connect to networke
  // Add root certificate for api.telegram.org
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED){
    #ifdef DEBUG
      Serial.print(".");
    #endif
    delay(200);
    digitalWrite(INTERNAL_LED,0);
    delay(200);
    digitalWrite(INTERNAL_LED,1);
  }
  #ifdef DEBUG
    Serial.print("\nWiFi connected. IP address: ");
    Serial.println(WiFi.localIP().toString());
  #endif

  //set commands
  bot_setup();

  //send /activate message
  bot.sendMessage(CHAT_ID, "Bot started up with IP: "+ WiFi.localIP().toString(), ""); 
  
  digitalWrite(INTERNAL_LED, isActive);
}

void loop() {
  //check if movement
  if(isActive){
    if(digitalRead(MOVEMENT_SENSOR_PIN)){
      sendImage();
      delay(500);
    }
  }

  //handel messanges and alarm
  if (millis() - lastTimeMessageCheck > CHECK_BREAK){
    checkForMessanges();
    checkAutoActivation();
  }
}

//check if cammera has to be auto activated
void checkAutoActivation(){
  //check if alarm should be used
  if(isAlarmActive){

    //get Date
    RtcDateTime now = Rtc.GetDateTime();

    //check if alarm was used tody
    if(lastUpdate != now.DayOfWeek()){
      #ifdef DEBUG
        Serial.println("alarm not used today");
      #endif

      //get alarm
      AutoActivate todaysActivation = autoactivation[now.DayOfWeek()]; //i changed mondey to 0 in the enumeration
      
      //check if alarm is active
      if(todaysActivation.active){
        #ifdef DEBUG
          Serial.println("execute alarm");
        #endif

        //compare now with alarm 
        String timeofAlarm = todaysActivation.time;
        RtcDateTime alarmeTime = RtcDateTime(now.Year(),now.Month(), now.Day(), timeofAlarm.substring(0,2).toInt(), timeofAlarm.substring(2,4).toInt(), timeofAlarm.substring(4,6).toInt());
        if(now > alarmeTime){
          //start alarm
          #ifdef DEBUG
            Serial.println("start");
          #endif
          lastUpdate = now.DayOfWeek();
          autoactivate();
        }
      }
    }
  }
}

//
void autoactivate(){
  isActive = true;
  EEPROM.write(addressIsActive, isActive);
  EEPROM.commit();

  String answer = "Camera is automatically \xF0\x9F\xA4\x96 set *active* \xE2\x9A\xA0!";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

//send all commands to telegram => they will be klickabel
void bot_setup(){
  Serial.println("setup bot");
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
                              "{\"command\":\"image\", \"description\":\"\xF0\x9F\x93\xB7 Take an image\"},"
                              "{\"command\":\"displaytime\", \"description\":\"\xF0\x9F\x95\x92 Show time\"},"
                              "{\"command\":\"settime\", \"description\":\"\xF0\x9F\x95\x92 \xE2\x9C\x8F Change Time\"},"
                              "{\"command\":\"showalarm\", \"description\":\"\xE2\x8F\xB0 Show all Alarms\"},"
                              "{\"command\":\"setalarm\", \"description\":\"\xE2\x8F\xB0 \xE2\x9C\x8F Set / Change new Alarm\"},"
                              "{\"command\":\"activatealarm\", \"description\":\"\xF0\x9F\xA4\x96 \xE2\x9C\x94 Activate Alarm\"},"
                              "{\"command\":\"deactivatealarm\", \"description\":\"\xF0\x9F\xA4\x96 \xF0\x9F\x9A\xAB Deactivate Alarm\"},"
                              "{\"command\":\"activatealarmforday\", \"description\":\"\xF0\x9F\xA4\x96 \xE2\x9C\x94 Activate Alarm for a Day\"},"
                              "{\"command\":\"deactivatealarmforday\", \"description\":\"\xF0\x9F\xA4\x96 \xF0\x9F\x9A\xAB Deactivate Alarm for a Day\"},"
                              "{\"command\":\"temperatur\", \"description\":\"\xE2\x99\xA8 Display RTC Temperatur\"},"
                              "{\"command\":\"getRTCStatus\", \"description\":\"returns RTC status\"},"
                              "{\"command\":\"activateRTC\", \"description\":\"activate RTC\"},"
                              "{\"command\":\"deactvateRTC\", \"description\":\"deactivate RTC => prepare for storage\"}"
                            "]");

  #ifdef DEBUG
    Serial.println("commands: "+ bot.setMyCommands(commands) ? "true":"false");
  #else
    bot.setMyCommands(commands); 
  #endif
}

void setupRTC(){
  Rtc.Begin();

  //check if update is needed
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid()) {
      if (Rtc.LastError() != 0) {
          // we have a communications error
          // see https://www.arduino.cc/en/Reference/WireEndTransmission for 
          // what the number means
          #ifdef DEBUG
            Serial.print("RTC communications error = ");
            Serial.println(Rtc.LastError());
          #endif
      }else{
          // Common Causes:
          //    1) first time you ran and the device wasn't running yet
          //    2) the battery on the device is low or even missing
          #ifdef DEBUG
            Serial.println("RTC lost confidence in the DateTime!");
          #endif

          // following line sets the RTC to the date & time this sketch was compiled
          // it will also reset the valid flag internally unless the Rtc device is
          // having an issue

          Rtc.SetDateTime(compiled);
      }
  }

  if (!Rtc.GetIsRunning()){
      #ifdef DEBUG
        Serial.println("RTC was not actively running, starting now");
      #endif
      Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled){
    #ifdef DEBUG
      Serial.println("RTC is older than compile time!  (Updating DateTime)");
    #endif
      Rtc.SetDateTime(compiled);
  }else if (now > compiled) {
    #ifdef DEBUG
      Serial.println("RTC is newer than compile time. (this is expected)");
    #endif
  }else if (now == compiled) {
    #ifdef DEBUG
      Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    #endif
  }

  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
}

//handel new message
void checkForMessanges(){
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages){
    #ifdef DEBUG
      Serial.println("got response");
    #endif
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
  lastTimeMessageCheck = millis();

}

//select handelmethod
void handleNewMessages(int numNewMessages){
  #ifdef DEBUG
    Serial.print("handleNewMessages ");
    Serial.println(numNewMessages);
  #endif

  for (int i = 0; i < numNewMessages; i++) {
    telegramMessage &msg = bot.messages[i];

    //check if messange contains any text
    if(msg.text.length() <= 0) return;
    
      #ifdef DEBUG
        Serial.println("Received " + msg.text+"\"");
      #endif
      
      if (msg.text == "/start"){
        start();
      }else if (msg.text == "/help"){
        help();
      }else if (msg.text == "/status"){
        status();
      }else if (msg.text == "/activate"){
        activate();
      }else if (msg.text == "/deactivate"){
        deactivate();
      }else if (msg.text == "/flashon"){
        flashon();
      }else if (msg.text == "/flashoff"){
        flashoff();
      }else if (msg.text == "/flashauto"){
        flashauto();
      }else if (msg.text == "/brightness"){
        brightness();
      }else if (msg.text == "/image"){
        image();
      }else if (msg.text == "/displaytime"){
        displayTime();
      }else if (msg.text == "/settime"){
        setTime();
      }else if (msg.text == "/showalarm"){
        showalarm();
      }else if (msg.text == "/setalarm"){
        setalarm();
      }else if (msg.text == "/activatealarm"){
        activatealarm();
      }else if (msg.text == "/deactivatealarm"){
        deactivatealarm();
      }else if (msg.text == "/activatealarmforday"){
        activatealarmforday();
      }else if (msg.text == "/deactivatealarmforday"){
        deactivatealarmforday();
      }else if (msg.text == "/temperatur"){
        temperatur();
      }else if (msg.text == "/getRTCStatus"){
        getRTCStatus();
      }else if (msg.text == "/activateRTC"){
        activateRTC();
      }else if (msg.text == "/deactvateRTC"){
        deactvateRTC();
      }else{
        if(nextSetTime){
          changeTime(msg.text);
        }else if(nextSetAlarm){
          changeAlarm(msg.text);
        }else if(nextAlarmActivate){
          changeAlarmAktivityPerDay(msg.text);
        }else if(nextAlarmDeactivate){
          changeAlarmDeaktivityPerDay(msg.text);
        }else{
          String answer = "\""+msg.text+ "\" is *unknown* \xF0\x9F\x98\x9F! Use /help for more information";
          bot.sendMessage(CHAT_ID, answer, "Markdown");
        }
      }

      if(!( msg.text == "/setalarm")){
        if(nextSetAlarm) nextSetAlarm = false;
      }
      if(!( msg.text == "/settime")){
        if(nextSetTime) nextSetTime = false;
      }
      if(!( msg.text == "/activatealarmforday")){
        if(nextAlarmActivate) nextAlarmActivate = false;
      }
      if(!( msg.text == "/deactivatealarmforday")){
        if(nextAlarmDeactivate) nextAlarmDeactivate = false;
      }
  }
}

/*
* #####################################################################################
* info stuff
*/
void help(){
  String answer = F(  "/help        - \xE2\x9D\x94 Print this message\n"
                      "/start       - \xE2\x9C\xA8 Print info and short manual\n"
                      "/status      - \xF0\x9F\x9A\xA5 Print status of the device (cammera, flash and brightness)\n"
                      "/activate    - \xE2\x9A\xA0 Activate image capture on movement\n"
                      "/deactivate  - \xF0\x9F\x9A\xAB Deactivate image capture on movement\n"
                      "/flashon     - \xE2\x9C\x94 Always use flash\n"
                      "/flashoff    - \xE2\x9D\x8C Never use flash\n"
                      "/flashauto   - \xF0\x9F\x91\x80 Automatically use flash if needed\n"
                      "/brightness  - \xF0\x9F\x8C\x9E Tells you if auto mode will use flash\n"
                      "/image       - \xF0\x9F\x93\xB7 Take an image\n"
                      "/displaytime - \xF0\x9F\x95\x92 Show time\n"
                      "/settime     - \xF0\x9F\x95\x92 \xE2\x9C\x8F change time\n"
                      "/showalarm   - \xE2\x8F\xB0 show all alarms\n"
                      "/setalarm    - \xE2\x8F\xB0 \xE2\x9C\x8F Change Alarm Settings\n"
                      "/activatealarm   - \xF0\x9F\xA4\x96 \xE2\x9C\x94 activate Alarm\n"
                      "/deactivatealarm    - \xF0\x9F\xA4\x96 \xE2\x9C\x8F deactivate Alarm\n"
                      "/activatealarmforday - \xF0\x9F\xA4\x96 \xE2\x9C\x94 Activate Alarm for a Day\n"
                      "/deactivatealarmforday - \xF0\x9F\xA4\x96 \xF0\x9F\x9A\xAB Deactivate Alarm for a Day\n"
                      "/temperatur - \xE2\x99\xA8 Show RTC temperature");
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

//send starttext
void start(){
  String answer = "*Huhu*, the camera \xF0\x9F\x93\xB7 is now active \xE2\x9A\xA0 and will send images when a movement is detected. "
                  "You can change the state with /activate \xE2\x9A\xA0 and /deactivate \xF0\x9F\x9A\xAB. The red light, at the back shows, "
                  "when the camera \xF0\x9F\x93\xB7  is active \xE2\x9A\xA0.\n\n "

                  "The Flash \xF0\x9F\x92\xA1 will be used automatically. This can be adjust by using /flashon \xE2\x9C\x94, /flashoff \xE2\x9D\x8C and /flashauto \xF0\x9F\x91\x80. "
                  "To define when the automatic mode use the flash the LDR is used. By using the poti at the bottom, the brightness "
                  "when to use the flash can be defined. To test this the button next to the poti can be used. "
                  "Here, a blue LED \xF0\x9F\x94\xB5 show if the flash will be used (led off) or not (led on). "
                  "Also /brightness \xF0\x9F\x8C\x9E returns information if the auto mode use the flash or not.\n\n "

                  "By using the /image command a image \xF0\x9F\x93\xB7 can be toke manually.\n\n"

                  "The internal Time \xF0\x9F\x95\x92 can be checked with /displaytime. The Time can be adjusted \xE2\x9C\x8F by using /settime."
                  "The time will be used to activate the cammera automatically\xF0\x9F\xA4\x96. To show the Alarm \xE2\x8F\xB0 settings use /showalarm. /setalarm can be used to "
                  "change the settings. By using /activatealarm or /deactivatealarm, the alarm \xF0\x9F\xA4\x96 can be switched on or off"

                  "Use /help for a short overview over all commands. ";

  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void status(){
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

  String alarm = "*deactive* \xF0\x9F\x9A\xAB";
  if(isAlarmActive){
    alarm = "*active* \xE2\x9C\x94";
  }

  String answer =  "\xF0\x9F\x9A\xA5 Status:\n"
            "\xF0\x9F\x93\xB7 Camera:\t\t"+status + "\n"
            "\xF0\x9F\x92\xA1 Flash:\t\t"+flash + "\n"
            "\xF0\x9F\x8C\x9E Brightness:\t"+bright+"\n"
            "\xF0\x9F\xA4\x96 Alarm:\t"+alarm;
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void temperatur(){
  String answer =  "The RTC hast " + String(Rtc.GetTemperature().AsFloatDegC()) + "C°";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

/*
* #####################################################################################
* activation stuff
*/
void activate(){
  isActive = true;
  EEPROM.write(addressIsActive, isActive);
  EEPROM.commit();
  
  String answer = "Camera is *active* \xE2\x9A\xA0!";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
  digitalWrite(INTERNAL_LED,0);
}

void deactivate(){
  isActive = false;
  EEPROM.write(addressIsActive, isActive);
  EEPROM.commit();

  String answer = "Camera is *deactive* \xF0\x9F\x9A\xAB!";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
  digitalWrite(INTERNAL_LED,1);
}

/*
* #####################################################################################
* flash stuff
*/
void flashon(){
  useFlash = true;
  flashAuto = false;
  EEPROM.write(addressUseFlash, useFlash);
  EEPROM.write(addressFlashAuto, flashAuto);
  EEPROM.commit();

  String answer = "Flash is *on* \xE2\x9C\x94";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void flashoff(){
  useFlash = false;
  flashAuto = false;
  EEPROM.write(addressUseFlash, useFlash);
  EEPROM.write(addressFlashAuto, flashAuto);
  EEPROM.commit();

  String answer = "Flash is *off* \xE2\x9D\x8C";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void flashauto(){
  flashAuto = true;
  useFlash = false;
  EEPROM.write(addressUseFlash, useFlash);
  EEPROM.write(addressFlashAuto, flashAuto);
  EEPROM.commit();

  String answer1 = "set the flash to *off* \xE2\x9D\x8C";
  if(!digitalRead(LIGHT_SENSOR_PIN)){
    answer1 = " set the flash to *on* \xE2\x9C\x94";
  }
  String answer = "*Automatically* \xF0\x9F\x91\x80 use flash and " + answer1;
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void brightness(){
  String bright = "*dim*";
  if(digitalRead(LIGHT_SENSOR_PIN)){
    bright = "*bright*";
  }
  String answer = "Lighting conditions are " + bright;
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

/*
* #####################################################################################
* alarm stuff
*/
void displayTime(){
  RtcDateTime dt = Rtc.GetDateTime();
  char datestring[20];

  snprintf_P(datestring, countof(datestring), PSTR("%02u.%02u.%04u %02u:%02u:%02u (%10s)"),
          dt.Day(),
          dt.Month(),
          dt.Year(),
          dt.Hour(),
          dt.Minute(),
          dt.Second(),
          weekdays[dt.DayOfWeek()].c_str());

  String answer = "The Time is: " + String(datestring);
  bot.sendMessage(CHAT_ID, answer + " Day of the Week: " + weekdays[dt.DayOfWeek()], "Markdown");
}

void setTime(){
  nextSetTime = true;

  String answer = "Enter new Time in formate DD.MM.YYYY hh:mm:ss";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void changeTime(String time){
  //check dateformat
  int idx = 0;
  for(;idx<10;idx++){
    char a = time.charAt(idx);
    if(!((48 <= a && a <= 57) || a == 46)){
      Serial.println(a);
      String answer = "Wrong format \xF0\x9F\x98\x94 in Date part. Pleas try again by using the cammand /settime again";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
      return;
    }
  }

  //check splitt
  if(!(time.charAt(idx++)==32)){
    String answer = "Wrong format \xF0\x9F\x98\x94. Pleas try again by using the cammand /settime again";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
      return;
  } 

  //time
  for(;idx<19;idx++){
    char a = time.charAt(idx);
    if(!((48 <= a && a <= 57) || a == 58)){
      String answer = "Wrong format \xF0\x9F\x98\x94 in Date time. Pleas try again by using the cammand /settime again";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
      return;
    }
  }

  int spiltpoint = time.indexOf(" ");
  String date = time.substring(0,spiltpoint);
  String timeClock = time.substring(spiltpoint+1,spiltpoint+1+6);
  RtcDateTime compiled = RtcDateTime(date.substring(6,10).toInt(),date.substring(3,7).toInt(),date.substring(0,2).toInt(),
            timeClock.substring(0,2).toInt(),timeClock.substring(3,5).toInt(),timeClock.substring(6,8).toInt());
  Rtc.SetDateTime(compiled);

  String answer = "Time set sucessfully \xF0\x9F\x98\x81";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
  displayTime();
}

/*
* #####################################################################################
* alarm stuff
*/
void showalarm(){
  String answere = "";

  for ( int fooInt = DayOfWeek_Sunday; fooInt != DayOfWeek_Saturday+1; fooInt++ ){
    DayOfWeek foo = static_cast<DayOfWeek>(fooInt);

    AutoActivate toPrint = autoactivation[foo];

    #ifdef DEBUG
      Serial.println(String(toPrint.time) + " " + String(toPrint.active));
    #endif

    String time = String(toPrint.time);

    String printing = "Day:" + String(weekdays[foo]) + " Time:" + time.substring(0,2) +":"+time.substring(2,4)+":"+time.substring(4,6) + " Active:" + String(toPrint.active)+"\n";
    answere += printing;
  }
  String answer = "The alarm settings are: \n";
  bot.sendMessage(CHAT_ID, answer+answere, "Markdown");
}

void setalarm(){
  nextSetAlarm= true;

  String answer = "Enter new Alarm in formate:\n [Day Of the Week] hh:mm:ss [activ]\nDay Of the Week: Mondaty = 0, Sunnday = 6\nactiv: 0 / 1";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void changeAlarm(String message){
  char a = message.charAt(0);
  a -= 48;
  if(!((a>=0 && a <= 6) | a ==(58-48))){
    String answer = "Error in Day Format, try again by using the /setalarm command";
    bot.sendMessage(CHAT_ID, answer, "Markdown");
    return;
  }

  for(int idx = 2; idx<10; idx++){
    char b = message.charAt(0);
    if(!(48 <= b && b <= 57)){
      String answer = "Error in Time Format, try again by using the /setalarm command";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
      return;
    }
  }

  String time = message.substring(2,10);

  char b = message.charAt(11);
  b=b-48;
  if(!(b>=0 && b <= 1)){
    String answer = "Error in Active Format, try again by using the /setalarm command";
    bot.sendMessage(CHAT_ID, answer, "Markdown");
    return;
  }

  //update alarm
  autoactivation[(int)a].active = (b==1);
  String timeparsed = time.substring(0,2)+time.substring(3,5)+time.substring(6,8);
  strncpy( autoactivation[(int)a].time, timeparsed.c_str(), 16);

  EEPROM.put(addressAutoActivate, autoactivation);
  EEPROM.commit();

  String answer = "\xE2\x8F\xB0 Alarm changed";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void activatealarm(){
  isAlarmActive = true;
  EEPROM.write(addressIsAlarmActive, isAlarmActive);
  EEPROM.commit();

  String answer = "\xF0\x9F\xA4\x96 \xE2\x9C\x94 Activate Alarm";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void deactivatealarm(){
  isAlarmActive = false;
  EEPROM.write(addressIsAlarmActive, isAlarmActive);
  EEPROM.commit();

  String answer = "\xF0\x9F\xA4\x96 \xF0\x9F\x9A\xAB Deactivate Alarm";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void activatealarmforday(){
  nextAlarmActivate = true;
  String answer = "Enter Day for which the alarm should be activated (0-Mondey, 6-Sunday)";
  String keybord = "[[\"0\"], [\"1\"], [\"2\"], [\"3\"], [\"4\"], [\"5\"], [\"6\"]]";
  bot.sendMessageWithReplyKeyboard(CHAT_ID, answer, "Markdown", keybord, true);
}

void deactivatealarmforday(){
  nextAlarmDeactivate = true;
  String answer = "Enter Day for which the alarm should be deactivated (0-Mondey, 6-Sunday)";
  String keybord = "[[\"0\"], [\"1\"], [\"2\"], [\"3\"], [\"4\"], [\"5\"], [\"6\"]]";
  bot.sendMessageWithReplyKeyboard(CHAT_ID, answer, "Markdown", keybord, true);
}

void changeAlarmAktivityPerDay(String m){
  char a = m.charAt(0)-48;
  if(!(a>=0 && a<=6)){
      String answer = "Input must be between 0 and 6 (Mondaty = 0 - Sunday = 6)";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
      return;
  }

    //update alarm
  autoactivation[(int)a].active = 1;
  EEPROM.put(addressAutoActivate, autoactivation);
  EEPROM.commit();

  String answer = "\xE2\x8F\xB0 Alarm changed";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void changeAlarmDeaktivityPerDay(String m){
  char a = m.charAt(0)-48;
  if(!(a>=0 && a<=6)){
      String answer = "Input must be between 0 and 6 (Mondaty = 0 - Sunday = 6)";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
      return;
  }

    //update alarm
  autoactivation[(int)a].active = 0;
  EEPROM.put(addressAutoActivate, autoactivation);
  EEPROM.commit();

  String answer = "\xE2\x8F\xB0 Alarm changed";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

/*
* #####################################################################################
* cammera stuff
*/
void image(){
  sendImage();
}

void sendImage(){
  //check if flash is needed
  if(useFlash | (flashAuto & !digitalRead(LIGHT_SENSOR_PIN))){
    digitalWrite(FLASH_LED_PIN,1);
  }

  fb = NULL;
  // Take Picture with Camera
  fb = esp_camera_fb_get();
  if (!fb){
    #ifdef DEBUG
    Serial.println("Camera capture failed");
    #endif
    bot.sendMessage(CHAT_ID, "Camera capture failed", "");
    return;
  }
  dataAvailable = true;
  #ifdef DEBUG
  Serial.println("Sending");
  #endif
  bot.sendPhotoByBinary(CHAT_ID, "image/jpeg", fb->len,
                        isMoreDataAvailable, nullptr,
                        getNextBuffer, getNextBufferLen);

  esp_camera_fb_return(fb);

  //trun flash off
  digitalWrite(FLASH_LED_PIN,0);
  #ifdef DEBUG
    Serial.println("done!");
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

/*
* #####################################################################################
* RTC activation / deactivation
*/
void getRTCStatus(){
  String answer = "RTC status: " + String(Rtc.GetIsRunning());
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void activateRTC(){
  Rtc.SetIsRunning(true);
  getRTCStatus();

  setupRTC();
}

void deactvateRTC(){
  Rtc.SetIsRunning(false);
  getRTCStatus();
}