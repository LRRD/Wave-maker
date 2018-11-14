// Wavemaker v1.1
// 6/15/2018
// Mason Parrone
// Controls wave maker assembly with K500 controller board, stepper motor driver, stepper, 2 hall effect sensors
// User adjusts waves per minute on top line
// User adjusts power as % (1 - 100) on bottom line
// Short press switches lines
// Long press toggles start/stop of wave making
// Longer press switches to tsunami mode
// Short press sends tsunami wave
// Longer press switches back to normal mode
// Added error code if WPM is set high (115 WPM) and power is set low (1%) it cant make 115 waves per minute at that slow speed: (Error code 1 (E1))
// Updated some LCD functions to take a parameter instead of having to call them multiple times in a row
// Added splash screen
// Changed power % to degrees per secoond
// Added checks for power settings, delays of 16 ms or more us delay instead of delayMicro
// Added twist control changes for above 16 ms power delay
// Added acceleration controls to the paddle
// Changed menu layout to a flat style with adjustments on each menu selection and an information display screen
// Added walking character to show when paddle is moving
// Added check encoder function to menu 0 to eliminate pitch discrepencies between screens
// Production code 9/2018 with Arduino version 1.8.5 and code version 1.0
// Change to menu style, button press time too confusing and not user-friendly.
// Flat menu style and new button press controls
// Removed instructions and code for button press lengths
// Production code 11/2018 with Arduino version 1.8.5 and code version 1.1

// Global Variables & Magic Numbers
// Pin References
const uint8_t directional = A0; //Direction
const uint8_t stepper = A1; //Step
const uint8_t enable = A2; //Enable
const uint8_t backSensor = A7; //Hall sensor
const uint8_t neutralSensor = A6; //Hall sensor
const uint8_t led1 = 3;			//LED pin reference
const uint8_t led2 = 5;		//LED pin reference
const uint8_t encoderswitch = 7;	//Encoder poin reference
const uint8_t channelB = 6;	//Encoder pin reference
const uint8_t channelA = 4;	//Encoder pin reference

//Power
uint16_t power = 200; //Power, used uint because it can be negative temporarily when decreasing by large increments
uint16_t lastpower = 200; //Used to refresh power
uint8_t pwpcnt = 0; //Used to display power as a %

//Step
uint16_t stepPulse_Micros = 25; //Default delay between each step
int stepcount = 0; //Keeps track of steps, changed to int so we can use negative numbers
int stepRange = -900; //# of steps in half range in both directions = -250 * gear ratio
int lastStepRange = -900; //Used to refresh step range
int farBack = -1000; //Step count at far back sensor
float travelDeg = 0.0; //Degrees of travel at current steprange setting

//Waves
float lastwpm = 0.0; //Used to refresh waves per minute
float wavespermin = 0.0; //Setpoint in waves per minute
uint32_t lastdbw = 2000; //Used to refresh wpm
uint32_t lastWaveTime = 0; //Millis() when last wave was sent
uint32_t totalWaves = 0; //Total waves sent
uint32_t lastwaves = 0; //Wave # at start of tsunami
uint32_t delaybetweenwaves = 1200; //Delay between waves in ms
uint32_t timeSinceLastWave = 0; //Used for wave delay timing
uint32_t waveTime = 0; //Used for WPM execution
uint32_t onetime = 0; //Used for wave time calc
uint32_t totalwavetime = 0; //Wave time variable
uint32_t lasttime = 0;  //Used for wave delay timing

//Accel/decel
float percent = .35; //Percent of travel to be used as accel/decel
float celery = 0.0; //Constant that controls speed of acceleration
uint32_t celerydelay = 0; //Delay for current step in accel/decel part of range
uint16_t pdif = 0; //Difference between current step and reference point for accel/decel
float pointer = 0.0; //Reference point for accel/decel
float pointer2 = 0.0; //Reference piont 2 for accel/decel
float celerAval = -0.00008421;
float celerBval = 0.0421;

//Tsunami
bool tsunamitoggle = false; //Used to toggle tsunami
bool lasttsunami = false; //Used to refresh tsunami display
uint32_t Tpower = 10; //Tsunami power setting
float Tcelery = .025; //Tsunami celery setting
float Tpercent = .20; //Tsunami percent setting
int TstepRange = -660; //Tsunami step range setting
uint8_t Tspeed = 125; //How much slower paddle draws back compared to moves forward when making tsunami
float Tpointer = 0.0; //Reference point for tsunami accel/decel
float Tpointer2 = 0.0; //Reference point 2 for tsunami accel/decel
bool firstTwave = true; //Bool to indicate first tsunami wave on menu 4

//Encoder
int released;				//Used for encoder button presses
static uint8_t cw_gray_codes[4] = { 2, 0, 3, 1 }; 	//Gray code sequence
static uint8_t ccw_gray_codes[4] = { 1, 3, 0, 2 };  //Gray code sequence
static uint8_t previous_gray_code = 0; //Last gray code.
uint8_t lastpress = HIGH; //Used for button press time
uint32_t pressTimeStart = 0; //Used for button press time
uint16_t pressTime; //Used for button press time

//Degrees/Sec
float dps = 160.0; //Used to display sensical value of paddle's roational speed.
float lastdps = 160.0; //Used to refresh DPS

//Error Codes
bool error = false; //Used for error code display
bool lasterror = false; //Used to refresh error codes, super simple way of detecting rising/falling edge with 2 bools
uint8_t errorcode = 0; //Used to display current error code

//Other
bool rotation = true; //Used to switch direction
bool starter = true; //Used to toggle wave start/stop
bool starter2 = false; //Used to home when starting
uint16_t timeout = 4000; //Ms until homing function timeout
uint8_t menu = 0; //Menu #
bool indicator = true; //Used for walking paddle character
bool edit = false; //Used to select/deselct edit mode for variables
bool bootup = true;                                 //Used for a one time run on startup, different than setup()
uint16_t standardDelay = 150; //Stardardized delay time for actions

// Functions
// LCD Functions
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

// backspace and erase previous character
void backSpace(uint8_t times) {
  for (uint8_t i = 0; i < times; i++)
  {
    Serial.write(0xFE);
    Serial.write(0x4E);
  }
}

// move cursor left
void cursorLeft(uint8_t times) {
  for (uint8_t i = 0; i < times; i++)
  {
    Serial.write(0xFE);
    Serial.write(0x49);
  }
}

// move cursor right
void cursorRight(uint8_t times) {
  for (uint8_t i = 0; i < times; i++)
  {
    Serial.write(0xFE);
    Serial.write(0x4A);
  }
}

// set LCD contrast
void setContrast(uint8_t contrast) {
  Serial.write(0xFE);
  Serial.write(0x52);
  Serial.write(contrast); //Must be between 1 and 50
}

// turn on backlight
void backlightBrightness(uint8_t brightness) {
  Serial.write(0xFE);
  Serial.write(0x53);
  Serial.write(brightness); //Must be between 1 and 8
}

void constraints()
{
  delaybetweenwaves = constrain(delaybetweenwaves, 100, 60000);
  power = constrain(power, 25, 500); //Powerset can be between 1 and 250
  stepRange = constrain(stepRange, -1000, -100); //-275 * gear ratio, and -25 * gear ratio
  percent = constrain(percent, .05, .45);
  celery = power * celerAval + celerBval; //Computer celery based on power
  celery = constrain(celery, .0001, .05); //Constrain celery
}

static void check_encoder() // Look for encoder rotation, updating encoder_count as necessary.
{
  // Get the Gray-code state of the encoder.
  int gray_code = ((digitalRead(channelA) == HIGH) << 1) | (digitalRead(channelB) == HIGH);
  if (gray_code != previous_gray_code)   //Assign current gray code to last gray code
  {
    if (edit) //In edit mode, make edits
    {
      if (menu == 1) //Waves/Min Adj
      {
        if (delaybetweenwaves < 10000) //Fine control of delay between waves
        {
          //Knob twist CW
          if (gray_code == cw_gray_codes[previous_gray_code])
          {
            delaybetweenwaves += 100;
          }
          //Knob twist CW
          else if (gray_code == ccw_gray_codes[previous_gray_code])
          {
            delaybetweenwaves -= 100;
          }
        }
        else if (delaybetweenwaves > 10000) //Coarse control of delay between waves
        {
          //Knob twist CW
          if (gray_code == cw_gray_codes[previous_gray_code])
          {
            delaybetweenwaves += 1000;
          }
          //Knob twist CW
          else if (gray_code == ccw_gray_codes[previous_gray_code])
          {
            delaybetweenwaves -= 1000;
          }
        }
        else if (delaybetweenwaves == 10000) //Intermediary value
        {
          //Knob twist CW
          if (gray_code == cw_gray_codes[previous_gray_code])
          {
            delaybetweenwaves += 1000;
          }
          //Knob twist CW
          else if (gray_code == ccw_gray_codes[previous_gray_code])
          {
            delaybetweenwaves -= 100;
          }
        }
      }
      else if (menu == 2)
      {
        //Knob twist CW
        if (gray_code == cw_gray_codes[previous_gray_code])
        {
          power -= 5;
        }
        //Knob twist CW
        else if (gray_code == ccw_gray_codes[previous_gray_code])
        {
          power += 5;
        }
      }
      else if (menu == 3)
      {
        //Knob twist CW
        if (gray_code == cw_gray_codes[previous_gray_code])
        {
          stepRange -= 11;
        }
        //Knob twist CW
        else if (gray_code == ccw_gray_codes[previous_gray_code])
        {
          stepRange += 11;
        }
      }
    }
    else //Not in edit mode, change menu
    {
      //Knob twist CW
      if (gray_code == cw_gray_codes[previous_gray_code])
      {
        if (bootup)
        {
          bootup = false;
        }
        else
        {
          menu++; //When menu hits 5 it will default back to 0 using switch case
        }
      }
      //Knob twist CW
      else if (gray_code == ccw_gray_codes[previous_gray_code])
      {
        if (menu == 0)
        {
          menu = 4; //Jump from 0 to 4 instead of going to -1
        }
        else
        {
          menu--;
        }
      }
      delay(standardDelay);
    }
    previous_gray_code = gray_code; //Stores current gray code for future comparison
    constraints(); //Constrain variables being edited
  }
}

//Switch handling
void checkbutton() //Check for rising and falling edge of button press
{
  if ((digitalRead(encoderswitch) == LOW) && (lastpress)) //Falling signal edge, happens when button is first pressed
  {
    lastpress = LOW; //Pressed low indicator
  }
  else if ((digitalRead(encoderswitch) == HIGH) && (!lastpress)) //Rising signal edge, happens when button is released
  {
    switchpressed(); //Run switch pressed routine once the button is lifted
  }
}

// Switch handling
void switchpressed() //Called upon encoder button release
{
  lastpress = HIGH; //Reset indicator
  if (menu == 0) //Home screen
  {
    togglestart(); //Toggle start bool/routine
  }
  else if ((menu > 0) && (menu < 4)) //Adjustment menus
  {
    toggleedit(); //Toggle edit bool/routine
  }
  else //Tsunami menu controls
  {
    if (firstTwave) //First tsunami wave since entering menu, home routine
    {
      home2();
      firstTwave = false;
    }
    cursorTopRight();
    Serial.print(F("T"));
    rotation = true;
    digitalWrite(led1, HIGH); //Indicate that wave action is started
    tsunamitoggle = true;
    lastwaves = totalWaves;
  }
  delay(standardDelay);
}

//Toggle start/stop of wave action
void togglestart() //Flips start bool when called
{
  if (starter)
  {
    digitalWrite(enable, HIGH); //Disable coils
    digitalWrite(led1, LOW); //Turn off starter indicator
    cursorBottomRight(); //Erase walking paddle character
    cursorLeft(1);
    Serial.print(F("  "));
  }
  else
  {
    rotation = true;
    digitalWrite(led1, HIGH); //Indicate that wave action is started
    starter2 = true;
    home2();
  }
  starter = !starter; //Toggle starter bool
}

//Toggle edit mode
void toggleedit() //Flips edit bool when called
{
  cursorLine2();
  if (edit)
  {
    Serial.print(F(" "));
  }
  else
  {
    Serial.print(F(">"));
  }
  edit = !edit; //Toggle editor bool
}

//Main wave function
void wave2()
{
  //CCW Rotational move until reaching end of travel
  if ((stepcount > (stepRange)) && (rotation)) //Sensor not triggered, direction set towards sensor, keep stepping
  {
    //Step
    digitalWrite(enable, LOW); //Enable
    digitalWrite(directional, HIGH); //CCW looking at motor shaft end
    digitalWrite(stepper, HIGH); //Start stepping
    delayMicroseconds(stepPulse_Micros); //Timer between pulses
    digitalWrite(stepper, LOW); //Stop stepping
    //Wait
    if (stepcount < pointer)
    {
      pdif = (abs(pointer - stepcount));
      celerydelay = (power + (power * pdif * celery)); //Delay = normal delay at current power + (power * distance from reference pt * 'celeration constant), Makes the delay proportional to distance from the 0 point.
      if ((celerydelay) <= 16000) //Micro range
      {
        delayMicroseconds(celerydelay);
      }
      else //Milli range
      {
        uint16_t powermilli = (celerydelay) / 1000; //Convert micro to milli
        delay(powermilli);
      }
    }
    else if (stepcount > pointer2)
    {
      pdif = (abs(pointer2 - stepcount));
      celerydelay = (power + (power * pdif * celery));
      if ((celerydelay) <= 16000)
      {
        delayMicroseconds(celerydelay);
      }
      else
      {
        uint16_t powermilli = (celerydelay) / 1000;
        delay(powermilli);
      }
    }
    else
    {
      if ((power) <= 16000)
      {
        delayMicroseconds(power);
      }
      else
      {
        uint16_t powermilli = (power) / 1000;
        delay(power);
      }
    }
    stepcount--; //Decrease step counter
    //Zero counter at known point
    if (analogRead(neutralSensor) < 100)
    {
      stepcount = 0;
    }
  }
  //End of CCW movement reached
  else if ((stepcount <= (stepRange)) && (rotation)) //Sensor triggered, disable, switch direction
  {
    totalwavetime = (millis() - onetime) * 2; //Elapsed time of movement
    lastWaveTime = millis(); //Assign current time to the completion of last wave
    totalWaves++; //Add one to the total wave counter
    rotation = false; //Change direction
    if (menu == 0)
    {
      cursorTopRight(); //Set cursor to desired pos
      cursorLeft(5);
      totalWaves = constrain(totalWaves, 1, 99999);
      Serial.print(totalWaves - 1); //-1 Because wave increments when the paddle draws back, not goes forward
      Serial.print(F("W"));
    }
  }
  //CW Rotational move until reaching end of travel
  else if ((stepcount < (abs(stepRange))) && (!rotation)) //Sensor not triggered, direction set towards sensor, keep stepping
  {
    //Step
    digitalWrite(enable, LOW); //Enable
    digitalWrite(directional, LOW); //CCW looking at motor shaft end
    digitalWrite(stepper, HIGH); //Start stepping
    delayMicroseconds(stepPulse_Micros); //Timer between pulses
    digitalWrite(stepper, LOW); //Stop stepping
    //Wait
    if (stepcount < pointer)
    {
      pdif = (abs(pointer - stepcount));
      celerydelay = (power + (power * pdif * celery)); //Delay = normal delay at current power + (power * distance from reference pt * 'celeration constant), Makes the delay proportional to distance from the 0 point.
      if ((celerydelay) <= 16000) //Micro range
      {
        delayMicroseconds(celerydelay);
      }
      else //Milli range
      {
        uint16_t powermilli = (celerydelay) / 1000; //Convert micro to milli
        delay(powermilli);
      }
    }
    else if (stepcount > pointer2)
    {
      pdif = (abs(pointer2 - stepcount));
      celerydelay = (power + (power * pdif * celery));
      if ((celerydelay) <= 16000)
      {
        delayMicroseconds(celerydelay);
      }
      else
      {
        uint16_t powermilli = (celerydelay) / 1000;
        delay(powermilli);
      }
    }
    else
    {
      if ((power) <= 16000)
      {
        delayMicroseconds(power);
      }
      else
      {
        uint16_t powermilli = (power) / 1000;
        delay(power);
      }
    }
    stepcount++; //Decrease step counter
    if (analogRead(neutralSensor) < 100)
    {
      stepcount = 0;
    }
  }
  //End of CW movement reached
  else if ((stepcount >= (abs(stepRange))) && (!rotation)) //Sensor triggered, disable, switch direction
  {
    rotation = true; //Change direction
    onetime = millis(); //Assign time to millis for start of next wave
  }
}


// Tsunami Wave Generator
void tsunami()
{
  //CCW Rotational move until reaching end of travel
  if ((stepcount > (TstepRange)) && (rotation)) //Sensor not triggered, direction set towards sensor, keep stepping
  {
    //Step
    digitalWrite(enable, LOW); //Enable
    digitalWrite(directional, HIGH); //CCW looking at motor shaft end
    digitalWrite(stepper, HIGH); //Start stepping
    delayMicroseconds(stepPulse_Micros); //Timer between pulses
    digitalWrite(stepper, LOW); //Stop stepping
    //Wait
    if (stepcount < Tpointer)
    {
      pdif = (abs(Tpointer - stepcount));
      celerydelay = ((Tpower * Tspeed) + ((Tpower * Tspeed) * pdif * Tcelery)); //Delay = normal delay at current power + (power * distance from reference pt * 'celeration constant), Makes the delay proportional to distance from the 0 point.
      if ((celerydelay) <= 16000) //Micro range
      {
        delayMicroseconds(celerydelay);
      }
      else //Milli range
      {
        uint16_t powermilli = (celerydelay) / 1000; //Convert micro to milli
        delay(powermilli);
      }
    }
    else if (stepcount > Tpointer2)
    {
      pdif = (abs(Tpointer2 - stepcount));
      celerydelay = ((Tpower * Tspeed) + ((Tpower * Tspeed) * pdif * Tcelery));
      if ((celerydelay) <= 16000)
      {
        delayMicroseconds(celerydelay);
      }
      else
      {
        uint16_t powermilli = (celerydelay) / 1000;
        delay(powermilli);
      }
    }
    else
    {
      if ((Tpower * Tspeed) <= 16000)
      {
        delayMicroseconds(Tpower * Tspeed);
      }
      else
      {
        uint16_t powermilli = (Tpower * Tspeed) / 1000;
        delay(Tpower * Tspeed);
      }
    }
    stepcount--; //Decrease step counter
    //Zero counter at known point
    if (analogRead(neutralSensor) < 100)
    {
      stepcount = 0;
    }
  }
  //End of CCW movement reached
  else if ((stepcount <= (TstepRange)) && (rotation)) //Sensor triggered, disable, switch direction
  {
    totalWaves++; //Add one to the total wave counter
    rotation = false; //Change direction
  }
  //CW Rotational move until reaching end of travel
  else if ((stepcount < (abs(TstepRange))) && (!rotation)) //Sensor not triggered, direction set towards sensor, keep stepping
  {
    //Step
    digitalWrite(enable, LOW); //Enable
    digitalWrite(directional, LOW); //CCW looking at motor shaft end
    digitalWrite(stepper, HIGH); //Start stepping
    delayMicroseconds(stepPulse_Micros); //Timer between pulses
    digitalWrite(stepper, LOW); //Stop stepping
    //Wait
    if (stepcount < Tpointer)
    {
      pdif = (abs(Tpointer - stepcount));
      celerydelay = (Tpower + (Tpower * pdif * Tcelery)); //Delay = normal delay at current power + (power * distance from reference pt * 'celeration constant), Makes the delay proportional to distance from the 0 point.
      if ((celerydelay) <= 16000) //Micro range
      {
        delayMicroseconds(celerydelay);
      }
      else //Milli range
      {
        uint16_t powermilli = (celerydelay) / 1000; //Convert micro to milli
        delay(powermilli);
      }
    }
    else
    {
      if ((Tpower) <= 16000)
      {
        delayMicroseconds(Tpower);
      }
      else
      {
        uint16_t powermilli = (Tpower) / 1000;
        delay(Tpower);
      }
    }
    stepcount++; //Decrease step counter
    if (analogRead(neutralSensor) < 100)
    {
      stepcount = 0;
    }
  }
  //End of CW movement reached
  else if ((stepcount >= (abs(TstepRange))) && (!rotation)) //Sensor triggered, disable, switch direction
  {
    rotation = true; //Change direction
    tsunamitoggle = false;
  }
}

// Menu Handling, All functionality of program lives within the menuselector
void menuselect()
{
  switch (menu)
  {
    case 0: //Default menu for info display only
      clearLCD();
      cursorHome();
      Serial.print(wavespermin, 1); //# of waves to make per minute
      Serial.print(F(" WPM"));
      cursorLine2();
      if ((dps > 1) && (dps < 115))
      {
        Serial.print(dps, 1); //# of degrees to move per second at current speed
        Serial.print(F(" DPS  "));
        cursorBottomRight();
        cursorLeft(5);
        Serial.print(F("HOME"));
      }
      else
      {
        Serial.print(F("0.0 DPS  "));
        cursorBottomRight();
        cursorLeft(5);
        Serial.print(F("HOME"));
      }
      cursorTopRight(); //Set cursor to desired pos
      cursorLeft(5);
      totalWaves = constrain(totalWaves, 1, 99999);
      Serial.print(totalWaves - 1); //-1 Because wave increments when the paddle draws back, not goes forward
      Serial.print(F("W"));
      while (menu == 0)
      {
        check_encoder(); //Process the encoder twist checking to eliminate slight pitch discrepencies when changing between screens
        checkbutton(); //Check for a switch press
        if (starter) //If the wave action is going:
        {
          if ((error) && (!lasterror)) //Error toggled, show error code
          {
            lasterror = error;
            cursorBottomRight();
            cursorLeft(1);
            Serial.print(F("E")); //E for error
            Serial.print(errorcode); //Current error code
          }
          else if ((!error) && (lasterror)) //Error cleared, clear error code
          {
            lasterror = error;
            cursorBottomRight();
            cursorLeft(1);
            Serial.print(F("  "));
          }
          else if ((!error) && (!lasterror)) //No error, display walking paddle character
          {
            uint32_t elapsedTime = millis() - lasttime; //Check time since last refresh
            if (elapsedTime >= 1000) //Refresh display
            {
              lasttime = millis(); //Last refresh = now
              indicator = !indicator;
              if (indicator)
              {
                cursorBottomRight();
                Serial.print('/');
              }
              else
              {
                cursorBottomRight();
                Serial.print('|');
              }
              dps = (abs((stepRange * .36) / 4) / (totalwavetime / 1000)); //Total degrees / total time of movement in 1D in seconds
              if (abs(lastdps - dps) > 1)
              {
                lastdps = dps;
                cursorLine2();
                Serial.print(F("         "));
                cursorLine2();
                if ((dps > 1) && (dps < 115))
                {
                  Serial.print(dps, 1); //# of degrees to move per second at current speed
                  Serial.print(F(" DPS  "));
                  cursorBottomRight();
                  cursorLeft(5);
                  Serial.print(F("HOME"));
                }
                else
                {
                  Serial.print(F("0.0 DPS  "));
                  cursorBottomRight();
                  cursorLeft(5);
                  Serial.print(F("HOME"));
                }
              }
              waveTime = (totalwavetime) + (delaybetweenwaves); //Millisec delay between waves
              wavespermin = 60000 / waveTime;
              if (abs(wavespermin - lastwpm) > 0)
              {
                lastwpm = wavespermin;
                cursorHome();
                Serial.print(F("        "));
                cursorHome();
                Serial.print(wavespermin, 1); //# of waves to make per minute
                Serial.print(F(" WPM"));
              }
            }

          }
          timeSinceLastWave = millis() - lastWaveTime; //Elapsed time between waves
          if ((timeSinceLastWave >= delaybetweenwaves) && (!error)) //Time since last wave is due
          {
            checkbutton();
            wave2(); //Keep calling wave until the current wave is complete
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
      if (delaybetweenwaves > 9900)
      {
        Serial.print(delaybetweenwaves / 1000); //# of waves to make per minute
      }
      else
      {
        Serial.print(delaybetweenwaves / 1000); //# of waves to make per minute
        Serial.print('.');
        Serial.print((delaybetweenwaves % 1000) / 100); //Remainder
      }
      Serial.print(F(" sec"));
      while (menu == 1)
      {
        check_encoder(); //Process the encoder twist checking to eliminate slight pitch discrepencies when changing between screens
        checkbutton(); //Check for a switch press
        if (starter) //If the wave action is going:
        {
          if ((!error) && (!lasterror)) //No error, display walking paddle character
          {
            uint32_t elapsedTime = millis() - lasttime; //Check time since last refresh
            if (elapsedTime >= 1000) //Refresh display
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

          }
          timeSinceLastWave = millis() - lastWaveTime; //Elapsed time between waves
          if ((timeSinceLastWave >= delaybetweenwaves) && (!error)) //Time since last wave is due
          {
            checkbutton();
            wave2(); //Keep calling wave until the current wave is complete
          }
        }
        else //Clear moving paddle symbol
        {
          cursorBottomRight();
          cursorLeft(1);
          Serial.print(F("  "));
        }
        if (delaybetweenwaves != lastdbw)
        {
          lastdbw = delaybetweenwaves;
          cursorLine2();
          cursorRight(1);
          Serial.print(F("               "));
          cursorLine2();
          cursorRight(2);
          if (delaybetweenwaves > 9900)
          {
            Serial.print(delaybetweenwaves / 1000); //# of waves to make per minute
          }
          else
          {
            Serial.print(delaybetweenwaves / 1000); //# of waves to make per minute
            Serial.print('.');
            Serial.print((delaybetweenwaves % 1000) / 100); //Remainder
          }
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
      pwpcnt = 101 - (map(power, 25, 500, 1, 100));
      Serial.print(pwpcnt); //Speed of wave action
      Serial.print(F("%"));
      while (menu == 2)
      {
        check_encoder(); //Process the encoder twist checking to eliminate slight pitch discrepencies when changing between screens
        checkbutton(); //Check for a switch press
        if (starter) //If the wave action is going:
        {
          if ((!error) && (!lasterror)) //No error, display walking paddle character
          {
            uint32_t elapsedTime = millis() - lasttime; //Check time since last refresh
            if (elapsedTime >= 1000) //Refresh display
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

          }
          timeSinceLastWave = millis() - lastWaveTime; //Elapsed time between waves
          if ((timeSinceLastWave >= delaybetweenwaves) && (!error)) //Time since last wave is due
          {
            checkbutton();
            wave2(); //Keep calling wave until the current wave is complete
          }
        }
        if (power != lastpower)
        {
          pwpcnt = 101 - (map(power, 25, 500, 1, 100));
          lastpower = power;
          cursorLine2();
          cursorRight(1);
          Serial.print(F("               "));
          cursorLine2();
          cursorRight(2);
          Serial.print(pwpcnt); //Speed of wave action
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
      travelDeg = abs((stepRange * .36) / 4);
      Serial.print(travelDeg, 0); //Range of steps in half-travel
      Serial.print(F(" Degrees"));
      while (menu == 3)
      {
        check_encoder(); //Process the encoder twist checking to eliminate slight pitch discrepencies when changing between screens
        checkbutton(); //Check for a switch press
        if (starter) //If the wave action is going:
        {
          if ((!error) && (!lasterror)) //No error, display walking paddle character
          {
            uint32_t elapsedTime = millis() - lasttime; //Check time since last refresh
            if (elapsedTime >= 1000) //Refresh display
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

          }
          timeSinceLastWave = millis() - lastWaveTime; //Elapsed time between waves
          if ((timeSinceLastWave >= delaybetweenwaves) && (!error)) //Time since last wave is due
          {
            checkbutton();
            wave2(); //Keep calling wave until the current wave is complete
          }
        }
        if (stepRange != lastStepRange)
        {
          lastStepRange = stepRange;
          cursorLine2();
          cursorRight(1);
          Serial.print(F("               "));
          cursorLine2();
          cursorRight(2);
          travelDeg = abs((stepRange * .36) / 4);
          Serial.print(travelDeg, 0); //Range of steps in half-travel
          Serial.print(F(" Degrees"));
          //Recalculate things that rely on steprange
          pointer = stepRange * (1 - percent); //Pointers are desired % of the way through the step range at either end
          pointer2 = (abs(pointer)); //First pointer is on negative side of 0, this is on positive side of 0
        }
      }
      break;

    case 4: //Tsunami Mode
      firstTwave = true;
      clearLCD();
      cursorHome();
      starter2 = true;
      //home2();
      Serial.print(F("Tsunami Mode"));
      cursorLine2();
      Serial.print(F("(Press)"));
      while (menu == 4)
      {
        check_encoder(); //Process the encoder twist checking to eliminate slight pitch discrepencies when changing between screens
        checkbutton(); //Check for a switch press
        if ((tsunamitoggle) && (!lasttsunami)) //Tsunami toggled on, indicate as such
        {
          cursorTopRight();
          Serial.print(F("T"));
          lasttsunami = true;
        }
        else if ((tsunamitoggle) && (lasttsunami)) //Run tsunami function
        {
          checkbutton();
          tsunami();
        }
        else if ((!tsunamitoggle) && (lasttsunami)) //Tsunami toggled off, clear indicator
        {
          cursorTopRight();
          Serial.print(F(" "));
          lasttsunami = false;
        }
      }
      break;

    default:
      menu = 0;
      break;
  }
}


void home()
{
  clearLCD();
  cursorHome();
  Serial.print(F("Paddle"));
  cursorLine2();
  Serial.print(F("Homing"));
  uint32_t elapsedMS = millis();
  uint32_t elapsedMS2 = 0;
  while (starter)
  {
    elapsedMS2 = millis() - elapsedMS;
    uint16_t reading = analogRead(backSensor);
    if (elapsedMS2 >= timeout)
    {
      digitalWrite(enable, HIGH); //Disable coils
      clearLCD();
      cursorHome();
      Serial.print(F("E0"));
      cursorLine2();
      Serial.print(F("Homing Failed"));
      delay(2000);
      clearLCD();
      cursorHome();
      Serial.print(F("E0 Check User"));
      cursorLine2();
      Serial.print(F("Manual"));
      delay(2000);
      while (true)
      {
        //Never exit loop
      }
    }
    if (reading < 100)
    {
      clearLCD();
      cursorHome();
      Serial.print(F("Paddle Homed"));
      cursorLine2();
      Serial.print(F("Successfully"));
      delay(2000);
      togglestart();
    }
    else if (reading > 500)
    {
      digitalWrite(enable, LOW); //Enable
      digitalWrite(directional, HIGH);
      digitalWrite(stepper, HIGH); //Start stepping
      delayMicroseconds(stepPulse_Micros); //Timer between pulses
      digitalWrite(stepper, LOW); //Stop stepping
      delayMicroseconds(Tpower * Tspeed); //Timer between pulses
    }
  }
}

void home2() //Paddle motion started, travel backwards until one of two sensors is detected, reset step count to known physical point
{
  bool reverse = false;
  while (starter2)
  {
    uint16_t reading = analogRead(backSensor);
    uint16_t reading2 = analogRead(neutralSensor);
    if ((reading < 100) && (!reverse))
    {
      delay(500); //Wait before chaning directions
      reverse = true; //Go back to neutral sensor
    }
    else if (reading2 < 100)
    {
      stepcount = 0;
      starter2 = false;
      delay(500);
    }
    else if (((reading > 500) && (reading2 > 500)) && (!reverse))
    {
      digitalWrite(enable, LOW); //Enable
      digitalWrite(directional, HIGH);
      digitalWrite(stepper, HIGH); //Start stepping
      delayMicroseconds(stepPulse_Micros); //Timer between pulses
      digitalWrite(stepper, LOW); //Stop stepping
      delayMicroseconds(Tpower * Tspeed); //Timer between pulses
    }
    if ((reverse) && (reading2 > 500))
    {
      digitalWrite(enable, LOW); //Enable
      digitalWrite(directional, LOW);
      digitalWrite(stepper, HIGH); //Start stepping
      delayMicroseconds(stepPulse_Micros); //Timer between pulses
      digitalWrite(stepper, LOW); //Stop stepping
      delayMicroseconds(Tpower * Tspeed); //Timer between pulses
    }
  }
}

// The setup() function runs once each time the micro-controller starts
void setup()
{
  Serial.begin(9600);
  pinMode(stepper, OUTPUT);
  pinMode(directional, OUTPUT);
  pinMode(enable, OUTPUT);
  pinMode(backSensor, INPUT);
  pinMode(neutralSensor, INPUT);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(encoderswitch, INPUT_PULLUP);
  pinMode(channelB, INPUT_PULLUP); //Changed from input to input_pullup for model #62A11-02-040CH
  pinMode(channelA, INPUT_PULLUP); //Changed from input to input_pullup for model #62A11-02-040CH
  displayOn();            //Initialize the LCD Display (Without this it displays gibberish upon data recieve.)
  setContrast(45);
  backlightBrightness(8);
  pointer = stepRange * (1 - percent); //Pointers are desired % of the way through the step range at either end
  pointer2 = (abs(pointer)); //First pointer is on negative side of 0, this is on positive side of 0
  Tpointer = TstepRange * (1 - Tpercent); //Tsunami pointers are desired % of the way through the step range at either end
  Tpointer2 = (abs(Tpointer)); //First tsunami pointer is on negative side of 0, this is on positive side of 0

  //Splash screen and instructions
  clearLCD();
  cursorHome();
  Serial.print(F("Wave Maker v1.1"));
  cursorLine2();
  Serial.print(F("www.EMRIVER.com"));
  delay(1250);
  clearLCD();
  cursorHome();
  home(); //Homing function to test
}

// Add the main program code into the continuous loop() function
void loop()
{
  menuselect();
}
