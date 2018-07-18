// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       wavey.ino
    Created:	6/19/2018 1:52:51 PM
    Author:     RHINODOG\Mason
*/

// Define User Types below here or use a .h file
//
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
uint16_t stepPulse_micros = 200; //Constant
uint32_t power = 600; 
bool rotation = true;
int stepcount = 0;
const int stepRange = -600; //# of steps in half range in both directions
float percent = .35; //Zone of acceleration as a % of travel in decimal form
float celery = .03; //A nutritious vegetable, also a constant that controls accel/decel speed, bigger numbers make both happen slower
bool starter = true;
float pointer = 0; //Reference point for accel/decel
float pointer2 = 0;
int celerange = 0; //Used for accel/decel math
uint32_t celerydelay = 0; 
uint16_t pdif = 0;

// Define Function Prototypes that use User Types below here or use a .h file
//

   
// Define Functions below here or use other .ino or cpp files
//

void wave2()
{
	if ((stepcount > (stepRange)) && (rotation)) //Sensor not triggered, direction set towards sensor, keep stepping
	{
		digitalWrite(enable, LOW); //Enable
		digitalWrite(directional, HIGH);
		digitalWrite(stepper, HIGH); //Start stepping
		delayMicroseconds(stepPulse_micros); //Timer between pulses
		digitalWrite(stepper, LOW); //Stop stepping
		if (stepcount < pointer)
		{
			pdif = (abs(pointer - stepcount));
			celerydelay = (power + (power * pdif * celery));
			if ((celerydelay) <= 16000)
			{
				delayMicroseconds(celerydelay);
			}
			else
			{
				delay(celerydelay / 1000);
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
				delay(celerydelay / 1000);
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
				delay(power / 1000);
			}
		}
		stepcount--; //Decrease step counter
		if (analogRead(hall2) < 100)
		{
			stepcount = 0;
		}
	}
	else if ((stepcount <= (stepRange)) && (rotation)) //Sensor triggered, disable, switch direction
	{
		digitalWrite(enable, HIGH); //Enable
		rotation = false; //Change direction
	}
	else if ((stepcount < (abs(stepRange))) && (!rotation)) //Sensor not triggered, direction set towards sensor, keep stepping
	{
		digitalWrite(enable, LOW); //Enable
		digitalWrite(directional, LOW);
		digitalWrite(stepper, HIGH); //Start stepping
		delayMicroseconds(stepPulse_micros); //Timer between pulses
		digitalWrite(stepper, LOW); //Stop stepping
		if (stepcount < pointer)
		{
			pdif = (abs(pointer - stepcount));
			celerydelay = (power + (power * pdif * celery));
			if ((celerydelay) <= 16000)
			{
				delayMicroseconds(celerydelay);
			}
			else
			{
				delay(celerydelay / 1000);
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
				delay(celerydelay / 1000);
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
				delay(power / 1000);
			}
		}
		stepcount++; //Decrease step counter
		if (analogRead(hall2) < 100)
		{
			stepcount = 0;
		}
	}
	else if ((stepcount >= (abs(stepRange))) && (!rotation)) //Sensor triggered, disable, switch direction
	{
		digitalWrite(enable, HIGH); //Enable
		rotation = true; //Change direction
	}
}

void home()
{
	Serial.print("Paddle");
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
			Serial.print("E0");
			Serial.print("Homing Failed");
			delay(2000);
			Serial.print("E0 Check User");
			Serial.print("Manual");
			delay(2000);
			while (true)
			{
				//Never exit loop
			}
		}
		if (reading < 100)
		{
			digitalWrite(enable, HIGH); //Disable coils
			delay(2000);
			starter = false;
		}
		else if (reading > 500)
		{
			digitalWrite(enable, LOW); //Enable
			digitalWrite(directional, HIGH);
			digitalWrite(stepper, HIGH); //Start stepping
			delayMicroseconds(stepPulse_micros); //Timer between pulses
			digitalWrite(stepper, LOW); //Stop stepping
			delayMicroseconds(stepPulse_micros * 10); //Timer between pulses
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
	pinMode(channelB, INPUT);
	pinMode(channelA, INPUT);
	pointer = stepRange * (1 - percent);
	pointer2 = (abs(pointer));
	celerange = (abs(stepRange - pointer));
	home();
}



// Add the main program code into the continuous loop() function
void loop()
{	
	wave2();
}


