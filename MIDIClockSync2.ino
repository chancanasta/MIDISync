//#define DEBUG_OUTPUT
#define TRUE 1
#define FALSE 0

#define SYNC_24 0
#define SYNC_48 1
#define SYNC_96 2

#define MIDI_BAUD 31250

//number of MIDI clocks per quarter note
#define MIDI_CLOCKS 24

//out pulse output
#define CLOCK_PIN   12
#define CLOCK_FREQ  800
#define CLOCK_TIME  2

//number of loops to keep LED on for a "flash"
#define LED_COUNT  200

//bunch of globals for the clock
byte gClockSetting=0;
byte gClockMultiplier=1;
byte gTickSetting=0;
byte gOldTickSetting=-1;
byte gClockCount=0;
byte gClockRunning=0;
//a few more
unsigned long gLastMilli=0;
unsigned long gMilliGap=0;
unsigned long gLastOutTick=0;
unsigned long gOutTickGap=0;
byte gGotInTick=0;
byte gOutTickCount;
//LED flash
byte gFlashCnt=0;
byte gLEDOn=FALSE;
byte gLEDCount=0;


#ifdef DEBUG_OUTPUT
//somewhere to format our messages
char gOutBuffer[40];
#endif

void setup() 
{
//MIDI
  pinMode(2, OUTPUT);     
  pinMode(3, OUTPUT);  
  digitalWrite(3, LOW); // GND 0 Volt supply to opto-coupler
  digitalWrite(2, HIGH); // +5 Volt supply to opto-coupler

  pinMode(4, INPUT); // Set Inputs for 4 way DIP Switch
  pinMode(5, INPUT); 
  pinMode(6, INPUT); 
  pinMode(7, INPUT); 
  digitalWrite(4, HIGH); // Set inputs Pull-up resistors High
  digitalWrite(5, HIGH);
  digitalWrite(6, HIGH);
  digitalWrite(7, HIGH);

//LED setup
  pinMode(LED_BUILTIN,OUTPUT);   // declare the LED's pin as output
//MIDI in setup
  Serial.begin(MIDI_BAUD);  //start serial with midi baudrate 31250
  Serial.flush();
#ifdef DEBUG_OUTPUT
  Serial.print("Starting\r\n");
#endif
}

void loop() 
{
  byte midiByte;

//are we flashing the LED?
  if(gLEDOn && gLEDCount==LED_COUNT)
    digitalWrite(LED_BUILTIN, HIGH );
 
//read in the DIP switches    
  gClockSetting=digitalRead(4) + (digitalRead(5)<<1) + (digitalRead(6)<<2) + (digitalRead(7)<<3);
  
//set the multiplier for the output clock
  switch(gClockSetting)
  {
    case SYNC_48:
      gClockMultiplier=2; //48ppq (Linn etc)
      break;
    case SYNC_96:
      gClockMultiplier=4; //96ppq (Oberheim etc)
      break;
    case SYNC_24:
    default:
      gClockMultiplier=1; //24ppq (Roland etc)

  }
  gTickSetting=gClockMultiplier*MIDI_CLOCKS;

//see if it's changed
  if(gTickSetting!=gOldTickSetting)
  {
#ifdef DEBUG_OUTPUT
    snprintf(gOutBuffer, sizeof(gOutBuffer), "PPQ set to  %d (DIP:%d)\r\n", gTickSetting,gClockSetting);
    Serial.write(gOutBuffer);
#endif
    gOldTickSetting=gTickSetting;
  }
 
 
// Read the incoming MIDI bytes and trap for start,stop,continue and clock signals 
  if (Serial.available())
  {
//get the byte
    midiByte = Serial.read();
  
//check if it's a status byte
    if(midiByte&0xF0)
    {
//it is, check for clocks, starts and stops
      switch(midiByte)
      {
//-------------------  
//Clock
        case 0xF8:           
          gotClock();
          break;
//Start
        case 0xFA:
    #ifdef DEBUG_OUTPUT
          Serial.write("Start\r\n");
    #endif
          gClockRunning=TRUE;
          gClockCount=0;
          gFlashCnt=0;
          break;
//Continue
        case 0xFB:
    #ifdef DEBUG_OUTPUT
          Serial.write("Continue\r\n");
    #endif
          gClockRunning=TRUE;
          break;
//Stop
        case 0xFC:
    #ifdef DEBUG_OUTPUT
          Serial.write("Stop\r\n");
    #endif
          gClockRunning=FALSE;
          break;
      }
    }
  //ignore all other MIDI data
  }
 
//LED on ?
  if(gLEDOn)
  {
//if it's on, decrease the counter
    gLEDCount--;
    if(!gLEDCount)
    {
//hit zero, turn off the LED
      digitalWrite(LED_BUILTIN, LOW );
      gLEDOn=FALSE;
    }

  }
//Check if we need to send a sync pulse
  checkSync();
}

//handles incoming midi clock signals
void gotClock()
{
//get the current time
  unsigned long time=millis();
//flag we got a tick
  gGotInTick=1;
#ifdef DEBUG_OUTPUT
  if(gOutTickCount!=0)
  {
    snprintf(gOutBuffer,"ERROR - got clock in but out tick not zero (%)\r\n",gOutTickCount);
    Serial.write(gOutBuffer);
  }
#endif

//
  gClockCount++;
//set the number of out ticks from this "in tick"  
  gOutTickCount=gClockMultiplier;
//get gap since last tick
  if(gLastMilli!=0)
  {
    gMilliGap=time-gLastMilli;
//get the gap between our "out ticks"
    gOutTickGap=gMilliGap/gClockMultiplier;
    gLastMilli=time;
  }
  else
  {
    gMilliGap=0;
    gLastMilli=time;
  }
 
//check if we've hit the 24ppq MIDI clocks
   if(gClockCount==MIDI_CLOCKS)
   {
      gClockCount=0;
#ifdef BLINK_ON_INPUT
//see if we're running or not
      if(gClockRunning)
        flash();
#endif
   }
}

//handles sending sync signal
void checkSync()
{
//if we got an in tick, just prior to this - send
  if(gGotInTick)
  {
    gGotInTick=0;
    if(gOutTickCount)
      sendSync();
  }
  else
  {
  //haven't recieved an in tick, see if we have pending out ticks
      if(gOutTickCount)
      {
//check time between now and last send
        unsigned long now=millis();
        unsigned long timeDiff=now-gLastOutTick;
        if(timeDiff>=gOutTickGap)
            sendSync();
      }
  }

}

void sendSync()
{
  gFlashCnt++;
//record the time we sent this
  gLastOutTick=millis();
//decrease the number of out ticks
  if( gOutTickCount>0)
    gOutTickCount--;

//if we're running, send a square wave to the output pin
  if(gClockRunning)
    tone(CLOCK_PIN,CLOCK_FREQ,CLOCK_TIME);
//if we've hit PPQ, flash
  if(gFlashCnt==MIDI_CLOCKS*gClockMultiplier)
  {
    if(gClockRunning)
        flash();
    gFlashCnt=0;
  }
}

void flash()
{
//check if we're already 'flashing'
  if(!gLEDOn)
  {
//we aren't, so set the LED var to on and initialse the counter
    gLEDOn=TRUE;
    gLEDCount=LED_COUNT;
  }
}
//------------------------------------------------------------------------------





