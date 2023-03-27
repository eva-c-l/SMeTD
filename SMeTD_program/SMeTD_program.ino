/**************************************************************************
  This is a program to read six temperature sensors based on the MCP98078
  microchip.  It has been designed for Eva Loerke, a student at The James
  Hutton Institute, and will be used to measure the temperature of water
  at depths of: Surface, 5cm below surface and then further depths of 10cm, 20cm,
  35cm and 55cm. (However, these depths can be amended at any time.) The air temperature
  will also be logged.
  The unit, and its associated software, has been designed and built by
  David Drummond of The James Hutton Institute.
  November 2019
*/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_MCP9808.h"
#include <SPI.h>
#include <SD.h>
#include <LowPower.h>
#include "RTClib.h"
#include <avr/wdt.h>
#include <EEPROM.h>


//RTC_DS1307 rtc;
RTC_DS3231 rtc;

char myDateString[10]; //19 digits plus the null char
char myTimeString[10]; //19 digits plus the null char

/**************THE FOLLOWING LINE SETS THE INTERVAL FOR READINGS*****************
   The interval is set multiples of 4 seconds but this can be chaged on the 'SLEEP' tab
   A sleep time of 0 will set the interval to every 4 seconds (approx)
   Note: the exact interval will be different because there are inherent time delays
   in the program as well as programmed delays.
*/
int sleepTime = 13; // a sleep time of 225 gives approx 900 seconds (15 Minutes) of sleep at 4 second intervals.
/********************************************************************************/

int sleepCount = 0, ID, num = 0000;
//const int SDPower = 9;
const int chipSelect = 10;  //this is the correct pin when using the GPS Shield.  Any other pin may be used if desired for other setups
int oldDay;

File dataFile;
char filename[] = "00000000.TXT";
float c1, c2, c3, c4, c5, c6;

Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
#define sleepNow (true)  //enables the sleep function
#define SDCard (true) //make false if you dont want to save the data
#define TCAADDR 0x70
int newDay = 0; //adds a date stamp to the sd card

void setup() {
  Serial.begin(9600);
  wdt_enable(WDTO_8S);   // Enable the watchdog timer with an 8 second timeout
  while (!Serial); //waits for serial terminal to be open, necessary in newer arduino boards.
  Serial.println(F("The James Hutton Institute Water Temperature Logger"));
  if (!SDCard)Serial.println(F("      ~~~~~~~~~~~PLEASE ENSURE YOU HAVE ENABLED THE SD CARD FOR THIS SENSOR IF REQURED~~~~~~~~~~~"));
  if (!sleepNow)Serial.println(F("    ~~~~~~~~~~~PLEASE ENSURE YOU HAVE ENABLED THE SLEEP FUNCTION FOR THIS SENSOR IF REQURED~~~~~~~~~~"));
  Serial.print(F("LOGGER INTERVAL IS SET TO:  ")); Serial.print((sleepTime * 4) / 60); Serial.print(F(" MINUTES")); Serial.print(F(" and ")); Serial.print(sleepTime * 4); Serial.println(F(" seconds"));
  pinMode(2, OUTPUT);  //these line allocate power to to each of the sensors
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  //pinMode(SDPower, OUTPUT);


/***************************************************************************/
  setRTC();  //Set the time on the RTC if required
  // The following line is used for debugging only.  Comment out for normal use
  //Scan();  //Run the 'I2CScan' tab
  /****************************************************
       The following lines will apply power to each sensor and check to see if it exists, then remove power
       to save battery life.  This sequence only runs once on power up.
  */
  wdt_reset();  //  If all is well then the watchdog timer will be reset;
  digitalWrite(8, HIGH);  //turn on sensor number 8 which is connected to I2C address 0x18
  delay(200);
  tcaselect(0);
  if (!tempsensor.begin(0x18)) {
    Serial.println(F("Couldn't find MCP9808 #1! (0X18)Check your connections and verify the address is correct.")); //Air
    //while (1);
  }
  digitalWrite(8, LOW);

  digitalWrite(7, HIGH);
  delay(200);
  tcaselect(1);
  if (!tempsensor.begin(0x19)) {
    Serial.println(F("Couldn't find MCP9808 #2! (0X19)Check your connections and verify the address is correct.")); //Top
    //while (1);
  }
  digitalWrite(7, LOW);

  digitalWrite(6, HIGH);
  delay(200);
  tcaselect(2);
  if (!tempsensor.begin(0x1A)) {
    Serial.println(F("Couldn't find MCP9808 #3! (0X1A)Check your connections and verify the address is correct."));  //1st
    //while (1);
  }
  digitalWrite(6, LOW);

  digitalWrite(4, HIGH);
  delay(200);
  tcaselect(3);
  if (!tempsensor.begin(0x1B)) {
    Serial.println(F("Couldn't find MCP9808 #4! (0X1B)Check your connections and verify the address is correct."));  //1st
    //while (1);
  }
  digitalWrite(4, LOW);

  digitalWrite(3, HIGH);
  delay(200);
  tcaselect(4);
  if (!tempsensor.begin(0x1C)) {
    Serial.println(F("Couldn't find MCP9808 #5! (0X1C)Check your connections and verify the address is correct."));  //1st
    //while (1);
  }
  digitalWrite(3, LOW);

  digitalWrite(2, HIGH);
  delay(200);
  tcaselect(5);
  if (!tempsensor.begin(0x1D)) {
    Serial.println(F("Couldn't find MCP9808 #6! (0X1D)Check your connections and verify the address is correct."));  //1st
    //while (1);
  }
  digitalWrite(2, LOW);

  //  If you have enabled the SD card (line48):
  if (SDCard) {
    //digitalWrite(SDPower, HIGH);
    delay(500);
    Serial.print(F("Initializing SD card..."));
    // see if the card is present and can be initialized:
    if (!SD.begin(chipSelect)) {
      Serial.println(F("Card failed, or not present"));
    }
    else {
      Serial.println(F("card initialized."));
    }
  }

  tempsensor.setResolution(3); // sets the resolution mode of all sensors, the modes are defined in the table below:
  // Serial.print(F("Sensor resolution in mode: "));
  // Serial.println (tempsensor.getResolution());
  // Mode Resolution SampleTime
  //  0    0.5°C       30 ms
  //  1    0.25°C      65 ms
  //  2    0.125°C     130 ms
  //  3    0.0625°C    250 ms

  /*****************************************************8
     The following procedure allocates a new file on the SD card every time powwer is applied
     This ensures that no data is overwritten
     You can create up to 1999 files
  */
  if (SDCard) {
    EEPROM.get(0, ID);
    Serial.print("Unit serial number is: "); Serial.println(ID);
    sprintf(filename, "JH%02d%03d", ID, num);
    //strcpy(filename, fileName);
    for (uint32_t i = 0; i < 2000; i++) {
      filename[4] = '0' + i / 1000;
      filename[5] = '0' + (i % 1000) / 100;
      filename[6] = '0' + (i % 100) / 10;
      filename[7] = '0' + i % 10;
      // create if does not exist, do not open existing, write, sync after write
      if (! SD.exists(filename)) {
        break;
      }
    }
    dataFile = SD.open(filename, FILE_WRITE);
    if (dataFile ) {
      dataFile.print("Unit Serial Number: "); dataFile.println(ID);
      Serial.print("Data will be written to "); Serial.println(filename);
    }
    else {
      Serial.print(F("Couldnt create ")); Serial.println(filename);
    }
    dataFile.close();
  }
  wdt_reset();  //  If all is well then the watchdog timer will be reset;

  DateTime now = rtc.now();
  sprintf(myDateString, "%4d-%02d-%02d" , now.year(), now.month(), now.day());
  sprintf(myTimeString, "%02d:%02d:%02d" , now.hour(), now.minute(), now.second());
  delay(100);
  Serial.print("Date is: "); Serial.println(myDateString);
  Serial.print("Time is: "); Serial.println(myTimeString);
}
/********************************END OF SETUP PROCEDURE**************************/

/*************** ALLOCATE ADDRESS FOR THE SENSORS********************************/
void tcaselect(uint8_t i) {
  if (i > 7) return;

  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}
/********************************************************************************/



void ReadSensors() {
  /***************************************************
     Each sensor is powered up in turn,
     a reading is taken and stored then powered down again
     before the next sensor is read
   ***************************************************/
  digitalWrite(8, HIGH);
  int x = 0;
  while (!tempsensor.begin(0x18)) {  //If the sensor didnt wake in time then it is read up to 100 times, this resolved a bug.
    delay(50);
    x++;
    if (x == 100); {
      break;
    }
    Serial.println(F("Couldn't find MCP9808 #1! (0X18)Check your connections and verify the address is correct."));  //1st
  }
  tempsensor.wake();   // wake up, ready to read!
  delay(100);
  tcaselect(0);
  c1 = tempsensor.readTempC();  // read the sensor value and store it in variable c1
  c1 = c1 - 0.6700;  //calibration factor (different for every sensor).
  //Serial.print(F("Sensor 1(X018): "));
  //Serial.print(c1, 4); Serial.print(F("\t"));
  delay(100);
  digitalWrite(8, LOW);
  /***********************************/

  digitalWrite(7, HIGH);
  while (!tempsensor.begin(0x19)) {
    delay(50);
    x++;
    if (x == 100); {
      break;
    }
    Serial.println(F("Couldn't find MCP9808 #2! (0X19)Check your connections and verify the address is correct."));  //1st
  }
  tempsensor.wake();   // wake up, ready to read!
  delay(100);
  tcaselect(1);
  c2 = tempsensor.readTempC();
  c2 = c2 - 0.3400;
  //Serial.print(F("Sensor 2(X019): "));
  // Serial.print(c2, 4); Serial.print(F("\t"));
  delay(100);
  digitalWrite(7, LOW);
  /**************************************/

  digitalWrite(6, HIGH);
  while (!tempsensor.begin(0x1A)) {
    delay(50);
    x++;
    if (x == 100); {
      break;
    }
    Serial.println(F("Couldn't find MCP9808 #3! (0X1A)Check your connections and verify the address is correct."));  //1st
  }
  tempsensor.wake();   // wake up, ready to read!
  delay(100);
  tcaselect(3);
  c3 = tempsensor.readTempC();
  c3 = c3 - 0.2981;
  //Serial.print(F("Sensor 3(X01A): "));
  //Serial.print(c3, 4); Serial.print(F("\t"));
  delay(100);
  digitalWrite(6, LOW);
  /**************************************/

  digitalWrite(4, HIGH);
  while (!tempsensor.begin(0x1B)) {
    delay(50);
    x++;
    if (x == 100); {
      break;
    }
    Serial.println(F("Couldn't find MCP9808 #4! (0X1B)Check your connections and verify the address is correct."));  //1st
  }
  tempsensor.wake();   // wake up, ready to read!
  delay(100);
  tcaselect(4);
  c4 = tempsensor.readTempC();
  c4 = c4 - 0.3391;
  //Serial.print(F("Sensor 4(X01B): "));
  //Serial.print(c4, 4); Serial.print(F("\t"));
  delay(100);
  digitalWrite(4, LOW);
  /**********************************/

  digitalWrite(3, HIGH);
  while (!tempsensor.begin(0x1C)) {
    delay(50);
    x++;
    if (x == 100); {
      break;
    }
    Serial.println(F("Couldn't find MCP9808 #5! (0X1C)Check your connections and verify the address is correct."));  //1st
  }
  tempsensor.wake();   // wake up, ready to read!
  delay(100);
  tcaselect(5);
  c5 = tempsensor.readTempC();
  c5 = c5 - 0.3391;
  //Serial.print(F("Sensor 5(X01C): "));
  // Serial.print(c5, 4); Serial.print(F("\t"));
  delay(100);
  digitalWrite(3, LOW);
  /********************************/

  digitalWrite(2, HIGH);
  while (!tempsensor.begin(0x1D)) {
    delay(50);
    x++;
    if (x == 100); {
      break;
    }
    Serial.println(F("Couldn't find MCP9808 #6! (0X18D)Check your connections and verify the address is correct."));  //1st
  }
  tempsensor.wake();   // wake up, ready to read!
  delay(100);
  tcaselect(6);
  c6 = tempsensor.readTempC();
  c6 = c6 - 0.4378;
  //Serial.print(F("Sensor 6(X01D): "));
  //Serial.println(c6, 4); //Serial.println(F("\t"));
  delay(100);
  digitalWrite(2, LOW);
}

void Scan() {
  /********************************************************
     THE FOLLOWING PROCEDURE WILL POWER UP ALL THE SENSORS
     AND THEN SCAN THE I2C BUS TO DETERMINE WHICH SENSOR
     EXISTS AND WHAT ITS ASSOCIATED ADDERESS IS
     (This is primarily used for debugging purposes)
   ********************************************************/

  Wire.begin();  //Starts the I2C
  byte error, address;
  int nDevices;
  for (int powerPin = 2; powerPin < 9; powerPin++)digitalWrite(powerPin, HIGH); //Applies power to all sensors
  delay(500); //500ms delay to allow sensors to settle before taking readings
  Serial.println(F("Scanning..."));

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print(F("I2C device found at address 0x"));
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("");

      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print(F("Unknown error at address 0x"));
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println(F("No I2C devices found\n"));
  else
    Serial.println(F("done\n"));
}

// Make sure the sensor is found, you can also pass in a different i2c
// address with tempsensor.begin(0x19) for example. Can be left in blank for default address use.
// The following table shows all addresses possible for this sensor, you can connect multiple sensors
// to the same i2c bus, just configure each sensor with a different address and define multiple objects for that ~(max 8)
//  A2 A1 A0 address
//  0  0  0   0x18  this is the default address
//  0  0  1   0x19
//  0  1  0   0x1A
//  0  1  1   0x1B
//  1  0  0   0x1C
//  1  0  1   0x1D
//  1  1  0   0x1E
//  1  1  1   0x1F


/********************************************************************************/

void readRTC() {
  DateTime now = rtc.now();
  if (now.day() != oldDay) {
    //Serial.println("It's a new day!!");
    sprintf(myDateString, "%4d-%02d-%02d" , now.year(), now.month(), now.day());
    //digitalWrite(SDPower, HIGH);
    delay(500);
    dataFile = SD.open(filename, FILE_WRITE);
    dataFile.println(myDateString);
    dataFile.close();
    oldDay = now.day();
  }
  sprintf(myTimeString, "%02d:%02d:%02d" , now.hour(), now.minute(), now.second());
  delay(100);
}

void setRTC()
{
  if (! rtc.begin()) {
    Serial.println(F("Couldn't find RTC"));
    while (1);
  }
  DateTime now = rtc.now();
  if (rtc.lostPower()) {
    Serial.println(F("RTC lost power, lets set the time!"));
    Serial.print('\n');
    // following line sets the RTC to the date & time this sketch was compiled
    char compileDate[] PROGMEM = __DATE__; char compileTime[] PROGMEM = __TIME__;
    Serial.print("Compile time =  "); Serial.println(compileDate); Serial.println(compileTime);
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
}

/********************************************************************************/
void GoToSleep()
{
  while (sleepCount < sleepTime) {
    sleepCount++;
    wdt_reset();  //  If all is well then the watchdog timer will be reset;
    LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);  //Change this to suit program.  The following table shows all possible values
  }
  sleepCount = 0;
}

/*
  SLEEP_15MS,
  SLEEP_30MS,
  SLEEP_60MS,
  SLEEP_120MS,
  SLEEP_250MS,
  SLEEP_500MS,
  SLEEP_1S,
  SLEEP_2S,
  SLEEP_4S,
  SLEEP_8S,
  SLEEP_FOREVER
*/
/********************************************************************************/

void loop() {
  wdt_reset();  //  If all is well then the watchdog timer will be reset;
  readRTC();
  ReadSensors();  //Run 'ReadSensors' tab
  /****************THE FOLLOWING LINES WRITE THE RECORDED DATA TO THE SD CARD******/
  if (SDCard) {
    //digitalWrite(SDPower, HIGH);
    delay(500);
    dataFile = SD.open(filename, FILE_WRITE);
    if ( ! dataFile ) {
      Serial.print(F("Couldnt create ")); Serial.println(filename);
    }
    Serial.print(F("Writing to ")); Serial.println(filename);
    if (dataFile) {
      dataFile.print(myTimeString); dataFile.print("\t");
      dataFile.print(c1, 4); dataFile.print("\t");
      dataFile.print(c2, 4); dataFile.print("\t");
      dataFile.print(c3, 4); dataFile.print("\t");
      dataFile.print(c4, 4); dataFile.print("\t");
      dataFile.print(c5, 4); dataFile.print("\t");
      dataFile.println(c6, 4);
      dataFile.close();
    }

    // if the file isn't open, pop up an error:
    else {
      Serial.print(F("error opening ")); Serial.print(filename); Serial.println(F(".  The data is not being saved to the SD card"));
    }
    //digitalWrite(SDPower, LOW); // turn the sd card off
  }
  Serial.print(myTimeString); Serial.print("\t");
  Serial.print(c1, 4); Serial.print("\t");
  Serial.print(c2, 4); Serial.print("\t");
  Serial.print(c3, 4); Serial.print("\t");
  Serial.print(c4, 4); Serial.print("\t");
  Serial.print(c5, 4); Serial.print("\t");
  Serial.println(c6, 4);
  delay(100);
  Serial.flush();

  if (sleepNow) GoToSleep(); //If you have enabled the sleep function (line 47) then run the 'Sleep' tab
}
