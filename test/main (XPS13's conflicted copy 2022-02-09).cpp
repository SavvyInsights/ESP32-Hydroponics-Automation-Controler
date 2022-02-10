#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <SimpleTimer.h>
#include <esp_adc_cal.h>
#include <RTClib.h>
#include <SPI.h>
#include <millisDelay.h> // part of the SafeString Library

// ---- Classes --------------
RTC_DS3231 rtc; 
SimpleTimer timer;

// ----- GLOBAL VARIABLES --------

DateTime uptime;
unsigned long unix_uptime;
unsigned long uptime_seconds;
DateTime now;
unsigned long unix_now;

// ----- LCD SETTINGS --------
int lcdColumns = 20; // LCD Columns
int lcdRows = 4; // LCD Rows
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// ----- DEFAULT SETTINGS ------
bool temp_in_c = true; // Tempurature defaults to C
bool twelve_hour_clock = true; // Clock format

float pump_init_delay = 1; // Minutes - Initial time before starting the pump on startup
float pump_on_time = 2; // Minutes - how long the pump stays on for
float pump_off_time = 10; // Minutes -  how long the pump stays off for

float ph_set_level = 6.2; // Desired pH level
int ph_delay_minutes = 60;// miniumum period allowed between doses in minutes
float ph_dose_seconds = 3; // Time Dosing pump runs per dose in seconds;
float ph_tolerance = 0.2; // how much can ph go from target before adjusting

float ppm_set_level = 400; // Desired nutrient levle
int ppm_delay_seconds = 60; //period btween readings/doses in minutes
float ppm_dose_seconds = 3; // Time Dosing pump runs per dose

// ----- SET PINS ------------------
OneWire oneWire(16);// Tempurature pin - Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
const int tds_pin = 34; // TDS sensor pin - try 13 if 26 doesnt work
const int ph_pin = 35; // pH sensor pin

const int pump_pin = 32; // pump relay
const int heater_pin = 33; // heater relay

const int ph_up_pin = 25; //pH up dosing pump
const int ph_down_pin = 26; // pH down dosing pump

const int nutrient_a_pin = 19; // nutrient part A dosing pump
const int nutrient_b_pin = 18; // nutrient part B dosing pump

// *************** CALIBRATION FUNCTION ******************
// To calibrate actual votage read at pin to the esp32 reading
uint32_t readADC_Cal(int ADC_Raw)
{
  esp_adc_cal_characteristics_t adc_chars;
  
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  return(esp_adc_cal_raw_to_voltage(ADC_Raw, &adc_chars));
}

// =======================================================
// ========== RTC Functions ==============================
// =======================================================
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
bool display_seconds = false;

int year;
int month;
int day;
int dayofweek;
int hour; // 24 hour clock
int twelvehour; // hour in 12 hour format
bool ispm; // yes = PM
String am_pm;
int minute;
int second;

int up_year;
int up_month;
int up_day;
int up_hour;
int up_12hour;
bool up_ispm;
int up_minute;
int up_second;

void initalize_rtc()
  {
    if (! rtc.begin()) 
      {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        while (1) delay(10);
      }
    if (rtc.lostPower()) 
      {
        Serial.println("RTC lost power, let's set the time!");
        // When time needs to be set on a new device, or after a power loss, the
        // following line sets the RTC to the date & time this sketch was compiled
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        // This line sets the RTC with an explicit date & time, for example to set
        // January 21, 2014 at 3am you would call:
        // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
      }  
  }

void setUptime()
  {
    DateTime uptime = rtc.now();
    up_year = uptime.year();
    up_month = uptime.month();
    up_day = uptime.day();
    up_hour = uptime.hour();
    up_12hour = uptime.twelveHour();
    up_ispm = uptime.isPM();
    up_minute = uptime.minute();
    up_second = uptime.second();
    unix_uptime = uptime.unixtime();
  }

void setTimeVariables()
  {
    DateTime now = rtc.now();
    year = now.year();
    month = now.month();
    day = now.day();
    dayofweek = now.dayOfTheWeek();
    hour = now.hour();
    twelvehour = now.twelveHour();
    ispm =now.isPM();
    minute = now.minute();
    second = now.second();
  }

void printDigits(int digit) // To alwasy display time in 2 digits
  {
      lcd.print(":");
      if(digit < 10)
        {
          lcd.print('0');
        }
        lcd.print(digit);
  }
void displayTime()  // Displays time in proper format
  {
    if (twelve_hour_clock == true)
      {
        hour = twelvehour;
        if (ispm == true) am_pm = "PM";
        else am_pm = "AM";
      }
    lcd.print(hour);
    printDigits(minute);
    if (twelve_hour_clock == true) lcd.print(am_pm);
    if (display_seconds == true) printDigits(second);
  }

void printDate() 
  {
    DateTime now = rtc.now();
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    delay(2000);
    Serial.println();

    //Serial.print(" since midnight 1/1/1970 = ");
    //Serial.print(now.unixtime());
    
    // calculate a date which is 7 days, 12 hours, 30 minutes, 6 seconds into the future
    /*DateTime future (now + TimeSpan(7,12,30,6));

      Serial.print(" now + 7d + 12h + 30m + 6s: ");
      Serial.print(future.year(), DEC);
      Serial.print('/');
    */
  }

// =======================================================
// ======= TEMPURATURE SENSOR DS18B20 ====================
// =======================================================

float tempC; // tempurature in Celsius
float tempF; // tempurature in Fahrenheit

#define TEMPERATURE_PRECISION 10
DallasTemperature waterTempSensor(&oneWire); // Pass our oneWire reference to Dallas Temperature.

void getWaterTemp()
  {
    waterTempSensor.requestTemperatures();    // send the command to get temperatures
    tempC = waterTempSensor.getTempCByIndex(0);  // read temperature in °C
    tempF = tempC * 9 / 5 + 32; // convert °C to °F
    if (tempC == DEVICE_DISCONNECTED_C) // Something is wrong, so return an error
      {
        Serial.println("Houston, we have a problem");
        tempC = -100; 
        tempF = tempC;
      }
  }

// =======================================================
// ======= PH SENSOR =====================================
// =======================================================

float ph_value; // actual pH value to display on screen
float ph_calibration_adjustment = 0; // adjust this to calibrate
//float calibration_value_ph = 21.34 + ph_calibration_adjustment;

void getPH()
  {
    float voltage_input = 3.3; // voltage can be 5 or 3.3
    float adc_resolution = 4095;
    unsigned long int average_reading ;
    int buffer_array_ph[10],temp;
    float calculated_voltage; // voltage calculated from reading
    float calibrated_voltage; // calculatd voltage adjusted using fuction

    for(int i=0;i<10;i++) // take 10 readings to get average
      { 
        buffer_array_ph[i]=analogRead(ph_pin);
        delay(30);
      }
    for(int i=0;i<9;i++)
      {
        for(int j=i+1;j<10;j++)
          {
            if(buffer_array_ph[i]>buffer_array_ph[j])
              {
                temp=buffer_array_ph[i];
                buffer_array_ph[i]=buffer_array_ph[j];
                buffer_array_ph[j]=temp;
              }
          }
      }
    average_reading =0;
    for(int i=2;i<8;i++)
      {
        average_reading  += buffer_array_ph[i];
      }
    average_reading  = average_reading  / 6;
    calculated_voltage = average_reading  * voltage_input / adc_resolution;
    calibrated_voltage = (readADC_Cal(average_reading ))/1000;
    ph_value = voltage_input * calibrated_voltage;
    //ph_value = (-5.70 * calibrated_voltage) + calibration_value_ph; // Calculate the actual pH
  
    /*
      Serial.print("    average_reading  = ");
      Serial.print(average_reading );
      Serial.print("      calculated_voltage = ");
      Serial.print(calculated_voltage);
      Serial.print("     calibtraded_voltage = ");
      Serial.print(calibrated_voltage);
      Serial.print("     ph_value = ");
      Serial.println(ph_value);
      delay(0); // pause between serial monitor output - can be set to zero after testing
    */
  }

// =======================================================
// ======= PPM OCEAN TDS METER SENSOR ====================
// =======================================================

int tds_value = 0;
const int sample_count = 30;    // sum of sample point
int analogBuffer[sample_count]; // store the analog value in the array, read from ADC
int analogBufferTemp[sample_count];
int analogBufferIndex = 0,copyIndex = 0;

// Function to get median
int getMedianNum(int bArray[], int iFilterLen) 
  {
    int bTab[iFilterLen];
    for (byte i = 0; i<iFilterLen; i++)
      bTab[i] = bArray[i];
    int i, j, bTemp;
    for (j = 0; j < iFilterLen - 1; j++) 
      {
        for (i = 0; i < iFilterLen - j - 1; i++) 
          {
            if (bTab[i] > bTab[i + 1]) 
              {
                bTemp = bTab[i];
                bTab[i] = bTab[i + 1];
                bTab[i + 1] = bTemp;
              }
          }
      }
      if ((iFilterLen & 1) > 0)
        bTemp = bTab[(iFilterLen - 1) / 2];
      else
        bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
    return bTemp;
  }

void getTDSReading()
  {
    const float voltage_input = 3.3;  // analog reference voltage(Volt) of the ADC
    const float adc_resolution = 4095;
    
    float average_voltage = 0;
    float temperature = 25;
   
    // get current tempurature
    if (tempC == -100) temperature = 25;
    else temperature = tempC;
    static unsigned long analogSampleTimepoint = millis();
    if(millis()-analogSampleTimepoint > 40U)     //every 40 milliseconds,read the analog value from the ADC
      {
        analogSampleTimepoint = millis();
        analogBuffer[analogBufferIndex] = analogRead(tds_pin);    //read the analog value and store into the buffer

        analogBufferIndex++;
        if(analogBufferIndex == sample_count) 
            analogBufferIndex = 0;
      }   
    static unsigned long printTimepoint = millis();
    if(millis()-printTimepoint > 800U)
      {
          printTimepoint = millis();
          for(copyIndex=0;copyIndex<sample_count;copyIndex++)
            analogBufferTemp[copyIndex]= analogBuffer[copyIndex];

          average_voltage = getMedianNum(analogBuffer,sample_count) * voltage_input / adc_resolution;// ESP32 - 1-4096 - Ardurino mega 0-1023read the analog value more stable by the median filtering algorithm, and convert to voltage value
          float current_read = analogRead(tds_pin);
          float current_voltage = current_read * voltage_input / adc_resolution;
          float median_read = getMedianNum(analogBuffer,sample_count);
          //float calibrated_voltage = readADC_Cal(average_voltage);
          //calibrated_voltage = calibrated_voltage/1000;
          
          float compensationCoefficient=1.0+0.02*(temperature-25.0);    //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
          float compensationVolatge=average_voltage/compensationCoefficient;  //temperature compensation
          tds_value=(133.42*compensationVolatge*compensationVolatge*compensationVolatge - 255.86*compensationVolatge*compensationVolatge + 857.39*compensationVolatge)*0.5; //convert voltage value to tds value
        /*
          Serial.print("   analogRead: ");
          Serial.print(analogRead(tds_pin));
          Serial.print("   median_read: ");
          Serial.print(median_read);
          Serial.print("   voltage : ");
          Serial.print(current_voltage);
          Serial.print("   average_voltage : ");
          Serial.print(average_voltage,2);
          //Serial.print("   calibrated_voltage: ");
          //Serial.print(calibrated_voltage,2);
          Serial.print("   Temp: ");
          Serial.print(temperature);
          Serial.print("   compensationVoltage: ");
          Serial.println(compensationVolatge);
          Serial.print("   TtdsValue: ");
          Serial.println(tds_value, 0);
          delay(0);
      */
      }
  }

// =================================================
// ========== PH DOSING PUMPS =========================
// =================================================
int ph_dose_pin;

millisDelay phDoseTimer; // the dosing amount time
millisDelay phDoseDelay; // the delay between doses - don't allow another dose before this

void phDose(int motor_pin) // turns on the approiate ph dosing pump
  {
    if (phDoseDelay.isRunning() == false)
      {
        digitalWrite(motor_pin, HIGH); // turn on dosing pump
        phDoseTimer.start(ph_dose_seconds*1000); // start the pump
        ph_dose_pin = motor_pin;
        phDoseDelay.start(ph_delay_minutes * 60 * 1000); // start delay before next dose is allowed
      }
    else
    {
      Serial.print("Dose delay timer is still on");
    }
  }

void phBalanceCheck() //this is to be called from pump turning on function
  {
    if (ph_value < ph_set_level - ph_tolerance) phDose(ph_up_pin); // ph is low start ph up pump
    if (ph_value > ph_set_level + ph_tolerance) phDose(ph_down_pin); // ph is high turn on lowering pump
  }
void phDosingTimer()
  {
    if (phDoseTimer.justFinished()) digitalWrite(ph_dose_pin, LOW);  // dosing is done, turn off, and start delay before next dose is allowed
  }

void phTest()
  {
    digitalWrite(ph_up_pin, HIGH);
    delay(3000);
    digitalWrite(ph_up_pin, LOW);
    delay(5000);
    digitalWrite(ph_down_pin, HIGH);
    delay(3000);
    digitalWrite(ph_down_pin, LOW);
  }

// ==================================================
// ===========  PUMP CONTROL ========================
// ==================================================
int pump_seconds; // current seconds left
int pump_minutes;
millisDelay pumpOnTimer;
millisDelay pumpOffTimer;

void setPumpSeconds() // Change seconds into minutes and seconds
  { 
    pump_minutes = pump_seconds / 60;
    if (pump_seconds < 60) pump_minutes = 0;
    else pump_seconds = pump_seconds - (pump_minutes * 60);
  }

void pumpTimer()
  {
    if (digitalRead(pump_pin) == 1 ) // pump is off, check timer
      {
        pump_seconds = pumpOffTimer.remaining() / 1000;
        if (pumpOffTimer.justFinished()) // off delay is done, start pump
          {
            digitalWrite(pump_pin, LOW); // turn on pump
            pumpOnTimer.start(pump_on_time * 60 * 1000);
            pump_seconds = pumpOnTimer.remaining() /1000;
            phBalanceCheck(); // check to see if ph dose is needed
          }
      }
    else // pump is on, check timing
      {
        pump_seconds = pumpOnTimer.remaining();
        if (pumpOnTimer.justFinished()) // on time is done turn off
          {
            digitalWrite(pump_pin, HIGH); // turn off pump
            pumpOffTimer.start(pump_off_time * 60 * 1000);
            pump_seconds = pumpOnTimer.remaining() /1000;
          }
      }
    setPumpSeconds();
  }

void displayPumpStatus()
  {
    if (digitalRead(pump_pin) == 0) lcd.print("ON ");
      else lcd.print("OFF");
    lcd.setCursor(9,3);
    lcd.print(pump_minutes);
    printDigits(pump_seconds); // use time fuction to print 2 digit seconds
  }

void pumpTest()
  {
    // change pump variables for demonstration or test
    float pump_init_delay = .5; // Minutes - Initial time before starting the pump on startup
    float pump_on_time = .5; // Minutes - how long the pump stays on for
    float pump_off_time = 1.5;

    pumpTimer();

    // Print pump data
    Serial.print(" Pump pin status : ");
    Serial.print(digitalRead(pump_pin));
    Serial.print("    pump minutes : ");
    Serial.print(pump_minutes);
    Serial.print(" seconds : ");
    Serial.println(pump_seconds);
    //delay(1000);
  }

// =================================================
// ========== LCD DISPLAY ==========================
// =================================================

void displaySplashscreen()// Display the splash screen
  {
    lcd.init(); // Initialize LCD
    lcd.backlight(); // Turn on the LCD backlight
    lcd.setCursor(1,0);
    lcd.print("CONCIERGE GROWERS");
    lcd.setCursor(5,1);
    lcd.print("Eat Good");
    lcd.setCursor(4,2);
    lcd.print("Feel Good");
    lcd.setCursor(4,3);
    lcd.print("Look Good");
    delay(1000);
    lcd.clear();
  }
void displayMainscreenstatic()// Display the parts that don't change
  {
    lcd.setCursor(0,0);
    lcd.print("Temp:");
    lcd.setCursor(1,1);
    lcd.print("TDS:");
    lcd.setCursor(2,2);
    lcd.print("PH:");
    lcd.setCursor(0,3);
    lcd.print("Pump:");
  }

void displayMainscreenData() // Display the data that changes on main screen
  {
    // Display tempurature
      lcd.setCursor(5,0);
      //getWaterTemp(); // will return -100 if there is an issue -  not needed variable tempC is already set
      if (tempC == -100) lcd.print("(error)   ");
      else {if (temp_in_c == true) {lcd.print(tempC);lcd.print((char)223);lcd.print("C"); }
            else {lcd.print(tempF); lcd.print((char)223); lcd.print("F");}}

    // Display TDS reading
      lcd.setCursor(5,1);
      if (tds_value > 1000) lcd.print("(error)    ");
      else {lcd.print(tds_value);lcd.print("(PPM)    ");}
    
    // ---- Display pH Reading
      lcd.setCursor(5,2);lcd.print(ph_value); 

    //----Display  Pump status
      lcd.setCursor(5,3);
      displayPumpStatus();

    // ---- Display time
      lcd.setCursor(13,0);
      displayTime();
  }

// ==================================================
// ===========  MAIN SETUP ==========================
// ==================================================
void setup(void)
  {
    Serial.begin(115200);// start serial port 115200
    Serial.println("Starting Hydroponics Automation Controler");
    timer.run(); // Initiates SimpleTimer

    // Initialize Sensors
    waterTempSensor.begin(); // initalize water temp sensor
    pinMode(tds_pin,INPUT); // setup TDS sensor pin
    //ph.begin(); // starts the ph api 
  
    //Initalize RTC
    initalize_rtc();
    setUptime();
    setTimeVariables();

    Serial.print("Uptime : ");
    Serial.print(up_year);
    Serial.print(up_month);
    Serial.print(up_12hour);
    Serial.print(up_minute);
      
    //Initalize Pump
    pinMode(pump_pin, OUTPUT);
    digitalWrite(pump_pin,HIGH);
    pumpOffTimer.start(pump_init_delay*60*1000); // start initilization period
    pump_seconds = pumpOffTimer.remaining() *1000;
    
   

    Serial.print("pump start timer : ");
    Serial.println(pump_seconds);

    // Initalize dosing pumps
    pinMode(ph_up_pin, OUTPUT);
    digitalWrite(ph_up_pin, LOW);
    pinMode(ph_down_pin, OUTPUT);
    digitalWrite(ph_down_pin, LOW);

    pinMode(nutrient_a_pin, OUTPUT);
    digitalWrite(nutrient_a_pin, LOW);
    pinMode(nutrient_b_pin, OUTPUT);
    digitalWrite(nutrient_b_pin, LOW);

    // Prepare screen
    displaySplashscreen();
    displayMainscreenstatic();
  }

// ====================================================
// ===========  MAIN LOOP =============================
// ====================================================

void loop(void)
{
  // Get readings
  getWaterTemp(); // sets tempC and tempF
  getTDSReading(); // sets tds_value
  getPH();

  // Time functions
  setTimeVariables();

  // Pump Timer
  //pumpTimer(); // uncomment this to turn on functioning pump timer
  pumpTest();
  
  // pH Balance
  // phTest(); //used to test ph dosing motors
  phDosingTimer(); // turn off dosing timer when finsihed
    

  //Nutrient Balance
  //nutrientDosing();

  displayMainscreenData();
}

// ----------------- END MAIN LOOP ------------------------