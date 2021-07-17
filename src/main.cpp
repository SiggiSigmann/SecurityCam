#include <Arduino.h>
#include <ArduinoJson.h>

#include "esp_camera.h"
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include "camera_code.h"

#include <EEPROM.h>

#include <UniversalTelegramBot.h>

//#include <RtcDS3231.h>
//#include <RtcDateTime.h>

#include "secrets.h"
//#include <SoftwareWire.h> 

#include <WiFi.h>
#include <WiFiClientSecure.h>

#define DEBUG 

//define pins
#define FLASH_LED_PIN 4
#define MOVEMENT_SENSOR_PIN 14
#define LIGHT_SENSOR_PIN 15
#define INTERNAL_LED 33

//def time
const unsigned long CHECK_BREAK = 5000; //time to wait between breaks
unsigned long lastTimeMessageCheck = millis();

//wifi and bot connection
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

//imagebuffer
camera_fb_t *fb = NULL;
bool dataAvailable = false;

//RTC
//SoftwareWire myWire(2, 3);
//RtcDS3231<SoftwareWire> Rtc(myWire);
//#define countof(a) (sizeof(a) / sizeof(a[0]))

//settings
bool isActive = true;
bool useFlash = false;
bool flashAuto = false;
bool nextSetTime = false;
bool nextSetAlarm = false;

struct AutoActivate{
  bool active;
  String time;
};
AutoActivate autoactivation[7];

//eeprom
int const eeproSize = sizeof(isActive)+sizeof(useFlash)+sizeof(flashAuto);
int const addressIsActive = 0;
int const addressUseFlash = sizeof(isActive) + addressIsActive;
int const addressFlashAuto = sizeof(useFlash) + addressUseFlash;

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
//void printDateTime(const RtcDateTime& dt);
void settime();
void displayTime();
void changeTime(String);
void autoactivate();
void setalarm();
void changeAlarm(String);
void checkAutoActivation();
void setTime();
void showalarm();



bool isMoreDataAvailable();
byte* getNextBuffer();
int getNextBufferLen();

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

  //eeprom load
  EEPROM.begin(eeproSize);
  //load from mempory
  EEPROM.get(addressIsActive, isActive);
  EEPROM.get(addressUseFlash, useFlash);
  EEPROM.get(addressFlashAuto, flashAuto);

  //autoactivation
  autoactivation[0] = AutoActivate{false, "00:00:00"};
  autoactivation[1] = AutoActivate{false, "00:00:00"};
  autoactivation[2] = AutoActivate{false, "00:00:00"};
  autoactivation[3] = AutoActivate{false, "00:00:00"};
  autoactivation[4] = AutoActivate{false, "00:00:00"};
  autoactivation[5] = AutoActivate{false, "00:00:00"};
  autoactivation[6] = AutoActivate{false, "00:00:00"};

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
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);

  //rtc
  //setupRTC();

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

  if (millis() - lastTimeMessageCheck > CHECK_BREAK){
    checkForMessanges();

    //checkAutoActivation();
  }
  //handel message

}

/*
char lastUpdate = 0;
void checkAutoActivation(){
  RtcDateTime now = Rtc.GetDateTime();

  if(lastUpdate != now.DayOfWeek()){
    AutoActivate todaysActivation = autoactivation[now.DayOfWeek()]; //i changed mondey to 0 in the enumeration

    if(todaysActivation.active){
      char datestring[8];
        snprintf_P(datestring, 
                countof(datestring),
                PSTR("%02u/%02u/%04u"),
                now.Month(),
                now.Day(),
                now.Year());
      Serial.print("TOday: " + String(datestring));

      RtcDateTime alarmeTime = RtcDateTime(datestring, todaysActivation.time.c_str());

      if(now > alarmeTime){
        lastUpdate = now.DayOfWeek();
        autoactivate();
      }
    }
  }
}
*/
/*
void autoactivate(){
  isActive = true;
  EEPROM.write(addressIsActive, isActive);
  EEPROM.commit();

  String answer = "Camera is automatically \xF0\x9F\xA4\x96 set *active* \xE2\x9A\xA0!";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}*/

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
                              "{\"command\":\"image\", \"description\":\"\xF0\x9F\x93\xB7 Take an image\"},"
                              /*"{\"command\":\"displaytime\", \"description\":\"\xF0\x9F\x95\x92 Show time\"},"
                              "{\"command\":\"settime\", \"description\":\"\xF0\x9F\x95\x92 \xE2\x9C\x8F Change Time\"},"
                              "{\"command\":\"showalarm\", \"description\":\"\xE2\x8F\xB0 Show all Alarms\"},"
                              "{\"command\":\"setalarm\", \"description\":\"\xE2\x8F\xB0 \xE2\x9C\x8F Set / Change new Alarm\"}"*/
                            "]");
  #ifdef DEBUG
    Serial.println("commands: "+ bot.setMyCommands(commands) ? "true":"false");
  #else
    bot.setMyCommands(commands); 
  #endif
}
/*
void setupRTC(){
  Rtc.Begin();

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    printDateTime(compiled);
    Serial.println();

    if (!Rtc.IsDateTimeValid()) 
    {
        if (Rtc.LastError() != 0)
        {
            // we have a communications error
            // see https://www.arduino.cc/en/Reference/WireEndTransmission for 
            // what the number means
            Serial.print("RTC communications error = ");
            Serial.println(Rtc.LastError());
        }
        else
        {
            // Common Causes:
            //    1) first time you ran and the device wasn't running yet
            //    2) the battery on the device is low or even missing

            Serial.println("RTC lost confidence in the DateTime!");

            // following line sets the RTC to the date & time this sketch was compiled
            // it will also reset the valid flag internally unless the Rtc device is
            // having an issue

            Rtc.SetDateTime(compiled);
        }
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) 
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    }
    else if (now > compiled) 
    {
        Serial.println("RTC is newer than compile time. (this is expected)");
    }
    else if (now == compiled) 
    {
        Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }

    // never assume the Rtc was last configured by you, so
    // just clear them to your needed state
    Rtc.Enable32kHzPin(false);
    Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
}*/

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
    /*}else if (msg.text == "/displaytime"){
      displayTime();
    }else if (msg.text == "/settime"){
      setTime();
    }else if (msg.text == "/showalarm"){
      showalarm();
    }else if (msg.text == "/setalarm"){
      setalarm();
    */}else{
      /*if(nextSetTime){
        changeTime(msg.text);
      }else if(nextSetAlarm){
        changeAlarm(msg.text);
      }else{*/
        String answer = "\""+msg.text+ "\" is *unknown* \xF0\x9F\x98\x9F! Use /help for more information";
        bot.sendMessage(CHAT_ID, answer, "Markdown");
      //}
    }
  }

  if(nextSetTime) nextSetTime = false;
  if(nextSetAlarm) nextSetAlarm = false;
}

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
                      /*"/displaytime - \xF0\x9F\x95\x92 Show time\n"
                      "/settime     - \xF0\x9F\x95\x92 \xE2\x9C\x8F change time\n"
                      "/showalarm   - \xE2\x8F\xB0 show all alarms\n"
                      "/setalarm    - \xE2\x8F\xB0 \xE2\x9C\x8F Change Alarm Settings"*/);
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

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

                  //"The internal Time \xF0\x9F\x95\x92 can be checked with /displaytime. The Time can be adjusted \xE2\x9C\x8F by using /settime."
                  //"The time will be used to acitvate the cammera automatically."

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

  String answer =  "\xF0\x9F\x9A\xA5 Status:\n"
            "\xF0\x9F\x93\xB7 Camera:\t\t"+status + "\n"
            "\xF0\x9F\x92\xA1 Flash:\t\t"+flash + "\n"
            "\xF0\x9F\x8C\x9E Brightness:\t"+bright;
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

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

void image(){
  sendImage();
}
/*
void displayTime(){
  RtcDateTime dt = Rtc.GetDateTime();
  char datestring[20];

  snprintf_P(datestring, countof(datestring), PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
          dt.Month(),
          dt.Day(),
          dt.Year(),
          dt.Hour(),
          dt.Minute(),
          dt.Second() );

  String answer = "The Time is: " + String(datestring);
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void setTime(){
  nextSetTime = true;

  String answer = "Enter new Time in formate MMDDYYYY HHMMSS";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void changeTime(String time){
  bool error = false;
  int idx = 0;
  for(;idx<8;idx++){
    char a = time.charAt(idx);
    if(!(48 >= a && a >= 57)){
      error = true;
    }
  }
  if(!(time.charAt(++idx)==32)) error = true;
  for(;idx<15;idx++){
    char a = time.charAt(idx);
    if(!(48 >= a && a >= 57)){
      error = true;
    }
  }

  if(!error){
    int spiltpoint = time.indexOf(" ");
    String date = time.substring(0,spiltpoint);
    String timeClock = time.substring(spiltpoint+1,spiltpoint+1+6);
    RtcDateTime compiled = RtcDateTime(date.c_str(), timeClock.c_str());
    Rtc.SetDateTime(compiled);

    String answer = "Time set sucessfully \xF0\x9F\x98\x81";
    bot.sendMessage(CHAT_ID, answer, "Markdown");
  }else{
    String answer = "Wrong format \xF0\x9F\x98\x94. Pleas try again by using the cammand /settime again";
    bot.sendMessage(CHAT_ID, answer, "Markdown");
  }
}

void showalarm(){
  String answere = "";

  for ( int fooInt = DayOfWeek_Monday; fooInt != DayOfWeek_Sunday; fooInt++ ){
    DayOfWeek foo = static_cast<DayOfWeek>(fooInt);

    AutoActivate toPrint = autoactivation[foo];

    String printing = "Day:" + String(foo) + " Time:" + String(toPrint.time) + " Active:" + String(toPrint.active)+"\n";
    answere += printing;
  }
  String answer = "The alarm settings are: \n";
  bot.sendMessage(CHAT_ID, answer+answere, "Markdown");
}

void setalarm(){
  nextSetAlarm= true;

  String answer = "Enter new Alarm in formate [Day Of the Week] HHMMSS [activ]\nDay Of the Week: Mondaty = 0, Sunnday = 6\nactiv: 0 / 1";
  bot.sendMessage(CHAT_ID, answer, "Markdown");
}

void changeAlarm(String message){
  char a = message.charAt(0);
  if(!(48 >= a && a >= 54)){
    String answer = "Error in Day Format, try again by using the /setalarm command";
    bot.sendMessage(CHAT_ID, answer, "Markdown");
  }

  a -= 48;

  for(int idx = 2; idx<8; idx++){
    char b = message.charAt(0);
    if(!(48 >= b && b >= 57)){
      String answer = "Error in Time Format, try again by using the /setalarm command";
      bot.sendMessage(CHAT_ID, answer, "Markdown");
    }
  }

  String time = message.substring(3,8);

  char b = message.charAt(9);
  if(!(48 >= b && b >= 49)){
    String answer = "Error in Active Format, try again by using the /setalarm command";
    bot.sendMessage(CHAT_ID, answer, "Markdown");
  }

  b=b-48;

  autoactivation[(int)a] = AutoActivate{(b==1), time};
}*/

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
