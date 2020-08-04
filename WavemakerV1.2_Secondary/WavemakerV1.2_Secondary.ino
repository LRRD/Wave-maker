// Wavemaker v1.2
// Secondary MCU
// Control stepper motor and read hall effect sensor input
// 2 Way data comm with master

#include <Arduino.h>
#include <Wire.h>

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
 
// Global Variables & Magic Numbers
const byte secondary_address = 8;       //I2C device address
const byte primary_address = 9;         //I2C primary device address
const byte directional = A1;            //Direction
const byte stepper = A2;                //Step
const byte enable = A0;                 //Enable
const byte backSensor = A7;             //Hall sensor
const byte neutralSensor = A6;          //Hall sensor

bool starter = false;                   //Used to toggle wave start/stop
int stepRange = 0;                      //# of steps in half range in both directions = -250 * gear ratio
float totalWaveTime = 0.0;              //Wave time variable
float lastTotalWaveTime = 0.0;          //Used to update total wave time
int power = 0;                          //Power, used uint because it can be negative temporarily when decreasing by large increments
bool error = false;                     //Used for error code display
float percent = 0.65;                   //Percent of travel to be used as accel/decel
float celery = 0.0;                     //Constant that controls speed of acceleration
float celerAval = -0.00008421;          //A value in acceleration/deceleration linear regression equation
float celerBval = 0.0421;               //B value in acceleration/deceleration linear regression equation
float delaybetweenwaves = 0.0;          //Delay between waves in ms
int stepcount = 0;                      //Keeps track of steps, changed to int so we can use negative numbers
bool rotation = true;                   //Used to switch direction
uint16_t stepPulse_Micros = 25;         //Default delay between each step
float pdif = 0.0;                       //Difference between current step and reference point for accel/decel
float pointer = 0.0;                    //Reference point for accel/decel
float pointer2 = 0.0;                   //Reference point 2 for accel/decel
float celerydelay = 0.0;                //Delay for current step in accel/decel part of range
float onetime = 0.0;                    //Used for wave time calc
uint32_t lastWaveTime = 0;              //Millis() when last wave was sent
uint32_t totalWaves = 0;                //Total waves sent
uint32_t lastTotalWaves = 0;            //Used to update totalwaves
bool homing = false;                    //True when running homing routine
uint32_t Tpower = 5;                    //Tsunami power setting
float Tcelery = 0.025;                  //Tsunami celery setting
float Tpercent = 0.20;                  //Tsunami percent setting
int TstepRange = -660;                  //Tsunami step range setting
uint8_t Tspeed = 125;                   //How much slower paddle draws back compared to moves forward when making tsunami
float Tpointer = 0.0;                   //Reference point for tsunami accel/decel
float Tpointer2 = 0.0;                  //Reference point 2 for tsunami accel/decel
int homePower = 250;                    //Power setting when homing routine is running
bool laststarter = false;               //Used to update start/stop of paddle motion
bool tsunami = false;                   //Used to signal primary is in tsunami mode
bool lasttsunami = false;               //Used to detect tsunami updates  
bool starter2 = false;                  //Used to signal point in homing routine
uint32_t updateTime = 0;                //Used to refresh periodically
uint16_t syncRate = 500;                //Refresh periodically every syncRate ms
float travelDZ = 16.47;                 //Degrees of travel in deadzone of neutral sensor
bool Tsend = false;                     //Signal mcu to send tsunami

//Function called when primary updates secondary over I2C, must match updateSecondary on primary
void receiveEvent()
{
	I2C_readAnything(starter);
	I2C_readAnything(stepRange);
	I2C_readAnything(power);
	I2C_readAnything(delaybetweenwaves);
  I2C_readAnything(tsunami);
  I2C_readAnything(Tsend);
	constraints();
}

//Update primary MCU with the listed variables, must match receiveEvent on primary
void updatePrimary()
{
  Wire.beginTransmission(primary_address);
  I2C_writeAnything(totalWaveTime);
  I2C_writeAnything(totalWaves);
  Wire.endTransmission();
}

//Constrain variables within acceptable ranges
void constraints()
{
  delaybetweenwaves = constrain(delaybetweenwaves, 0.2, 30.0);
  power = constrain(power, 15, 250); //Powerset can be between 1 and 250
  stepRange = constrain(stepRange, -1000, -450); //-275 * gear ratio, and -25 * gear ratio
  percent = constrain(percent, .05, .45);
  celery = power * celerAval + celerBval; //Computer celery based on power
  celery = constrain(celery, .0001, .05); //Constrain celery
  totalWaves = constrain(totalWaves, 1, 99999);
}

//Main wave function
void wave()
{
  //CW Rotational move until reaching end of travel
  if ((stepcount > (stepRange)) && (rotation)) //Sensor not triggered, direction set towards sensor, keep stepping
  {
    //Step
    digitalWrite(enable, LOW); //Enable
    digitalWrite(directional, HIGH); //CW looking at motor shaft end
    digitalWrite(stepper, HIGH); //Start stepping
    delayMicroseconds(stepPulse_Micros); //Timer between pulses
    digitalWrite(stepper, LOW); //Stop stepping
    //Wait
    if (stepcount < pointer)
    {
      pdif = (abs(pointer - stepcount));
      float power2 = power / 1.0;
      celerydelay = (power2 + (power2 * pdif * celery)); //Delay = normal delay at current power + (power * distance from reference pt * 'celeration constant), Makes the delay proportional to distance from the 0 point.
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
    totalWaveTime = (millis() - onetime) * 2; //Elapsed time of movement
    lastWaveTime = millis(); //Assign current time to the completion of last wave
    totalWaves++; //Add one to the total wave counter
    rotation = false; //Change direction
  }
  //CW Rotational move until reaching end of travel
  else if ((stepcount < (abs(stepRange))) && (!rotation)) //Sensor not triggered, direction set towards sensor, keep stepping
  {
    //Step
    digitalWrite(enable, LOW); //Enable
    digitalWrite(directional, LOW); //CCW looking at motor shaft end
    digitalWrite(stepper, HIGH); //Start stepping
    delayMicroseconds(stepPulse_Micros); //Timer between pulses5.958
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


//Send a tsunami wave
void tsunamiWave()
{
  //CW Rotational move until reaching end of travel
  if ((stepcount > (TstepRange)) && (rotation)) //Sensor not triggered, direction set towards sensor, keep stepping
  {
    //Step
    digitalWrite(enable, LOW); //Enable
    digitalWrite(directional, HIGH); //CW looking at motor shaft end
    digitalWrite(stepper, HIGH); //Start stepping
    delayMicroseconds(stepPulse_Micros); //Timer between pulses
    digitalWrite(stepper, LOW); //Stop stepping
    //Wait
    uint32_t returnPower = Tpower * 50;
    if ((returnPower) <= 16000)
    {
      delayMicroseconds(returnPower);
    }
    else
    {
      uint16_t powermilli = (returnPower) / 1000;
      delay(returnPower);
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
    Tsend = false;
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
    else if (stepcount > Tpointer2)
    {
      pdif = (abs(Tpointer2 - stepcount));
      celerydelay = (Tpower + (Tpower * pdif * Tcelery));
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
  }
}

//Orient paddle relative to sensor
void home2() //Paddle motion started, travel backwards until one of two sensors is detected, reset step count to known physical point
{
  bool reverse = true;
  while (starter2)
  {
    if (analogRead(neutralSensor) < 100)
    {
      stepcount = 0;
      starter2 = false;
    }
    else if ((analogRead(backSensor) < 100) && (reverse))
    {
      reverse = false;
      delay(1000);
    }
    else
    {
      digitalWrite(enable, LOW); //Enable
      if (reverse)
      {
        digitalWrite(directional, HIGH);
      }
      else
      {
        digitalWrite(directional, LOW);
      }
      digitalWrite(stepper, HIGH); //Start stepping
      delayMicroseconds(stepPulse_Micros); //Timer between pulses
      digitalWrite(stepper, LOW); //Stop stepping
      delayMicroseconds(homePower); //Timer between pulses
    }
  }
}

//Update primary periodically
void periodicUpdate()
{
  if ((totalWaves != lastTotalWaves) || (totalWaveTime != lastTotalWaveTime))    //Don't update primary unless there is new information to give it
  {
    lastTotalWaves = totalWaves;
    lastTotalWaveTime = totalWaveTime;
    updatePrimary();
    updateTime = millis();
  }
}

// Setup
void setup()
{
  //Pinmode setup
	pinMode(enable, OUTPUT);
  pinMode(directional, OUTPUT);
  pinMode(stepper, OUTPUT);
  pinMode(backSensor, INPUT_PULLUP);
  pinMode(neutralSensor, INPUT_PULLUP);

  //I2C setup
	Wire.begin(secondary_address);
	Wire.onReceive(receiveEvent); // register event

  //Initial math calculations
  pointer = stepRange * (1 - percent); //Pointers are desired % of the way through the step range at either end
  pointer2 = (abs(pointer)); //First pointer is on negative side of 0, this is on positive side of 0
  Tpointer = TstepRange * (1 - Tpercent); //Tsunami pointers are desired % of the way through the step range at either end
  Tpointer2 = (abs(Tpointer)); //First tsunami pointer is on negative side of 0, this is on positive side of 0
  
  starter2 = true;
  home2();
}

void loop()
{
  if (tsunami)  //Different routine for tsunami mode
  {
    if (!lasttsunami)  //If tsunami mode just turned on
    {
      home2();
      lasttsunami = true;
    }
    else
    {
      if (Tsend)
      {
        tsunamiWave();
      }
    }
  }
  if (starter && !tsunami)  //Normal wave action
  {
    if (millis() - lastWaveTime >= (delaybetweenwaves * 1000.0))  //Wait before sending next wave
    {
      wave();
    }
  }
  //Update primary
  if (millis() - updateTime >= syncRate)  //Don't update primary faster than sync time
  {
    periodicUpdate();
  }
}
