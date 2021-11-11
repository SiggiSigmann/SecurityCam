# SecurityCam
ESP-Cam take picture when movement detected and post it to telegram

<img src="https://raw.githubusercontent.com/SiggiSigmann/SecurityCam/main/img/3.jpg" alt="3" title="3"  width="25%"/>
<img src="https://raw.githubusercontent.com/SiggiSigmann/SecurityCam/main/img/1.jpg" alt="1" title="1"  width="25%"/>
<img src="https://raw.githubusercontent.com/SiggiSigmann/SecurityCam/main/img/2.jpg" alt="2" title="2"  width="25%"/>

## How to Install
1. copy secrets_temp.h and rename it to secrets.h
2. Add your Telegram BOT_TOKEN and CHAT_ID as describes by https://randomnerdtutorials.com/telegram-control-esp32-esp8266-nodemcu-outputs/
3. Flash ESP
4. To adjust when to used the flash use the Potentiometer. To check if the flash will be used press the button. If the LED glows then the flash will be used.
5. Adjust the activity and auto start time by using the telegram 


## Commands
* help => Print short overview
* start => Print info and short manual
* status => Print status of the device (cammera, flash and brightness)
* activate => Activate image capture on movement
* deactivate => Deactivate image capture on movement
* flashon => Always use flash
* flashoff => Never use flash
* flashauto => Automatically use flash if needed
* brightness => Tells you if auto mode will use flash
* image => Take an image
* displaytime => Show time
* settime =>  Change Time
* showalarm => Show all Alarms
* setalarm => Set / Change new Alarm
* activatealarm => Activate Alarm
* deactivatealarm => Deactivate Alarm
* activatealarmforday => Activate Alarm for a Day
* deactivatealarmforday => Deactivate Alarm for a Day
* temperatur => Display RTC Temperatur
* getRTCStatus => returns RTC status
* activateRTC => activate RTC
* deactvateRTC=> deactivate RTC => prepare for storage
## Circuit
<img src="https://raw.githubusercontent.com/SiggiSigmann/SecurityCam/main/img/circuit.jpg" alt="Circuit" title="Circuit" />
Made with https://easyeda.com/