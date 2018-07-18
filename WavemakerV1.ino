// Wavemaker v1.0
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

/*
int directional = A0;
int stepper = A1;
int enable = A2;
int hall1 = A7;
int hall2 = A6;
int led1 = 3;			//LED pin reference
int led2 = 5;		//LED pin reference
int encoderswitch = 7;	//Encoder poin reference
int channelB = 6;	//Encoder pin reference
int channelA = 4;	//Encoder pin reference
uint16_t stepPulse_Micros = 200; //Constant
uint32_t power = 600;
bool rotation = true;
int stepcount = 0;
int stepRange = -600; //# of steps in half range in both directions
float percent = .35; //Zone of acceleration as a % of travel in decimal form
float celery = .03; //A nutritious vegetable, also a constant that controls accel/decel speed, bigger numbers make both happen slower
bool starter = true;
float pointer = 0; //Reference point for accel/decel
float pointer2 = 0;
int celerange = 0; //Used for accel/decel math
uint32_t celerydelay = 0;
uint16_t pdif = 0;
*/


// Global Variables & Magic Numbers
// Pin References
const uint8_t directional = A0;
const uint8_t stepper = A1;
const uint8_t enable = A2;
const uint8_t hall1 = A7;
const uint8_t hall2 = A6;
const uint8_t led1 = 3;			//LED pin reference
const uint8_t led2 = 5;		//LED pin reference
const uint8_t encoderswitch = 7;	//Encoder poin reference
const uint8_t channelB = 6;	//Encoder pin reference
const uint8_t channelA = 4;	//Encoder pin reference
uint16_t stepPulse_Micros = 200;
uint32_t power = 600; //Power, used it because it can be negative temporarily when decreasing by large increments
bool rotation = true; //Used to switch direction
int stepcount = 0; //Keeps track of steps, changed to int so we can use negative numbers
int stepRange = -600; //# of steps in half range in both directions = -250 * gear ratio
double percent = .35; //Percent of travel to be used as accel/decel
double celery = .03; //Constant that controls speed of acceleration 
bool starter = true; //Used to toggle wave start/stop
float pointer = 0; //Reference point for accel/decel
float pointer2 = 0; //Reference piont 2 for accel/decel
uint16_t celerange = 0; //Used for timing calculation of accel/decel
uint32_t celerydelay = 0; //Delay for current step in accel/decel part of range
uint16_t pdif = 0; //Difference between current step and reference point for accel/decel

// Other Globals
int released;				//Used for encoder button presses
static uint8_t cw_gray_codes[4] = { 2, 0, 3, 1 }; 	//Gray code sequence
static uint8_t ccw_gray_codes[4] = { 1, 3, 0, 2 };  //Gray code sequence
static uint8_t previous_gray_code = 0; //Last gray code.
uint8_t lastpress = HIGH; //Used for button press time
uint32_t pressTimeStart = 0; //Used for button press time
uint16_t pressTime; //Used for button press time
uint8_t menu = 0; //Menu #
uint8_t wavespermin = 23; //Setpoint in waves per minute
uint8_t lastwpm = 23; //Used to refresh wpm
uint32_t lastWaveTime = 0; //Millis() when last wave was sent
int lastpower = 600; //Used to refresh power

uint32_t totalWaves = 0; //Total waves sent
uint32_t lastwaves = 0; //Wave # at start of tsunami

bool tsunamitoggle = false; //Used to toggle tsunami


int lastStepRange = -600; 
bool error = false; //Used for error code display
bool lasterror = false;
uint8_t errorcode = 0; //Used to display current error code
uint32_t timeSinceLastWave = 0; //Used for WPM calculation
uint32_t otherDelay = 0;  //Used for WPM calculation and degree/s calculation
long waveDelay = 0; //Used for WPM execution
double dps = 160.0; //Used to display sensical value of paddle's roational speed.
double lastdps = 160.0;
uint32_t onetime = 0;
double totalwavetime = 0;
uint32_t lasttime = 0; 
double lastCelery = .03;
double lastPercent = .35;
uint32_t celerydelaytotal = 0; //Total delay for accel/decel per wave at current settings

bool indicator = true;

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
	for (uint8_t i=0; i < times; i++)
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

void calculations()
{
	wavespermin = constrain(wavespermin, 1, 200); //Waves can be between 1 and 60 waves per min
	power = constrain(power, 0, 1200); //Powerset can be between 1 and 250
	stepRange = constrain(stepRange, -660, -60); //-275 * gear ratio, and -25 * gear ratio
	percent = constrain(percent, .05, .45);
	celery = constrain(celery, .005, .1);
		//Math and calculations for delay between waves
	celerydelaytotal = 0; //Zero it out
		/*
		We know the number of steps in the acceleration zone
		The acceleration zone is defined as the shaded range, in one direction, equaling 4 per full wave
				                                                                                                    
                                                   .                                                
                                                 /hyh+`                                             
                                              `+moo o+ho`                                           
                                            `oms:/   +.-ys.                                         
                                          .sms+::     /-`.sy-                                       
                                        -ymo/+/-       ::` `oh/                                     
                                      :ymo//+/.         -:`  `+h+`                                  
                                    /hd+///+/`           ./.    /ho`                                
                                  /hh+///++/`             ./-     :ys.                              
                               `+hy////+/o:                `/-      .yy-                            
                             `ohs////+/++-                   /:       `sh:                          
                           .syo////+/+o+.                     :/`       `+h/                        
                         -yy+////+/+/+:.                       -/.         /ho`                     
                       :hs/////+/+//y:`                         .:-          :hs.                   
                     /do///+/+/+//:+-`                           .::           -yy-                 
                  `+do///+/+/+////o.                              `:/            .sh:               
                `omo///+/+/+/////o`                                `-+             `oh/             
              .smo///+/+/+////++s`                                   .+               /h+`          
            -ymo///+/+/+////+/+o                                      `o`               ym.         
          .hho///+/+/+////+/+oo                                        `o`            /h+`          
           -yh++/+//////+/+/o/                                           o.        `/h+`            
             .shs/////+/+//s:                                             +-     .sh/             
               `/yh+//+//:o-                                               /: `+ys.           
                  `+yh+//s.                                                -shs-              
                     `/sho-`                                           ./sy+.                
                         ./sss+-`                                 ./oss+-                      
                              -/ossso/:.`                `.-/+ssss+:`                          
                                    `-:+osssssssssssssssso+/-.                              
                                      
                                                                                                    
                                                                                                    
		For each step calculate the non-standard delay that is added based on the current settings
		Get the summation of this non-standard delay
		It is the total added delay for acceleration/deceleration in each zone
		Also, calculate the standard delay for each wave
		Find the standard delay for each step and multiply by the number of steps in step range
		Step range is half of the travel in one direction, so multiply by 4
		Add the standard delay and the non-standard delay to get the total delay per wave
		Divide by 1000 to convert microseconds to milliseconds
		Now that we know the time it takes to make one wave at current settings, multiply by waves per minute
		Out of 60s available, this lets us know how much time is remaining that needs to be taken up waiting
		Divide the remaining time by the number of waves per minutes so that the wait time is uniform between waves
		*/
	for (int i = 0; i < celerange; i++)
	{
		celerydelaytotal += (power * celery * pdif); //Summation of calculated accel/decel delay for each step in zone
	}
	otherDelay = ((((abs(stepRange) * 4) * (stepPulse_Micros + power)) + (celerydelaytotal * 4)) / 1000); 
	waveDelay = (60000 - (otherDelay  * wavespermin)) / wavespermin; //Millisec delay between waves
	if ((waveDelay <= 0) || (waveDelay >= 60000))
	{
		error = true;
		errorcode = 1; //Error code to indicate that current WPM and Power are incompatible.
	}
	else
	{
		error = false;
	}
	waveDelay = constrain(waveDelay, 0, 60000); //Dont let uint32_t roll over
}

static void check_encoder() // Look for encoder rotation, updating encoder_count as necessary.
{
  // Get the Gray-code state of the encoder.
  int gray_code = ((digitalRead(channelA) == HIGH) << 1) | (digitalRead(channelB) == HIGH);
  if (gray_code != previous_gray_code)   //Assign current gray code to last gray code
  {
	if (menu == 1) //Waves/Min Adj
	{
		//Knob twist CW
		if (gray_code == cw_gray_codes[previous_gray_code])
		{
			wavespermin++;
		}
		//Knob twist CW
		else if (gray_code == ccw_gray_codes[previous_gray_code])
		{
			wavespermin--;
		}
	}
	else if (menu == 2)
	{
		//Knob twist CW
		if (gray_code == cw_gray_codes[previous_gray_code])
		{
			power += 25;
		}
		//Knob twist CW
		else if (gray_code == ccw_gray_codes[previous_gray_code])
		{
			power -= 25;
		}
	}
	else if (menu == 3)
	{
		//Knob twist CW
		if (gray_code == cw_gray_codes[previous_gray_code])
		{
			stepRange -= 5;
		}
		//Knob twist CW
		else if (gray_code == ccw_gray_codes[previous_gray_code])
		{
			stepRange += 5;
		}
	}
	else if (menu == 4)
	{
		//Knob twist CW
		if (gray_code == cw_gray_codes[previous_gray_code])
		{
			percent += .01;
		}
		//Knob twist CW
		else if (gray_code == ccw_gray_codes[previous_gray_code])
		{
			percent -= .01;
		}
	}
	else if (menu == 5)
	{
		//Knob twist CW
		if (gray_code == cw_gray_codes[previous_gray_code])
		{
			celery += .005;
		}
		//Knob twist CW
		else if (gray_code == ccw_gray_codes[previous_gray_code])
		{
			celery -= .005;
		}
	}
	previous_gray_code = gray_code; //Stores current gray code for future comparison
	calculations(); //Calculate math
  }
}

/*
// Tsunami Wave Generator
void tsunami()
{
	power = powerset * powerMagic;
	if ((stepcount > (stepRange)) && (rotation)) //Sensor not triggered, direction set towards sensor, keep stepping
	{
		digitalWrite(enable, LOW); //Enable
		digitalWrite(directional, HIGH);
		digitalWrite(stepper, HIGH); //Start stepping
		delayMicroseconds(stepPulse_Micros); //Timer between pulses
		digitalWrite(stepper, LOW); //Stop stepping
		if ((power) <= 16000)
		{
			delayMicroseconds(power);
		}
		else
		{
			uint16_t powermilli = (power) / 1000;
			delay(powermilli);
		}
		stepcount--; //Decrease step counter
	}
	if ((stepcount <= (stepRange)) && (rotation)) //Sensor triggered, disable, switch direction
	{
		digitalWrite(enable, HIGH); //Disable coils
		rotation = false; //Change direction bool
		if (totalWaves > lastwaves)
		{
			tsunamitoggle = false; //Disable tsunami bool routine
		}
		totalWaves++; //Increment total wave counter
		cursorTopRight();
		Serial.print(" "); //Overwrite tsunami indicator (T) with ( )

	}
	if ((stepcount < (abs(stepRange))) && (!rotation)) //Sensor not triggered, direction set towards sensor, keep stepping
	{
		digitalWrite(enable, LOW); //Enable
		digitalWrite(directional, LOW);
		digitalWrite(stepper, HIGH); //Start stepping
		delayMicroseconds(stepPulse_Micros); //Timer between pulses
		digitalWrite(stepper, LOW); //Stop stepping
		if (power <= 16000)
		{
			delayMicroseconds(power);
		}
		else
		{
			uint16_t powermilli = power / 1000;
			delay(powermilli);
		}
		stepcount++; //Increase step counter
		if (analogRead(hall2) < 100) //Direction to send wave, neutral sensor detected, zero step counter
		{
			stepcount = 0;
		}
	}
	if ((stepcount >= (abs(stepRange))) && (!rotation)) //Sensor triggered, disable, switch direction
	{
		digitalWrite(enable, HIGH); //Enable
		rotation = true; //Change direction
		onetime = millis(); //Assign time to millis
	}
	if ((analogRead(hall1) < 100)) //Direction to send wave, neutral sensor detected, zero step counter
	{
		stepcount = stepRange;
		rotation = false;
	}
}
*/

// Switch handling
void switchpressed() //Called when encoder button pressed, reads time between falling edge and rising edge of button signal
//Has different press length routines, short, long, longer press
{
	if ((digitalRead(encoderswitch) == LOW) && (lastpress)) //Falling signal edge, happens when button is first pressed
	{
		lastpress = LOW; //Pressed low indicator
		pressTimeStart = millis(); //Start timer
	}
	if ((digitalRead(encoderswitch) == HIGH) && (!lastpress)) //Rising signal edge, happens when button is released
	{
		pressTime = millis() - pressTimeStart;
		lastpress = HIGH; //Reset indicator
		if (menu < 5) //Switch between standard menus
		{
			if (pressTime < 500) //Short press
			{
				menu++;
				delay(50);
			}
			else if ((pressTime >= 500) && (pressTime < 2500)) //Long press
			{
				togglestart();
				delay(50);
			}
			else if (pressTime >= 2500) //Longer press
			{
				starter = false;
				menu = 6;
				delay(50);
			}
		}
		else if (menu == 5)
		{
			if (pressTime < 500) //Short press
			{
				menu = 0;
				delay(50);
			}
			else if ((pressTime >= 500) && (pressTime < 2500)) //Long press
			{
				togglestart();
				delay(50);
			}
			else if (pressTime >= 2500) //Longer press
			{
				starter = false;
				menu = 6;
				delay(50);
			}
		}
		/*
		else //Tsunami menu controls
		{
			if (pressTime < 2500) //Short press
			{

				refreshbool = true; //Update display after
				cursorTopRight();
				Serial.print("T");
				rotation = false;
				tsunamitoggle = true;
				lastwaves = totalWaves;
			}
			else if (pressTime >= 2500) //Longer press
			{

				refreshbool = true; //Update display after
				menu = 0;
			}
		}
		*/
	}
}

//Toggle start/stop of wave action
void togglestart() //Flips start bool when called
{
  if (starter)
  {
	  digitalWrite(enable, HIGH); //Disable coils
	  digitalWrite(led1, LOW); //Turn off starter indicator
  }
  else
  {
	  rotation = true;
	  digitalWrite(led1, HIGH); //Indicate that wave action is started
  }
  starter = !starter; //Toggle starter bool
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
		if (analogRead(hall2) < 100)
		{
			stepcount = 0;
		}
	}
	//End of CCW movement reached
	else if ((stepcount <= (stepRange)) && (rotation)) //Sensor triggered, disable, switch direction
	{
		totalwavetime = millis() - onetime; //Elapsed time of movement
		lastWaveTime = millis(); //Assign current time to the completion of last wave
		totalWaves++; //Add one to the total wave counter
		rotation = false; //Change direction
		if (menu == 0)
		{
			cursorTopRight(); //Set cursor to desired pos
			cursorLeft(5);
			if (totalWaves > 99999) //Print waves elapsed (5 Positions Max)
			{
				totalWaves = 99999;
			}
			Serial.print(totalWaves - 1); //-1 Because wave increments when the paddle draws back, not goes forward
			Serial.print("W");
			cursorTopRight();
			Serial.println("Other Delay");
			Serial.println(otherDelay / 1000);
			Serial.println("Total Wave Time");
			Serial.println(totalwavetime / 1000);
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
		if (analogRead(hall2) < 100)
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

// Menu Handling, All functionality of program lives within the menuselector
void menuselect2()
{
	switch (menu)
	{
	case 0: //Default menu for info display only
		clearLCD();
		cursorHome();
		Serial.print(wavespermin); //# of waves to make per minute
		Serial.print(" WPM");
		cursorLine2();
		Serial.print(dps, 1); //# of degrees to move per second at current speed
		Serial.print(" DPS ");
		while (menu == 0)
		{
			if (starter) //If the wave action is going:
			{
				if ((error) && (!lasterror)) //Error toggled, show error code
				{
					lasterror = error;
					cursorBottomRight();
					cursorLeft(1);
					Serial.print("E"); //E for error
					Serial.print(errorcode); //Current error code
				}
				else if ((!error) && (lasterror)) //Error cleared, clear error code
				{
					lasterror = error;
					cursorBottomRight();
					cursorLeft(1);
					Serial.print("  ");
				}
				else if ((!error) && (!lasterror)) //No error, display walking paddle character
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
						
						dps = (abs((stepRange * .36) / 2.4) / (totalwavetime / 1000)); //Total degrees / total time of movement in 1D in seconds
						if (abs(lastdps - dps) > 1)
						{
							lastdps = dps;
							cursorLine2();
							Serial.print("         ");
							cursorLine2();
							Serial.print(dps, 1); //# of degrees to move per second at current speed
							Serial.print(" DPS ");
						}
					}

				}
				timeSinceLastWave = millis() - lastWaveTime; //Elapsed time between waves
				if ((timeSinceLastWave >= waveDelay) && (!error)) //Time since last wave is due
				{
					wave2(); //Keep calling wave until the current wave is complete
				}
			}
			else //Clear moving paddle symbol
			{
				cursorBottomRight();
				cursorLeft(1);
				Serial.print("  ");
			}
			switchpressed(); //Check for a switch press
		}
		break;

	case 1: //Waves per minute adjustment
		clearLCD();
		cursorHome();
		Serial.print("Waves/Minunte");
		cursorLine2();
		Serial.print("< ");
		Serial.print(wavespermin); //# of waves to make per minute
		Serial.print(" >");
		while (menu == 1)
		{
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
				if ((timeSinceLastWave >= waveDelay) && (!error)) //Time since last wave is due
				{
					wave2(); //Keep calling wave until the current wave is complete
				}
			}
			else //Clear moving paddle symbol
			{
				cursorBottomRight();
				cursorLeft(1);
				Serial.print("  ");
			}
			if (wavespermin != lastwpm)
			{
				lastwpm = wavespermin;
				cursorLine2();
				Serial.print("                ");
				cursorLine2();
				Serial.print("< ");
				Serial.print(wavespermin); //# of waves to make per minute
				Serial.print(" >");
			}
			check_encoder(); //Check to see if knob is being twisted
			switchpressed(); //Check to see if knob is being pressed
		}
		break;

	case 2: //Wave Power Adjustment
		clearLCD();
		cursorHome();
		Serial.print("Power Set");
		cursorLine2();
		Serial.print("< ");
		Serial.print(power); //Speed of wave action
		Serial.print(" >");
		while (menu == 2)
		{
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
				if ((timeSinceLastWave >= waveDelay) && (!error)) //Time since last wave is due
				{
					wave2(); //Keep calling wave until the current wave is complete
				}
			}
			if (power != lastpower)
			{
				lastpower = power;
				cursorLine2();
				Serial.print("                ");
				cursorLine2();
				Serial.print("< ");
				Serial.print(power); //Speed of wave action
				Serial.print(" >");
			}
			check_encoder(); //Check to see if knob is being twisted
			switchpressed(); //Check to see if knob is being pressed
		}
		break;

	case 3: //Travel Distance Adjustment
		clearLCD();
		cursorHome();
		Serial.print("Travel Range");
		cursorLine2();
		Serial.print("< ");
		Serial.print(abs(stepRange)); //Range of steps in half-travel
		Serial.print(" >");
		while (menu == 3)
		{
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
				if ((timeSinceLastWave >= waveDelay) && (!error)) //Time since last wave is due
				{
					wave2(); //Keep calling wave until the current wave is complete
				}
			}
			if (stepRange != lastStepRange)
			{
				lastStepRange = stepRange;
				cursorLine2();
				Serial.print("                ");
				cursorLine2();
				Serial.print("< ");
				Serial.print(abs(stepRange)); //Range of steps in half-travel
				Serial.print(" >");
				//Recalculate things that rely on steprange
				pointer = stepRange * (1 - percent); //Pointers are desired % of the way through the step range at either end
				pointer2 = (abs(pointer)); //First pointer is on negative side of 0, this is on positive side of 0
				celerange = (abs(stepRange - pointer)); //The current range of the 'celeration zone measured in steps (4 zones per full wave)
			}
			check_encoder(); //Check to see if knob is being twisted
			switchpressed(); //Check to see if knob is being pressed
		}
		break;

	case 4: //Accel/Decel Range Adjustment
		clearLCD();
		cursorHome();
		Serial.print("Accel/Decel");
		cursorLine2();
		Serial.print("Range ");
		Serial.print("< ");
		Serial.print((percent * 100), 0); //Range of accel/decel as % of travel
		Serial.print("% >");
		while (menu == 4)
		{
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
				if ((timeSinceLastWave >= waveDelay) && (!error)) //Time since last wave is due
				{
					wave2(); //Keep calling wave until the current wave is complete
				}
			}
			if (percent != lastPercent)
			{
				lastPercent = percent;
				cursorLine2();
				Serial.print("                ");
				cursorLine2();
				Serial.print("Range ");
				Serial.print("< ");
				Serial.print((percent * 100), 0); //Range of accel/decel as % of travel
				Serial.print("% >");
				//Recalculate things that rely on percent
				pointer = stepRange * (1 - percent); //Pointers are desired % of the way through the step range at either end
				pointer2 = (abs(pointer)); //First pointer is on negative side of 0, this is on positive side of 0
				celerange = (abs(stepRange - pointer)); //The current range of the 'celeration zone measured in steps (4 zones per full wave)

			}
			check_encoder(); //Check to see if knob is being twisted
			switchpressed(); //Check to see if knob is being pressed
		}
		break;
 
	case 5: //Accel/Decel Speed Adjustment
		clearLCD();
		cursorHome();
		Serial.print("Accel/Decel");
		cursorLine2();
		Serial.print("Speed ");
		Serial.print("< ");
		Serial.print(celery, 3); //Speed of accel/decel
		Serial.print(" >");
		while (menu == 5)
		{
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
				if ((timeSinceLastWave >= waveDelay) && (!error)) //Time since last wave is due
				{
					wave2(); //Keep calling wave until the current wave is complete
				}
			}
			if (celery != lastCelery)
			{
				lastCelery = celery;
				cursorLine2();
				Serial.print("                ");
				cursorLine2();
				Serial.print("Speed ");
				Serial.print("< ");
				Serial.print(celery, 3); //Speed of accel/decel
				Serial.print(" >");
			}
			check_encoder(); //Check to see if knob is being twisted
			switchpressed(); //Check to see if knob is being pressed
		}
		break;

		/*
	case 6: //Tsunami Mode
		clearLCD();
		cursorHome();
		Serial.print("Accel/Decel");
		cursorLine2();
		Serial.print("Speed ");
		Serial.print("< ");
		Serial.print(celery, 3); //Speed of accel/decel
		Serial.print(" >");
		while (menu == 5)
		{

		}
		break;
		*/

	default:
		menu = 0;
		break;
	}
}


void home()
{
	clearLCD();
	cursorHome();
	Serial.print("Paddle");
	cursorLine2();
	Serial.print("Homing");
	uint32_t elapsedMS = millis();
	uint32_t elapsedMS2 = 0;
	uint16_t timeout = 4000;
	while (starter)
	{
		elapsedMS2 = millis() - elapsedMS;
		uint16_t reading = analogRead(hall1);
		if (elapsedMS2 >= timeout)
		{
			digitalWrite(enable, HIGH); //Disable coils
			clearLCD();
			cursorHome();
			Serial.print("E0");
			cursorLine2();
			Serial.print("Homing Failed");
			delay(2000);
			clearLCD();
			cursorHome();
			Serial.print("E0 Check User");
			cursorLine2();
			Serial.print("Manual");
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
			Serial.print("Paddle Homed");
			cursorLine2();
			Serial.print("Successfully");
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
			delayMicroseconds(stepPulse_Micros * 10); //Timer between pulses
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
  pinMode(hall1, INPUT);
  pinMode(hall2, INPUT);
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
  celerange = (abs(stepRange - pointer)); //The current range of the 'celeration zone measured in steps (4 zones per full wave)
  calculations();
  //Splash screen and instructions
  
  clearLCD();             //Clear the display
  cursorHome();
  Serial.print("Wave Maker v1.0");
  cursorLine2();
  Serial.print("www.EMRIVER.com");
  delay(1500);
  clearLCD();             //Clear the display
  cursorHome();
  Serial.print("  Press Length  ");
  cursorLine2();
  Serial.print("   Controls:   ");
  delay(1000);
  clearLCD();             //Clear the display
  cursorHome();
  Serial.print("Press < .5s");
  cursorLine2();
  Serial.print("Switches Line");
  delay(1000);
  clearLCD();             //Clear the display
  cursorHome();
  Serial.print(".5s <Press< 2.5s");
  cursorLine2();
  Serial.print("Start/Stop Waves");
  delay(1000);
  clearLCD();             //Clear the display
  cursorHome();
  Serial.print("2.5s < Press");
  cursorLine2();
  Serial.print("Toggle Tsunami");
  delay(1000);
  home();
}

// Add the main program code into the continuous loop() function
void loop()
{
  menuselect2();
}