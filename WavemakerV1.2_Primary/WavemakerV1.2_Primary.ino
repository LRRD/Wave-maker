// Wavemaker v1.2
// Primary MCU
// Detect user knob rotation and button presses
// Manage LCD
// Send updates to secondary MCU controlling stepper motor and sensors
    
#include <Arduino.h>      //For Nick Gammon's I2C writeAnything
#include <Wire.h>         //For I2C communications

// Global Variables & Magic Numbers
const byte secondary_address = 8;       //I2C device address
const byte primary_address = 9;         //I2C primary device address
const byte encoderswitch = 7;           //Encoder pin reference
const byte channelB = 6;                //Encoder pin reference
const byte channelA = 4;                //Encoder pin reference

bool starter = false;               //Wave action while starter is true
bool laststarter = false;           //Manage updates to secondary when starter changes
int power = 100;                    //Added delay per microstep, in microseconds, essentially paddle speed
int lastpower = 100;                //Manage updates to secondary when power changes
int stepRange = 0;                  //# of steps in half of paddle motion, between 0 and extremum
int lastStepRange = 0;              //Manage updates to secondary when step range changes 
float delaybetweenwaves = 0.4;      //Delay between waves in ms
float lastdbw = 0.0;                //Manage updates to secondary when dbw changes
float totalwavetime = 0.0;          //Total elapsed time it took to send one wave, received from secondary
uint32_t totalWaves = 0;            //Total number of waves sent since last power cycle, received from secondary
uint32_t updateTime = 0;            //Records millis() when necessary to refresh periodically
uint16_t syncRate = 500;            //Refresh rate in ms, works with updateTime
bool increasing = false;            //Reports true when knob rotated CW
bool decreasing = false;            //Reports true when knob rotated CCW
bool pressed = false;               //Reports true when button is released
int menu = 0;                       //Menu switch case number
bool tsunami = false;               //Tell secondary we're currently on tsunami menu     
bool lastencoderstate = false;      //Used to detect falling signal edge of button press
float wavespermin = 0.0;            //Calculated waves being sent per minute at current settings
float lastwpm = 0.0;                //Manage updates to secondary when wpm changes
uint32_t lasttime = 0;              //Records millis() when necessary to refresh periodically
bool indicator = false;             //Used to flip flop between "/" and "|" to create moving paddle animation
bool editing = false;               //Used to toggle on/off editing mode in certain menus              
float pwpcnt = 0.0;                 //Percentage of power represented as a percent
float travelDeg = 85.0;             //Total degrees of travel in paddle motion
float travelDZ = 16.47;             //Degrees of travel in paddle deadzone, all considered step count = 0, experimentall tested and averaged
bool Tsend = false;                 //Manage updates to secondary to send tsunami wave
bool startup = true;                //Ignores first button press after startup to prevent false start
bool firstClick = true;             //Ignores first button rotation after startup to prevent false menu change
bool lastTsunami = false;           //Manage updates to secondary when tsunami changes  
int scroll = 0;                     //Scrolling data printout switch case number
uint32_t scrollTimer = 0;           //Records millis() when necessary to update scrolling data
uint16_t scrollInterval = 3000;     //Refresh rate in ms, works with scrollTimer

static uint8_t cw_gray_codes[4] = { 2, 0, 3, 1 };   //Gray code of CW encoder rotation
static uint8_t ccw_gray_codes[4] = { 1, 3, 0, 2 };  //Gray code of CCW encoder rotation
static uint8_t previous_gray_code = 0;              //Used to track gray code rotation

//I2C_anything, used to simply send & receive all data types over I2C, by Nick Gammon
template <typename T> int I2C_writeAnything (const T& value)
{
  const byte * p = (const byte*) &value;
  unsigned int i;
  for (i = 0; i < sizeof value; i++)
    Wire.write(*p++);
  return i;
}  // end of I2C_writeAnything

template <typename T> int I2C_readAnything(T& value)
{
  byte * p = (byte*) &value;
  unsigned int i;
  for (i = 0; i < sizeof value; i++)
    *p++ = Wire.read();
  return i;
}  // end of I2C_readAnything

//Update secondary MCU with the listed variables, must match receiveEvent on secondary
void updateSecondary()
{
  Wire.beginTransmission(secondary_address);    //Start data transfer
  I2C_writeAnything(starter);                   //Start/stop paddle
  I2C_writeAnything(stepRange);                 //Step range for paddle movement
  I2C_writeAnything(power);                     //Power setting for paddle movement
  I2C_writeAnything(delaybetweenwaves);         //Delay to wait between each wave
  I2C_writeAnything(tsunami);                   //In tsunami mode
  I2C_writeAnything(Tsend);                     //Send tsunami wave
  Wire.endTransmission();                       //End data transfer
  if (Tsend)                                    //Disable tsunami send bool after updating secondary
  {
    Tsend = false;
  }
}

//Function called when secondary updates primary over I2C, must match updatePrimary on secondary
void receiveEvent()
{
  I2C_readAnything(totalwavetime);    //Elapsed time of each wave
  I2C_readAnything(totalWaves);       //Total waves sent
  constraints();                      //Constrain variables to desired range
}

// Look for encoder rotation and button presses
void check_encoder()
{
  bool encoderstate = digitalRead(encoderswitch);             //Used to track falling edge of encoder button when pressed
  if ((encoderstate == HIGH) && (lastencoderstate == LOW))    //Switch is released
  {
    if (startup)
    {
      startup = false;
    }
    else
    {
      pressed = true;
    }
  }
  lastencoderstate = encoderstate;                            //Used to track falling edge of encoder button when pressed             
  int gray_code = ((digitalRead(channelA) == HIGH) << 1) | (digitalRead(channelB) == HIGH);   //Read gray code state of encoder
  if (gray_code != previous_gray_code)        //Encoder rotated
  {
    if (firstClick)                           //Ignore first click only on startup
    {
      firstClick = false; 
    }
    else
    {
      if (gray_code == cw_gray_codes[previous_gray_code])       //Rotated CW
      {
        increasing = true;
      }
      else if (gray_code == ccw_gray_codes[previous_gray_code]) //Rotated CCW
      {
        decreasing = true;
      }
    }
    previous_gray_code = gray_code;                             //Track gray code state
  }
}

//Constrain variables within acceptable range, should match secondary MCU
void constraints()
{
  delaybetweenwaves = constrain(delaybetweenwaves, 0.2, 30.0);
  power = constrain(power, 15, 250);
  stepRange = constrain(stepRange, -1000, -450);
  totalWaves = constrain(totalWaves, 1, 99999);
}

// turn on display
void displayOn() {
  Serial.write(0xFE);
  Serial.write(0x41);
}

// move the cursor to the home position on line 2
void cursorLine2() {
  Serial.write(0xFE);
  Serial.write(0x45);
  Serial.write(0x40); //Hex code for row 2, column 1
}

// move the cursor to the home position on line 2
void cursorTopRight() {
  Serial.write(0xFE);
  Serial.write(0x45);
  Serial.write(0x0F); //Hex code for row 1, column 16
}

// move the cursor to the home position on line 2
void cursorBottomRight() {
  Serial.write(0xFE);
  Serial.write(0x45);
  Serial.write(0x4F); //Hex code for row 2, column 16
}

// move the cursor to the home position
void cursorHome() {
  Serial.write(0xFE);
  Serial.write(0x46);
}

// clear the LCD
void clearLCD() {
  Serial.write(0xFE);
  Serial.write(0x51);
}

// move cursor left
void cursorLeft(int left) {
  for (int i = 0; i < left; i++)
  {
    Serial.write(0xFE);
    Serial.write(0x49);
  }
}

// move cursor right
void cursorRight(int right) {
  for (int i = 0; i < right; i++)
  {
    Serial.write(0xFE);
    Serial.write(0x4A);
  }
}

// set LCD contrast
void setContrast(int contrast) {
  Serial.write(0xFE);
  Serial.write(0x52);
  Serial.write(contrast); //Must be between 1 and 50
}

// turn on backlight
void backlightBrightness(int brightness) {
  Serial.write(0xFE);
  Serial.write(0x53);
  Serial.write(brightness); //Must be between 1 and 8
}

void definecustom()   //Degree symbol, custom symbol #0
{
  Serial.write(0xFE);       //Talk to LCD display
  Serial.write(0x54);       //Define custom character
  Serial.write(0);          //Custom character address

  Serial.write(0x1C);
  Serial.write(0x14);
  Serial.write(0x1C);
  Serial.write(0x00);
  Serial.write(0x00);
  Serial.write(0x00);
  Serial.write(0x00);
  Serial.write(0x00);
}

//Update secondary MCU, only when pertinent variables change, track those variable changes, and track time since the last update
void periodicUpdate()
{
  if ((power != lastpower) || (starter != laststarter) || (delaybetweenwaves != lastdbw) || (stepRange != lastStepRange) || (tsunami != lastTsunami) || (Tsend))
  {
    lastpower = power;
    laststarter = starter;
    lastdbw = delaybetweenwaves;
    lastStepRange = stepRange;
    lastTsunami = tsunami;
    updateSecondary();
    updateTime = millis();
  }
}

//Reset encoder button and rotation bools
void disableBool()
{
  pressed = false;
  increasing = false;
  decreasing = false;
}

// Menu Handling, All functionality of program lives within the menuselector
void menuselect()
{
  switch (menu)
  {
    //Home screen, some code runs once upon entry, some continues to run in while loop, only place you can start/stop wave action
    case 0:
      //Entry routine
      clearLCD();
      cursorHome();
      //Waves per minute display
      Serial.print(F("0.0"));
      Serial.print(F(" WPM"));
      cursorLine2();
      //Scrolling data display
      scrollTimer = millis();
      if (scroll == 0)
      {
        Serial.print(delaybetweenwaves, 1);
        Serial.print(F("s Delay"));
      }
      else if (scroll == 1)
      {
        pwpcnt = map(power, 15, 250, 100, 1);
        Serial.print(pwpcnt, 0);
        Serial.print(F("% Speed"));
      }
      else if (scroll == 2)
      {
        Serial.print(travelDeg, 0);
        Serial.write(0);
        Serial.print(F(" Range"));
      }
      cursorTopRight(); //Set cursor to desired pos
      cursorLeft(5);
      //Total waves sent display
      totalWaves = constrain(totalWaves, 1, 99999);
      Serial.print(totalWaves - 1); //-1 Because wave increments when the paddle draws back, not goes forward
      Serial.print(F("W"));
      disableBool();
      //Loop routine
      while (menu == 0)
      {
        check_encoder();          //Check for encoder press and rotation
        if (increasing)           //Change menu
        {
          menu = 1;
          disableBool();
        }
        if (pressed)              //Start/stop
        {
          disableBool();
          if (starter)
          {
            starter = false;
          }
          else
          {
            starter = true;
          }
          delay(200);
        }
        periodicUpdate();         //Update secondary mcu
        uint32_t elapsedScrollTime = millis() - scrollTimer;
        if (elapsedScrollTime >= scrollInterval)    //Change scrolling data periodically
        {
          scrollTimer = millis();
          if (scroll < 2)
          {
            scroll++;
          }
          else
          {
            scroll = 0;
          }
        }
        uint32_t elapsedTime = millis() - lasttime; //Check time since last refresh
        if (elapsedTime >= 500) //Refresh display
        {
          lasttime = millis(); //Last refresh = now
          if (starter)
          {
            indicator = !indicator;
            if (indicator)
            {
              cursorBottomRight();
              Serial.print(F("/"));
            }
            else
            {
              cursorBottomRight();
              Serial.print(F("|"));
            }
            wavespermin = 60000.0 / (totalwavetime + (delaybetweenwaves * 1000.0)); //Calculate WPM
            wavespermin = constrain(wavespermin, 0.0, 99.0);
            cursorHome();
            Serial.print(F("        "));
            cursorHome();
            Serial.print(wavespermin, 1); //# of waves to make per minute
            Serial.print(F(" WPM"));
            cursorTopRight(); //Set cursor to desired pos
            cursorLeft(5);
            totalWaves = constrain(totalWaves, 1, 99999);
            Serial.print(totalWaves - 1); //-1 Because wave increments when the paddle draws back, not goes forward
            Serial.print(F("W"));
          }
          cursorLine2();
          Serial.print(F("               "));
          cursorLine2();
          if (scroll == 0)
          {
            Serial.print(delaybetweenwaves, 1);
            Serial.print(F("s Delay"));
          }
          else if (scroll == 1)
          {
            pwpcnt = map(power, 15, 250, 100, 1);
            Serial.print(pwpcnt , 0);
            Serial.print(F("% Speed"));
          }
          else if (scroll == 2)
          {
            Serial.print(travelDeg, 0);
            Serial.write(0);
            Serial.print(F(" Range"));
          }
        }
      }
      break;

    case 1: //Waves per minute adjustment
      clearLCD();
      cursorHome();
      Serial.print(F("Delay btwn Waves"));
      cursorLine2();
      Serial.print(F("  "));
      if (delaybetweenwaves >= 10)
      {
        Serial.print(delaybetweenwaves, 0);
      }
      else
      {
        Serial.print(delaybetweenwaves, 1);
      }
      Serial.print(F(" sec"));
      disableBool();
      editing = false;
      while (menu == 1)
      {
        check_encoder(); //Process the encoder twist checking to eliminate slight pitch discrepencies when changing between screens
        if (pressed)
        {
          disableBool();
          if (editing)
          {
            editing = false;
          }
          else
          {
            editing = true;
          }
          delay(200);
        }
        if (increasing)
        {
          disableBool();
          if (editing)
          {
            delaybetweenwaves += 0.2;
            delaybetweenwaves = constrain(delaybetweenwaves, 0.2, 30.0);
          }
          else
          {
            menu = 2;
          }
        }
        else if (decreasing)
        {
          disableBool();
          if (editing)
          {
            delaybetweenwaves -= 0.2;
            delaybetweenwaves = constrain(delaybetweenwaves, 0.2, 30.0);
          }
          else
          {
            menu = 0;
          }
        }
        periodicUpdate();
        uint32_t elapsedTime = millis() - lasttime; //Check time since last refresh
        if (elapsedTime >= 500) //Refresh display
        {
          lasttime = millis(); //Last refresh = now
          if (starter) //If the wave action is going:
          {
            cursorBottomRight();
            indicator = !indicator;
            if (indicator)
            {
              Serial.print('/');
            }
            else
            {
              Serial.print('|');
            }
          }
          if (editing)
          {
            cursorLine2();
            Serial.print(F(">"));
          }
          else
          {
            cursorLine2();
            Serial.print(F(" "));
          }
          cursorLine2();
          cursorRight(1);
          Serial.print(F("              "));
          cursorLine2();
          cursorRight(2);
          Serial.print(delaybetweenwaves, 1);
          Serial.print(F(" sec"));
        }
      }
      break;

    case 2: //Wave Power Adjustment
      clearLCD();
      cursorHome();
      Serial.print(F("Paddle Speed"));
      cursorLine2();
      Serial.print(F("  "));
      pwpcnt = map(power, 15, 250, 100, 1);
      Serial.print(pwpcnt, 0); //Speed of wave action
      Serial.print(F("%"));
      disableBool();
      editing = false;
      while (menu == 2)
      {
        check_encoder(); //Process the encoder twist checking to eliminate slight pitch discrepencies when changing between screens
        if (pressed)
        {
          disableBool();
          if (editing)
          {
            editing = false;
          }
          else
          {
            editing = true;
          }
          delay(200);
        }
        if (increasing)
        {
          disableBool();
          if (editing)
          {
            pwpcnt++;
            pwpcnt = constrain(pwpcnt, 1, 100);
            power = map(pwpcnt, 1, 100, 250, 15);     //Calculate power based on chosen power percent
          }
          else
          {
            menu = 3;
          }
        }
        else if (decreasing)
        {
          disableBool();
          if (editing)
          {
            pwpcnt--;
            pwpcnt = constrain(pwpcnt, 1, 100);
            power = map(pwpcnt, 1, 100, 250, 15);     //Calculate power based on chosen power percent
          }
          else
          {
            menu = 1;
          }
        }
        periodicUpdate();
        uint32_t elapsedTime = millis() - lasttime; //Check time since last refresh
        if (elapsedTime >= 500) //Refresh display
        {
          lasttime = millis(); //Last refresh = now
          if (starter)
          {
            cursorBottomRight();
            lasttime = millis(); //Last refresh = now
            indicator = !indicator;
            if (indicator)
            {
              Serial.print('/');
            }
            else
            {
              Serial.print('|');
            }
          }
          if (editing)
          {
            cursorLine2();
            Serial.print(F(">"));
          }
          else
          {
            cursorLine2();
            Serial.print(F(" "));
          }
          cursorLine2();
          cursorRight(1);
          Serial.print(F("              "));
          cursorLine2();
          cursorRight(2);
          Serial.print(pwpcnt, 0); //Speed of wave action
          Serial.print(F("%"));
        }
      }
      break;

    case 3: //Travel Distance Adjustment
      clearLCD();
      cursorHome();
      Serial.print(F("Travel Range"));
      cursorLine2();
      Serial.print(F("  "));
      Serial.print(travelDeg, 0); //Range of steps in half-travel
      Serial.write(0);
      disableBool();
      editing = false;
      while (menu == 3)
      {
        check_encoder(); //Process the encoder twist checking to eliminate slight pitch discrepencies when changing between screens
        if (pressed)
        {
          disableBool();
          if (editing)
          {
            editing = false;
          }
          else
          {
            editing = true;
          }
          delay(200);
        }
        if (increasing)
        {
          disableBool();
          if (editing)
          {
            travelDeg++;
            travelDeg = constrain(travelDeg, 60, 105);
            stepRange = ((travelDeg - travelDZ) / .09) * -1;      //Calculate step range based on input travel degrees, involves deg/step, gear reduction, microstepping, and travel deadzone of neutral sensor
          }
          else
          {
            menu = 4;
          }
        }
        else if (decreasing)
        {
          disableBool();
          if (editing)
          {
            travelDeg--;
            travelDeg = constrain(travelDeg, 60, 105);
            stepRange = ((travelDeg - travelDZ) / .09) * -1;
          }
          else
          {
            menu = 2;
          }
        }
        periodicUpdate();
        uint32_t elapsedTime = millis() - lasttime; //Check time since last refresh
        if (elapsedTime >= 500) //Refresh display
        {
          lasttime = millis(); //Last refresh = now
          if (starter)
          {
            cursorBottomRight();
            lasttime = millis(); //Last refresh = now
            indicator = !indicator;
            if (indicator)
            {
              Serial.print('/');
            }
            else
            {
              Serial.print('|');
            }
          }
          if (editing)
          {
            cursorLine2();
            Serial.print(F(">"));
          }
          else
          {
            cursorLine2();
            Serial.print(F(" "));
          }
          cursorLine2();
          cursorRight(1);
          Serial.print(F("              "));
          cursorLine2();
          cursorRight(2);
          Serial.print(travelDeg, 0);
          Serial.write(0);
        }
      }
      break;

    case 4: //Tsunami Mode
      clearLCD();
      cursorHome();
      Serial.print(F("Tsunami Mode"));
      cursorLine2();
      Serial.print(F("(Press)"));
      disableBool();
      tsunami = true;
      Tsend = false;
      while (menu == 4)
      {
        check_encoder(); //Process the encoder twist checking to eliminate slight pitch discrepencies when changing between screens
        if (pressed)
        {
          disableBool();
          Tsend = true;
          delay(200);
        }
        if (decreasing)
        {
          disableBool();
          tsunami = false;
          menu = 3;
        }
        periodicUpdate();
      }
      break;

    default:    //If menu equals something other than a listed case number
      menu = 0;
      break;
  }
}

void setup()
{
  //LCD Initialization
  Serial.begin(9600);
  displayOn();
  setContrast(40);
  backlightBrightness(6);
  definecustom();
  clearLCD();
  cursorHome();

  //Pinmode configuration
  pinMode(channelA, INPUT_PULLUP);
  pinMode(channelB, INPUT_PULLUP);
  pinMode(encoderswitch, INPUT_PULLUP);

  //I2C setup
  Wire.begin(primary_address);
  Wire.onReceive(receiveEvent);

  //Splash screen and instructions
  Serial.print(F("Wave Maker v1.2"));
  cursorLine2();
  Serial.print(F("www.EMRIVER.com"));
  delay(1250);
  clearLCD();
  cursorHome();

  //Calculate starting stepRange
  stepRange = ((travelDeg - travelDZ) / .09) * -1;
}

void loop()
{
  menuselect();
}
