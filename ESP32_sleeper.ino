#define BUTTON_PIN_BITMASK 0x200000000 // 2^33 in hex
#define BLYNK_PRINT Serial
#define REPORT_SIZE 10
#define EEPROM_SIZE 512
#define BATTERY_MONITOR 34
#define BATTERY_MONITOR_ENABLE 25
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */

// Library includes
#include <WiFi.h>
#include "WiFi_list.h"
#include <WiFiClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <BlynkSimpleEsp32.h>
#include <EEPROM.h>
#include <time.h>
#include "Battery.h"

// Battery initiation
Battery battery(3400, 4200, BATTERY_MONITOR);

// Blynk initiation
BlynkTimer timer;
char auth[] = "f0fa7767c6524ab7b8e64def84a816ca";
WidgetTerminal terminal(V2);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

// Deep sleep constants
RTC_DATA_ATTR int bootCount = 0;

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case 1:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      terminal.println("Wakeup caused by external signal using RTC_IO");
      terminal.flush();
      break;
    case 2:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      terminal.println("Wakeup caused by external signal using RTC_CNTL");
      terminal.flush();
      break;
    case 3:
      Serial.println("Wakeup caused by timer");
      terminal.println("Wakeup caused by timer");
      terminal.flush();
      break;
    case 4:
      Serial.println("Wakeup caused by touchpad");
      terminal.println("Wakeup caused by touchpad");
      terminal.flush();
      break;
    case 5:
      Serial.println("Wakeup caused by ULP program");
      terminal.println("Wakeup caused by ULP program");
      terminal.flush();
      break;
    default:
      Serial.println("Wakeup was not caused by deep sleep");
      terminal.println("Wakeup was not caused by deep sleep");
      terminal.flush();
      break;
  }
  terminal.println("--------------------------------------------");
  terminal.flush();
}

void shiftReportData(){
  for(int i = (REPORT_SIZE + 1) * 8 - 1; i > 7; --i){
    EEPROM.write(i + 8, EEPROM.read(i)); EEPROM.commit();
  }
  Serial.println("Shifted all report data down one cell.");
}

int safeToReport(int year, int month, int day, int hour, int minute, int second, int action, int reserve){
  shiftReportData();
  EEPROM.write(8, year);
  EEPROM.write(9, month);
  EEPROM.write(10, day);
  EEPROM.write(11, hour);
  EEPROM.write(12, minute);
  EEPROM.write(13, second);
  EEPROM.write(14, action);
  EEPROM.write(15, reserve);
  EEPROM.commit();
  Serial.println("Saved latest incident to report data.");
}

void emailReport(){
  String report = {};
  report = "The latest incidents were:<br>";
  
  String tempIncident = {};
  for(int i = 0; i < 80; i = i + 8){
    tempIncident = String(EEPROM.read(10 + i)) + "." + String(EEPROM.read(9 + i)) + ".20" + String(EEPROM.read(8 + i)) + " " + String(EEPROM.read(11 + i)) + ":" + String(EEPROM.read(12 + i)) + ":" + String(EEPROM.read(13 + i)) + "  " + String(descideIncident(14 + i)) + "<br>";
    report = report + tempIncident;
  }
  
  Blynk.email("adrian.schwizgebel@gmail.com", "Daily gremln report.", report);
  Serial.println("Email has been sent.");
}

float ADCtoVoltage(){
  float voltage = float(analogRead(BATTERY_MONITOR)) / 827.2727;
  return voltage;
}

byte VoltageToPercentage(float voltage){
  byte percentage = int((voltage - 3.0) * 83.0);
  return percentage;
}

String descideIncident(int address){
  switch(EEPROM.read(address)){
    case 0:
      return "Nothing has happened, not sure why I woke up.";
      break;
    case 1:
      return "GPIO-33 woke me up.";
      break;
    default:
      return "Not sure what has happened.";
      break;
  }
}



void notifyAlarm(){
  Blynk.notify("Someone woke up Gremln");
  Serial.println("Mobile has been notified.");
}

void setup() {
  delay(10);
  pinMode(BATTERY_MONITOR_ENABLE, OUTPUT);
  digitalWrite(BATTERY_MONITOR_ENABLE, HIGH);
  Serial.begin(115200);
  Serial.println("________________________________________________________________");
  Serial.println("Just woke up.");
  battery.onDemand(BATTERY_MONITOR_ENABLE, HIGH);
  battery.begin(5000, 1.0, &sigmoidal);
  Serial.println(analogRead(BATTERY_MONITOR));
  Serial.println(ADCtoVoltage());
  EEPROM.begin(EEPROM_SIZE);
  
  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  
  // Start Blynk
  Blynk.begin(auth, ssid[CHOOSE_WIFI], pass[CHOOSE_WIFI]);
  //if(bootCount == 1) terminal.clear();
  Serial.println("Blynk running.");

  // Do all the NTP shit
  // Initialize an NTPClient to get time
  timeClient.begin();
  timeClient.setTimeOffset(3600); // 1 hour = 3600
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  formattedDate = timeClient.getFormattedDate();
  
  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  Serial.print("DATE: "); Serial.println(dayStamp);
  delay(10);
  
  // Extract time
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  Serial.print("HOUR: "); Serial.println(timeStamp);
  delay(10);

  int times[10] = {};
  for(int i = 0; i < 8; i++){
    times[i] = timeStamp[i] - 48;
  }

  int days[20] = {};
  for(int i = 0; i < 16; i++){
    days[i] = dayStamp[i] - 48;
  }
  
  // Safe to email report
  int year = 10 * days[2] + days[3];
  int month = 10 * days[5] + days[6];
  int day = 10 * days[8] + days[9];
  int hour = 10 * times[0] + times[1];
  int minute = 10 * times[3] + times[4];
  int second = 10 * times[6] + times[7];
  int action = 1;
  int reserve = 0;
  safeToReport(year, month, day, hour, minute, second, action, reserve);
  
  // Blynk notification
  //notifyAlarm();
  Blynk.virtualWrite(V0, battery.level());
  Blynk.virtualWrite(V3, VoltageToPercentage(ADCtoVoltage()));
  Blynk.virtualWrite(V1, battery.voltage());
  Blynk.virtualWrite(V4, ADCtoVoltage());
  delay(10);

  // Blynk email report
  //emailReport();
  delay(10);
  
  // Print to blynk terminal
  terminal.println("Date: " + String(dayStamp) + "\nTime: " + String(timeStamp));
  terminal.println("BootCount: " + String(bootCount));
  terminal.println("Battery voltage: " + String(battery.voltage()) + "V - " + String(battery.level()) + "%");
  terminal.flush();
  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  // Configure wake-up source
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,1); //1 = High, 0 = Low

  // Blynk interrupts
  //timer.setInterval(250L, blinkLedWidget);

  for(int i = 0; i < 40; i++){
    Blynk.run();
    timer.run();
    delay(30);
  }
  digitalWrite(BATTERY_MONITOR_ENABLE, LOW);
  //Go to sleep now
  Serial.println("Going to sleep now");
  delay(10);
  esp_deep_sleep_start();
}

void loop() {
}
