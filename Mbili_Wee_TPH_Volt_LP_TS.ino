//Measure every 15 seconds and sends to ThingSpeak every 30 minutes
//wireless.ictp.it/rwanda_2015 RJC
#include <Wire.h>
//#include <SeeedOLED.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

//SODAQ Mbili libraries
#include <Sodaq_BMP085.h>
#include <Sodaq_SHT2x.h>
#include <Sodaq_DS3231.h>
#include <GPRSbee.h>
#include <Sodaq_PcInt.h>

//Wee library
#include "ESP8266.h"

//The sleep time in seconds (MAX 86399)
//Each time clock is refreshed
#define SLEEP_PERIOD 60

//The quantity of lectures before sending (Total time to send=PERIOD_SEND times SLEEP_PERIOD)
#define PERIOD_SEND 30

//Define power pin to switch onoff bee
#define GROVE_PWR 22

//RTC Interrupt pin and period
#define RTC_PIN A7

//Data header
#define DATA_HEADER "TimeDate, TempSHT21, TempBMP, PressureBMP, HumiditySHT21, BatVoltage"

#define SSID "MarconiLab"
#define PASSWORD   "marconi-lab"
#define HOST_NAME   "api.thingspeak.com"
#define HOST_PORT   (80)

//Add your apikey (THINGSPEAK_KEY) from your channel
// GET /update?key=[THINGSPEAK_KEY]&field1=[data 1]&field2=[data 2]...;
String GET = "GET /update?key=NZV1O6QF1SJ8TMB4";


//Seperators
#define FIRST_SEP "?"
#define OTHER_SEP "&"
#define LABEL_DATA_SEP "="

//Data labels, cannot change for ThingSpeak (max 8 fields)
#define LABEL1 "field1"
#define LABEL2 "field2"
#define LABEL3 "field3"
#define LABEL4 "field4"
#define LABEL5 "field5"
#define LABEL6 "field6"
#define LABEL7 "field7"
#define LABEL8 "field8"

//TPH BMP sensor
Sodaq_BMP085 bmp;

//Wee WIFI
ESP8266 wifi(Serial1);

//init cont in 29 to post in firt run
int cont=PERIOD_SEND-1;

//Sodaq_DS3231 RTC; //Create RTC object for DS3231 RTC come Temp Sensor 
char weekDay[][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
//year, month, date, hour, min, sec and week-day(starts from 0 and goes to 6)
//writing any non-existent time-data may interfere with normal operation of the RTC.
//Take care of week-day also.*/
DateTime dt(2015, 07, 8, 15, 43, 00, 3);

//These constants are used for reading the battery voltage
#define ADC_AREF 3.3
#define BATVOLTPIN A6
#define BATVOLT_R1 4.7
#define BATVOLT_R2 10

void setup() 
{
  //Initialise the serial connection
  Serial.begin(9600);
  Serial.println("Power up...");
  
  //set power pin as output and switch on
  pinMode(GROVE_PWR,OUTPUT);// (D22). 
  digitalWrite(GROVE_PWR, HIGH);
  
  //Initialise sensors
  setupSensors();
  
  //Setup Wifi connection
  setupWifi();

  //Setup sleep mode
  setupSleep();

  //Setup Oled Display
  //setupOled();
  
  //Echo the data header to the serial connection
  Serial.println(DATA_HEADER);
 
}

void loop() 
{
  //Take readings
  takeReading(getNow()); 
    
  //Sleep
  systemSleep(); 

}

void setupSensors()
{
  //Initialise the wire protocol for the TPH sensors
  Wire.begin();
  
  //Initialise the TPH BMP sensor
  bmp.begin();

  //Initialise the DS3231 RTC
  rtc.begin();
  //rtc.setDateTime(dt); //Adjust date-time as defined 'dt' above
}

void setupWifi(){      
    Serial.print("FW Version:");
    Serial.println(wifi.getVersion().c_str());
      
    if (wifi.setOprToStationSoftAP()) {
        Serial.print("to station + softap ok\r\n");
    } else {
        Serial.print("to station + softap err\r\n");
    }
 
    if (wifi.joinAP(SSID, PASSWORD)) {
        Serial.print("Join AP success\r\n");
        Serial.print("IP:");
        Serial.println( wifi.getLocalIP().c_str());       
    } else {
        Serial.print("Join AP failure\r\n");
    }
    
    if (wifi.disableMUX()) {
        Serial.print("single ok\r\n");
    } else {
        Serial.print("single err\r\n");
    }
}

void WifiOn(){      
    if (wifi.joinAP(SSID, PASSWORD)) {
        Serial.print("Join AP success\r\n");
        Serial.print("IP:");
        Serial.println( wifi.getLocalIP().c_str());       
    } else {
        Serial.print("Join AP failure\r\n");
    }
}
  
void setupSleep()
{
  pinMode(RTC_PIN, INPUT_PULLUP);
  PcInt::attachInterrupt(RTC_PIN, wakeISR);
  
  //Set the sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
}

void wakeISR()
{
  //Leave this blank
}

void setupOled()
{
  SeeedOled.clearDisplay();          //clear the screen and set start position to top left corner
  SeeedOled.setNormalDisplay();      //Set display to normal mode (i.e non-inverse mode)
  SeeedOled.setPageMode();           //Set addressing mode to Page Mode
  //SeeedOled.setTextXY(0,5);          //Set the cursor to Xth Page, Yth Column  
  //SeeedOled.putString("IoTea"); //Print the String
 }

void systemSleep()
{
  Serial.print("Sleeping in low-power mode for ");
  Serial.print(SLEEP_PERIOD/60.0);
  Serial.println(" minutes");
  
  //Wait until the serial ports have finished transmitting
  Serial.flush();
  Serial1.flush();
  
  //Schedule the next wake up pulse timeStamp + SLEEP_PERIOD
  DateTime wakeTime(getNow() + SLEEP_PERIOD);
  rtc.enableInterrupts(wakeTime.hour(), wakeTime.minute(), wakeTime.second());
  
  //The next timed interrupt will not be sent until this is cleared
  rtc.clearINTStatus();
  
  //Grove off
 // digitalWrite(GROVE_PWR, LOW);
  
  //Disable ADC
  ADCSRA &= ~_BV(ADEN);
  
  //Sleep time
  noInterrupts();
  sleep_enable();
  interrupts();
  sleep_cpu();
  sleep_disable();
 
  //Enbale ADC
  ADCSRA |= _BV(ADEN);
//  digitalWrite(GROVE_PWR, HIGH);
  Serial.println("Waking-up");
}

void takeReading(uint32_t ts)
{
  //Create the data record
  String dataRec = createDataRecord();
  
  //Echo the data to the serial connection
  Serial.println(dataRec);
  
  //print values into Oled Display
  //oleddata();
  
  cont++;
  Serial.println(cont);
  if (cont==PERIOD_SEND)
   {
    //Grove on
    digitalWrite(GROVE_PWR, HIGH);
    
    WifiOn();
    
    //Create TCP connection
    createTCP();
  
    //Read sensor values
    String Temp1=String(SHT2x.GetTemperature());
    String Temp2=String(bmp.readTemperature());
    String Press=String(bmp.readPressure() / 100);
    String Hum=String(SHT2x.GetHumidity());
  
    //Read the voltage
    int mv = getRealBatteryVoltage() * 1000.0;
    String Smv=String(mv);
  
    Serial.print(Temp1);Serial.print(",");
    Serial.print(Temp2);Serial.print(",");
    Serial.print(Press);Serial.print(",");
    Serial.print(Hum);Serial.print(",");
    Serial.println(Smv);

    //post to TS
    updateTS( Temp1, Temp2, Press, Hum, Smv );

    if (wifi.leaveAP()) {
        Serial.print("Left AP success\r\n");
    } else {
        Serial.print("Left AP failure\r\n");
    }
    
    //Grove off
    digitalWrite(GROVE_PWR, LOW);
    cont=0;
  }
}

void createTCP(){
    uint8_t buffer[128] = {0};
    
    if (wifi.createTCP(HOST_NAME, HOST_PORT)) {
        Serial.print("create tcp ok\r\n");
    } else {
        Serial.print("create tcp err\r\n");
    }
}

//----- update the  Thingspeak string with 3 values
void updateTS( String T1, String T2 , String P, String H, String V)
{
  //create GET message
  String cmd = GET + "&field1=" + T1 +"&field2="+ T2 + "&field3=" + P +"&field4=" + H +"&field5=" + V+"\r\n";
   
   char buf[150]; //make buffer large enough for 7 digits
   cmd.toCharArray(buf, sizeof(buf));
    
    wifi.send((const uint8_t*)buf, strlen(buf));
    
    uint8_t buffer[128] = {0};
    uint32_t len = wifi.recv(buffer, sizeof(buffer), 10000);
    if (len > 0) {
        Serial.print("Received:[");
        for(uint32_t i = 0; i < len; i++) {
            Serial.print((char)buffer[i]);
        }
        Serial.print("]\r\n");
    }
    
    if (wifi.releaseTCP()) {
        Serial.print("release tcp ok\r\n");
    } else {
        Serial.print("release tcp err\r\n");
    }
}

 void oleddata()
 {
  DateTime now = rtc.now(); //get the current date-time    
  SeeedOled.init();                  //initialize SEEED OLED display
  SeeedOled.clearDisplay();          //clear the screen and set start position to top left corner
  SeeedOled.setTextXY(0,0);          //Set the cursor to Xth Page, Yth Column
  SeeedOled.putNumber(now.year());
  SeeedOled.putString("-");
  SeeedOled.putNumber(now.month());
  SeeedOled.putString("-");
  SeeedOled.putNumber(now.date()); 
  SeeedOled.putString("  ");
  SeeedOled.putString(weekDay[now.dayOfWeek()]);  
  SeeedOled.setTextXY(1,7);          //Set the cursor to Xth Page, Yth Column
  SeeedOled.putNumber(now.hour());
  SeeedOled.putString(":");
  SeeedOled.putNumber(now.minute());
  SeeedOled.putString(":");
  SeeedOled.putFloat(now.second(),0); 

    
  SeeedOled.setTextXY(3,0);          //Set the cursor to Xth Page, Yth Column  
  SeeedOled.putString("TEMP = "); //Print the String
  String date1 = String (bmp.readTemperature()) ;
  SeeedOled.putString(date1.c_str()); //Print the String
  SeeedOled.setTextXY(3,13);          //Set the cursor to Xth Page, Yth Column
  SeeedOled.putString("C"); //Print the String
  
  SeeedOled.setTextXY(5,0);          //Set the cursor to Xth Page, Yth Column  
  SeeedOled.putString("HUMI = "); //Print the String
  String date2 = String (SHT2x.GetHumidity()) ;
  SeeedOled.putString(date2.c_str()); //Print the String
  SeeedOled.setTextXY(5,13);          //Set the cursor to Xth Page, Yth Column
  SeeedOled.putString("%"); //Print the String
  
  SeeedOled.setTextXY(6,0);          //Set the cursor to Xth Page, Yth Column  
  SeeedOled.putString("PRES = "); //Print the String
  String date3 = String ((bmp.readPressure() / 100)) ;
  SeeedOled.putString(date3.c_str()); //Print the String
  SeeedOled.setTextXY(6,12);          //Set the cursor to Xth Page, Yth Column
  SeeedOled.putString(" hPa"); //Print the String*/
  
  SeeedOled.setTextXY(7,0);          //Set the cursor to Xth Page, Yth Column  
  SeeedOled.putString("BATT = "); //Print the String
  int mv = getRealBatteryVoltage() * 1000.0;
  String Smv=String(mv);
  SeeedOled.putString(Smv.c_str()); //Print the String
  SeeedOled.setTextXY(7,12);          //Set the cursor to Xth Page, Yth Column
  SeeedOled.putString(" mA"); //Print the String*/
 }

String createDataRecord()
{
  //Create a String type data record in csv format
  //TimeDate, TempSHT21, TempBMP, PressureBMP, HumiditySHT21, Voltage
  String data = getDateTime() + ", ";
  data += String(SHT2x.GetTemperature())  + ", ";
  data += String(bmp.readTemperature()) + ", ";
  data += String(bmp.readPressure() / 100)  + ", ";
  data += String(SHT2x.GetHumidity()) + ", ";
  data += String(getRealBatteryVoltage() * 1000);
  
  return data;
}

String getDateTime()
{
  String dateTimeStr;
  
  //Create a DateTime object from the current time
  DateTime dt(rtc.makeDateTime(rtc.now().getEpoch()));

  //Convert it to a String
  dt.addToString(dateTimeStr);
  
  return dateTimeStr;  
}

uint32_t getNow()
{
  return rtc.now().getEpoch();
}

float getRealBatteryVoltage()
{
  uint16_t batteryVoltage = analogRead(BATVOLTPIN);
  return (ADC_AREF / 1023.0) * (BATVOLT_R1 + BATVOLT_R2) / BATVOLT_R2 * batteryVoltage;
}
