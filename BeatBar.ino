#include <Adafruit_NeoPixel.h>
#define LEFT_BTN_PIN  12
#define RIGHT_BTN_PIN  13
#define PIN_POTI       A7
#define LENGTH         64  //length of the bar, maximum 128
const int ledPins[5] = {2, 4, 6, 8, 10};



/// @file    TwinkleFox.ino
/// @brief   Twinkling "holiday" lights that fade in and out.
/// @example TwinkleFox.ino

#include "FastLED.h"


#define NUM_LEDS      180
#define LED_TYPE   WS2811
#define COLOR_ORDER   GRB
#define DATA_PIN        2
//#define CLK_PIN       4
#define VOLTS          12
#define MAX_MA       4000





//  TwinkleFOX: Twinkling 'holiday' lights that fade in and out.
//  Colors are chosen from a palette; a few palettes are provided.
//
//  This December 2015 implementation improves on the December 2014 version
//  in several ways:
//  - smoother fading, compatible with any colors and any palettes
//  - easier control of twinkle speed and twinkle density
//  - supports an optional 'background color'
//  - takes even less RAM: zero RAM overhead per pixel
//  - illustrates a couple of interesting techniques (uh oh...)
//
//  The idea behind this (new) implementation is that there's one
//  basic, repeating pattern that each pixel follows like a waveform:
//  The brightness rises from 0..255 and then falls back down to 0.
//  The brightness at any given point in time can be determined as
//  as a function of time, for example:
//    brightness = sine( time ); // a sine wave of brightness over time
//
//  So the way this implementation works is that every pixel follows
//  the exact same wave function over time.  In this particular case,
//  I chose a sawtooth triangle wave (triwave8) rather than a sine wave,
//  but the idea is the same: brightness = triwave8( time ).  
//  
//  Of course, if all the pixels used the exact same wave form, and 
//  if they all used the exact same 'clock' for their 'time base', all
//  the pixels would brighten and dim at once -- which does not look
//  like twinkling at all.
//
//  So to achieve random-looking twinkling, each pixel is given a 
//  slightly different 'clock' signal.  Some of the clocks run faster, 
//  some run slower, and each 'clock' also has a random offset from zero.
//  The net result is that the 'clocks' for all the pixels are always out 
//  of sync from each other, producing a nice random distribution
//  of twinkles.
//
//  The 'clock speed adjustment' and 'time offset' for each pixel
//  are generated randomly.  One (normal) approach to implementing that
//  would be to randomly generate the clock parameters for each pixel 
//  at startup, and store them in some arrays.  However, that consumes
//  a great deal of precious RAM, and it turns out to be totally
//  unnessary!  If the random number generate is 'seeded' with the
//  same starting value every time, it will generate the same sequence
//  of values every time.  So the clock adjustment parameters for each
//  pixel are 'stored' in a pseudo-random number generator!  The PRNG 
//  is reset, and then the first numbers out of it are the clock 
//  adjustment parameters for the first pixel, the second numbers out
//  of it are the parameters for the second pixel, and so on.
//  In this way, we can 'store' a stable sequence of thousands of
//  random clock adjustment parameters in literally two bytes of RAM.
//
//  There's a little bit of fixed-point math involved in applying the
//  clock speed adjustments, which are expressed in eighths.  Each pixel's
//  clock speed ranges from 8/8ths of the system clock (i.e. 1x) to
//  23/8ths of the system clock (i.e. nearly 3x).
//
//  On a basic Arduino Uno or Leonardo, this code can twinkle 300+ pixels
//  smoothly at over 50 updates per seond.
//
//  -Mark Kriegsman, December 2015

CRGBArray<NUM_LEDS> leds;

// Overall twinkle speed.
// 0 (VERY slow) to 8 (VERY fast).  
// 4, 5, and 6 are recommended, default is 4.
#define TWINKLE_SPEED 4

// Overall twinkle density.
// 0 (NONE lit) to 8 (ALL lit at once).  
// Default is 5.
#define TWINKLE_DENSITY 3

// How often to change color palettes.
#define SECONDS_PER_PALETTE  15
// Also: toward the bottom of the file is an array 
// called "ActivePaletteList" which controls which color
// palettes are used; you can add or remove color palettes
// from there freely.

// Background color for 'unlit' pixels
// Can be set to CRGB::Black if desired.
CRGB gBackgroundColor = CRGB::Black; 
// Example of dim incandescent fairy light background color
// CRGB gBackgroundColor = CRGB(CRGB::FairyLight).nscale8_video(16);

// If AUTO_SELECT_BACKGROUND_COLOR is set to 1,
// then for any palette where the first two entries 
// are the same, a dimmed version of that color will
// automatically be used as the background color.
#define AUTO_SELECT_BACKGROUND_COLOR 0

// If COOL_LIKE_INCANDESCENT is set to 1, colors will 
// fade out slighted 'reddened', similar to how
// incandescent bulbs change color as they get dim down.
#define COOL_LIKE_INCANDESCENT 1

CRGBPalette16 gCurrentPalette;
CRGBPalette16 gTargetPalette;

class BeatCounter {
  //counts midi beat-messages and calculates current number of beats
  private:
    int16_t beats; //number of fully passed beats
    const int16_t COUNTSPERBEAT = 24; //number of timing messages within a quater beat
    void (*onNewBeat)(int16_t); // Callback function to notify the caller about the end of a beat

  public:
    int16_t length; //length of the bar in beats
    int16_t counts; //counted timing messages since last beat

    BeatCounter& operator=(BeatCounter&& other) {
      counts = other.counts;
      beats = other.beats;
      length = other.length;
      onNewBeat = other.onNewBeat;
      return *this;
    }

    BeatCounter(){
      //empty default constructor
    }

    BeatCounter(int16_t barLength, void (*newBeatCallback)(int16_t)) {
      //length in number of single beats
      length = barLength;
      onNewBeat = newBeatCallback;
      Reset();
    }

    int16_t GetValue() {
      //returns current value in 0..100
      return ((beats * COUNTSPERBEAT + counts) * 100) / (length * COUNTSPERBEAT);
    }

    int16_t GetBeats(){
      //returns the counter value as beats
      return beats;
    }

    void Increment(){
      counts++;
      if(counts >= COUNTSPERBEAT){
        //12 timing-counts = one beat, actually one quater-beat, but here referred to as a beat
        counts = 0;
        beats++;
        if(beats >= length){
          //length reached, reset counter
          Reset();
        } else{
          onNewBeat(beats);
        }       
      }
    }

    void Reset(){
      counts = 0;
      beats = 0;
      onNewBeat(0);
    }

    void changeLength(bool up){
      length *= up?2:1/2;
      constrain(length, 4, 128);
    }

};

// Define enum for message types
enum MidiMessageTypes {
  Invalid = 0x00,                 // zero as invalid message
  NoteOff = 0x80,                 // Data byte 1: note number, Data byte 2: velocity
  NoteOn = 0x90,                  // Data byte 1: note number, Data byte 2: velocity
  PolyphonicAftertouch = 0xA0,    // Data byte 1: note number, Data byte 2: pressure
  ControlChange = 0xB0,           // Data byte 1: controller number, Data byte 2: controller value
  ProgramChange = 0xC0,           // Data byte 1: program number
  ChannelAftertouch = 0xD0,       // Data byte 1: pressure
  PitchBend = 0xE0,               // Data byte 1: LSB, Data byte 2: MSB (14-bit value)
  SystemExclusive = 0xF0,         // Data bytes: variable length (sysex message)
  TimeCodeQuarterFrame = 0xF1,    // Data byte 1: time code value
  SongPositionPointer = 0xF2,     // Data byte 1: LSB, Data byte 2: MSB (14-bit value)
  SongSelect = 0xF3,              // Data byte 1: song number
  TuneRequest = 0xF6,             // No data bytes
  EndOfExclusive = 0xF7,          // No data bytes (end of sysex message)
  TimingClock = 0xF8,             // No data bytes
  Start = 0xFA,                   // No data bytes
  Continue = 0xFB,                // No data bytes
  Stop = 0xFC,                    // No data bytes
  ActiveSensing = 0xFE,           // No data bytes
  Reset = 0xFF                    // No data bytes
};

class MidiReader {
  private:
    // Callback function to notify the caller about received midi messages
    void (*onNewMidi)(MidiMessageTypes, int, int, int);

    // Buffer for incoming midi messages
    int midiBuffer[3];
    uint8_t bufferIndex = 0;

  public:
    bool running = true; //is false after midi stop message, and back true after start/continue
    MidiReader(){}

    MidiReader(void (*newMidiCallback)(MidiMessageTypes, int, int, int)) {
      onNewMidi = newMidiCallback;
    }

    // Method to be called in the main loop
    void update() { 
      while (Serial.available() > 0) {
        uint8_t incomingByte = Serial.read();
        // Serial.print(incomingByte, HEX);
        // Serial.print("\n");
        if(incomingByte & 0x80){
          //is command byte, MSB == 1
          midiBuffer[0] = incomingByte;
          bufferIndex = 1;
        } else {
          // databyte, MSB == 0
          if(bufferIndex > 0){
            midiBuffer[bufferIndex] = incomingByte;
            bufferIndex++;
          } else {
            //error, databyte recieved without command byte
          }
        }
        int val = midiBuffer[0];
        if(val & 0x80){
          MidiMessageTypes type = 0;
          uint8_t channel = 0;
          //check if message complete
          if(midiBuffer[0] >= 0xF0){
            //is System Message 0xF0..0xFF
            type = (MidiMessageTypes)midiBuffer[0];
          } else{
            //is channel message
            type = (MidiMessageTypes)(midiBuffer[0] & 0xF0);
            channel = midiBuffer[0] & 0x0F;
          }
          switch(type){
            case Invalid:
              //Invalid Message
            break;

            case NoteOn:
            case NoteOff:
            case PolyphonicAftertouch:
            case ControlChange:
            case PitchBend:
            case SongPositionPointer:
            case SystemExclusive:
              //messages that require 2 data bytes
              if(bufferIndex == 2){
                //enough data for NoteOn Message available
                onNewMidi(type, channel, midiBuffer[1], midiBuffer[2]);              
              }
            break;

            case ProgramChange:
            case ChannelAftertouch:
            case SongSelect:
            case TimeCodeQuarterFrame:
              //messages that require 1 data byte
              if(bufferIndex == 1){
                //enough data for NoteOn Message available
                onNewMidi(type, channel, midiBuffer[1], 0);              
              }
            break;

            case Reset:
            case ActiveSensing:
            case Stop:
            case Start:
            case Continue:
            case TimingClock:
            case EndOfExclusive:
            case TuneRequest:
              //messages that require no data bytes
              onNewMidi(type, 0, 0, 0);
              if(type == Start || type == Continue){
                running = true;
              } else if (type == Stop){
                running = false;
              }
            break;

            default:
              // //Unknown message recieved
              // Serial.print("unknown Message: ");
              // Serial.print(type, HEX);
              // Serial.print("\n");
            break;
          }          
        }
      }
    }
};

class Led{
  private:
    int pin;
    int brightness = 2;
    int brightCounter = 0;
    int brightPeriod = 10;
    int blinkRate = 0;
    int blinkDuty = 0;
    int blinkCount = 0;
    int blinkCounter = 0;
    int lastMlls;
    bool state;
  public:
    Led(){
      
    }

    Led(int pin){
      //
      lastMlls = millis();
      this->pin = pin;
      pinMode(pin, OUTPUT);
      brightCounter = brightPeriod;
    }

    void update(){
      int mls = millis();
      int deltat = mls - lastMlls;
      lastMlls = mls;
      bool dimmed = false;
      if(brightCounter > 0){
        brightCounter -= deltat;
        if(brightCounter < brightness){
          dimmed = false;
        } else {
          dimmed = true;
        }
        if(brightCounter <= 0){
          brightCounter = brightPeriod;
        }
      }
      if(blinkCounter > 0){
        blinkCounter -= deltat;
        if(blinkCounter > (blinkRate - blinkDuty)){
          digitalWrite(pin, state && !dimmed);
        } else {
          digitalWrite(pin, LOW);
        }
        if(blinkCounter <= 0){
          if(blinkCount != 0){
            if(blinkCount > 0){
              blinkCount--;
            }
            blinkCounter = blinkRate;
          }          
        }
      } else{
        if(blinkCount != 0){
          if(blinkCount > 0){
            blinkCount --;
          }
          blinkCounter = blinkRate;
        }
        digitalWrite(pin, state && !dimmed);
      }
    }

    void setState(bool state){
      this->state = state;
    }

    void blink(int rate, int duty, int count){
      //sets to blink N times, count = -1 for infinite
      //rate in ms period time, 1000 slow rate, 10 fast rate
      //duty in 0..100 (%)
      blinkRate = rate;
      blinkDuty = (duty * rate)/100;
      blinkCount = count;
      blinkCounter = rate;
    }

    void setBrightness(int brightness){
      //brightness in 0..10
      this->brightness = brightness;
    }
};

class LedBar{
  private:

  public:
    int length = 180;
    int pin;
    uint16_t position = 0; //position of the pointer
    bool highlighted = false;
    uint32_t markerColor = CRGB::Green;
    uint32_t highlightColor = CRGB::Blue;
    uint32_t pointerColor = CRGB::Red;
    int16_t MARKERS[8] = {0, 22, 45, 67, 90, 112, 135, 157};

    LedBar(){
    }

    void update(int beats, int beatsLength){
      highlighted = ((beats%4)+4)%4 == 0;
      position = (length * beats) / beatsLength;
    }

    bool isMarker(int pixel){
      bool is = false;
      for(int i=0; i<8; i++){
        is = is || MARKERS[i] == pixel;
      }
      return is;
    }
};

class Button{
  public:
    bool state = false;
    bool lastState = false;
    bool pressed = false;
    bool released = false;
    int pin = 0;

    Button(){
      
    }

    Button(int pinNumber){
      //Initialize button input
      pin = pinNumber;
      pinMode(pinNumber, INPUT);
    }

    void update(){
      //call periodically
      lastState = state;
      state = digitalRead(pin);
      pressed = !lastState && state; //was pressed in this iteration
      released = lastState && !state; //was released in this iteration
    }
};

MidiReader midi;
LedBar ledBar;
BeatCounter beatCounter;

Led boardLeds[5];
int lastMlls;
int brightCounter = 0;
int currBrightness = 0;
Button leftBtn;
Button rightBtn;
bool pausemode = false;

void cb_onBeat(int16_t beats){
  //called at end of beat

  ledBar.update(beats, beatCounter.length);

  if(beats == 0){
    for(int i=0; i<5; i++){
      boardLeds[i].setState(true);
    }
  } else{
    for(int i=0; i<5; i++){
      boardLeds[i].setState(false);
    }
  }

  if(!pausemode){
    int i = 0;
    for( CRGB& pixel: leds) {
      if(beats == 0){
        pixel = CRGB::Red;
      } else if(i == ledBar.position){
        pixel = CRGB::Red;
      } else if(ledBar.isMarker(i)){
        pixel = ledBar.highlighted?CRGB::Blue : CRGB::Green;
      } else{
        pixel = 0;
      }
      i++;
    }  
    FastLED.show();     
  }
  

}

void cb_onMidi(MidiMessageTypes cmd, int channel, int data1, int data2){
  //is called my MidiReader on every incoming midi message. Use MidiMessageTypes enum to check for specific messages
  switch(cmd){
    case Start:
      pausemode = false;
      beatCounter.Reset();
    break;
    case Stop:
      pausemode = true;
      beatCounter.Reset();
    break;
    case TimingClock:
      // if(midi.running){
        beatCounter.Increment();
      // }

      // Serial.print("Bar.position: " + (String)beatCounter.counts + " - Beats: " + (String)beatCounter.GetBeats() + " - running: " + (String)midi.running + " - cmd byte: ");
      // Serial.print(cmd, HEX);
      // Serial.print( "\n");
    break;

  }

  // Serial.print("Midi Message: 0x");
  // Serial.print(cmd, HEX);
  // Serial.print(" / 0x");
  // Serial.print(channel, HEX);
  // Serial.print(" / 0x");
  // Serial.print(data1, HEX);
  // Serial.print(" / 0x");
  // Serial.print(data2, HEX);
  // Serial.print("\n");
}

void setup() {
  Button lbt(LEFT_BTN_PIN);
  Button rbt(RIGHT_BTN_PIN);
  leftBtn = lbt;
  rightBtn = rbt;
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(31250);
  midi = MidiReader(cb_onMidi);
  beatCounter = BeatCounter(LENGTH, cb_onBeat);

  pinMode(PIN_POTI, INPUT);

  for(int i=0; i<5; i++){
    Led ld(ledPins[i]);
    boardLeds[i] = ld;
    boardLeds[i].setState(true);
  }
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip);

  chooseNextColorPalette(gTargetPalette);
}

void loop() {
  leftBtn.update();
  rightBtn.update();
  if(rightBtn.pressed){
    beatCounter.changeLength(true);
  }
  
  if(leftBtn.pressed){
    beatCounter.changeLength(false);
  }
  int16_t bright = analogRead(PIN_POTI)/4;
  FastLED.setBrightness(constrain(bright+10, 0, 250));
  for(int i=0; i<5; i++){
    boardLeds[i].setBrightness(bright/50 + 1);
    boardLeds[i].update();
  }
  midi.update();
    if(pausemode){
      EVERY_N_SECONDS( SECONDS_PER_PALETTE ) { 
        chooseNextColorPalette( gTargetPalette ); 
      }
      
      EVERY_N_MILLISECONDS( 10 ) {
        nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 12);
      }

      drawTwinkles(leds, ledBar);   
      FastLED.show();    
    }    
  }

//  This function loops over each pixel, calculates the 
//  adjusted 'clock' that this pixel should use, and calls 
//  "CalculateOneTwinkle" on each pixel.  It then displays
//  either the twinkle color of the background color, 
//  whichever is brighter.
void drawTwinkles( CRGBSet& L, LedBar bar)
{
  // "PRNG16" is the pseudorandom number generator
  // It MUST be reset to the same starting value each time
  // this function is called, so that the sequence of 'random'
  // numbers that it generates is (paradoxically) stable.
  uint16_t PRNG16 = 11337;
  
  uint32_t clock32 = millis();

  // Set up the background color, "bg".
  // if AUTO_SELECT_BACKGROUND_COLOR == 1, and the first two colors of
  // the current palette are identical, then a deeply faded version of
  // that color is used for the background color
  CRGB bg;
  if( (AUTO_SELECT_BACKGROUND_COLOR == 1) &&
      (gCurrentPalette[0] == gCurrentPalette[1] )) {
    bg = gCurrentPalette[0];
    uint8_t bglight = bg.getAverageLight();
    if( bglight > 64) {
      bg.nscale8_video( 16); // very bright, so scale to 1/16th
    } else if( bglight > 16) {
      bg.nscale8_video( 64); // not that bright, so scale to 1/4th
    } else {
      bg.nscale8_video( 86); // dim, scale to 1/3rd.
    }
  } else {
    bg = gBackgroundColor; // just use the explicitly defined background color
  }

  uint8_t backgroundBrightness = bg.getAverageLight();
  int i = 0;
  for( CRGB& pixel: L) {
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    uint16_t myclockoffset16= PRNG16; // use that number as clock offset
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    // use that number as clock speed adjustment factor (in 8ths, from 8/8ths to 23/8ths)
    uint8_t myspeedmultiplierQ5_3 =  ((((PRNG16 & 0xFF)>>4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
    uint32_t myclock30 = (uint32_t)((clock32 * myspeedmultiplierQ5_3) >> 3) + myclockoffset16;
    uint8_t  myunique8 = PRNG16 >> 8; // get 'salt' value for this pixel

    // We now have the adjusted 'clock' for this pixel, now we call
    // the function that computes what color the pixel should be based
    // on the "brightness = f( time )" idea.
    CRGB c = computeOneTwinkle( myclock30, myunique8);

    uint8_t cbright = c.getAverageLight();
    int16_t deltabright = cbright - backgroundBrightness;
  if( deltabright >= 32 || (!bg)) {
      // If the new pixel is significantly brighter than the background color, 
      // use the new color.
      pixel = c;
    } else if( deltabright > 0 ) {
      // If the new pixel is just slightly brighter than the background color,
      // mix a blend of the new color and the background color
      pixel = blend( bg, c, deltabright * 8);
    } else { 
      // if the new pixel is not at all brighter than the background color,
      // just use the background color.
      pixel = bg;
    }
  }
}


//  This function takes a time in pseudo-milliseconds,
//  figures out brightness = f( time ), and also hue = f( time )
//  The 'low digits' of the millisecond time are used as 
//  input to the brightness wave function.  
//  The 'high digits' are used to select a color, so that the color
//  does not change over the course of the fade-in, fade-out
//  of one cycle of the brightness wave function.
//  The 'high digits' are also used to determine whether this pixel
//  should light at all during this cycle, based on the TWINKLE_DENSITY.
CRGB computeOneTwinkle( uint32_t ms, uint8_t salt)
{
  uint16_t ticks = ms >> (8-TWINKLE_SPEED);
  uint8_t fastcycle8 = ticks;
  uint16_t slowcycle16 = (ticks >> 8) + salt;
  slowcycle16 += sin8( slowcycle16);
  slowcycle16 =  (slowcycle16 * 2053) + 1384;
  uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);
  
  uint8_t bright = 0;
  if( ((slowcycle8 & 0x0E)/2) < TWINKLE_DENSITY) {
    bright = attackDecayWave8( fastcycle8);
  }

  uint8_t hue = slowcycle8 - salt;
  CRGB c;
  if( bright > 0) {
    c = ColorFromPalette( gCurrentPalette, hue, bright, NOBLEND);
    if( COOL_LIKE_INCANDESCENT == 1 ) {
      coolLikeIncandescent( c, fastcycle8);
    }
  } else {
    c = CRGB::Black;
  }
  return c;
}


// This function is like 'triwave8', which produces a 
// symmetrical up-and-down triangle sawtooth waveform, except that this
// function produces a triangle wave with a faster attack and a slower decay:
//
//     / \ 
//    /     \ 
//   /         \ 
//  /             \ 
//

uint8_t attackDecayWave8( uint8_t i)
{
  if( i < 86) {
    return i * 3;
  } else {
    i -= 86;
    return 255 - (i + (i/2));
  }
}

// This function takes a pixel, and if its in the 'fading down'
// part of the cycle, it adjusts the color a little bit like the 
// way that incandescent bulbs fade toward 'red' as they dim.
void coolLikeIncandescent( CRGB& c, uint8_t phase)
{
  if( phase < 128) return;

  uint8_t cooling = (phase - 128) >> 4;
  c.g = qsub8( c.g, cooling);
  c.b = qsub8( c.b, cooling * 2);
}

// A mostly red palette with green accents and white trim.
// "CRGB::Gray" is used as white to keep the brightness more uniform.
const TProgmemRGBPalette16 RedGreenWhite_p FL_PROGMEM =
{  CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, 
   CRGB::Red, CRGB::Red, CRGB::Red, CRGB::Red, 
   CRGB::Red, CRGB::Red, CRGB::Gray, CRGB::Gray, 
   CRGB::Green, CRGB::Green, CRGB::Green, CRGB::Green };

// A mostly (dark) green palette with red berries.
#define Holly_Green 0x00580c
#define Holly_Red   0xB00402
const TProgmemRGBPalette16 Holly_p FL_PROGMEM =
{  Holly_Green, Holly_Green, Holly_Green, Holly_Green, 
   Holly_Green, Holly_Green, Holly_Green, Holly_Green, 
   Holly_Green, Holly_Green, Holly_Green, Holly_Green, 
   Holly_Green, Holly_Green, Holly_Green, Holly_Red 
};

// A red and white striped palette
// "CRGB::Gray" is used as white to keep the brightness more uniform.
const TProgmemRGBPalette16 RedWhite_p FL_PROGMEM =
{  CRGB::Red,  CRGB::Red,  CRGB::Red,  CRGB::Red, 
   CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray,
   CRGB::Red,  CRGB::Red,  CRGB::Red,  CRGB::Red, 
   CRGB::Gray, CRGB::Gray, CRGB::Gray, CRGB::Gray };

// A mostly blue palette with white accents.
// "CRGB::Gray" is used as white to keep the brightness more uniform.
const TProgmemRGBPalette16 BlueWhite_p FL_PROGMEM =
{  CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
   CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
   CRGB::Blue, CRGB::Blue, CRGB::Blue, CRGB::Blue, 
   CRGB::Blue, CRGB::Gray, CRGB::Gray, CRGB::Gray };

// A pure "fairy light" palette with some brightness variations
#define HALFFAIRY ((CRGB::FairyLight & 0xFEFEFE) / 2)
#define QUARTERFAIRY ((CRGB::FairyLight & 0xFCFCFC) / 4)
const TProgmemRGBPalette16 FairyLight_p FL_PROGMEM =
{  CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, 
   HALFFAIRY,        HALFFAIRY,        CRGB::FairyLight, CRGB::FairyLight, 
   QUARTERFAIRY,     QUARTERFAIRY,     CRGB::FairyLight, CRGB::FairyLight, 
   CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight, CRGB::FairyLight };

// A palette of soft snowflakes with the occasional bright one
const TProgmemRGBPalette16 Snow_p FL_PROGMEM =
{  0x304048, 0x304048, 0x304048, 0x304048,
   0x304048, 0x304048, 0x304048, 0x304048,
   0x304048, 0x304048, 0x304048, 0x304048,
   0x304048, 0x304048, 0x304048, 0xE0F0FF };

// A palette reminiscent of large 'old-school' C9-size tree lights
// in the five classic colors: red, orange, green, blue, and white.
#define C9_Red    0xB80400
#define C9_Orange 0x902C02
#define C9_Green  0x046002
#define C9_Blue   0x070758
#define C9_White  0x606820
const TProgmemRGBPalette16 RetroC9_p FL_PROGMEM =
{  C9_Red,    C9_Orange, C9_Red,    C9_Orange,
   C9_Orange, C9_Red,    C9_Orange, C9_Red,
   C9_Green,  C9_Green,  C9_Green,  C9_Green,
   C9_Blue,   C9_Blue,   C9_Blue,
   C9_White
};

// A cold, icy pale blue palette
#define Ice_Blue1 0x0C1040
#define Ice_Blue2 0x182080
#define Ice_Blue3 0x5080C0
const TProgmemRGBPalette16 Ice_p FL_PROGMEM =
{
  Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
  Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
  Ice_Blue1, Ice_Blue1, Ice_Blue1, Ice_Blue1,
  Ice_Blue2, Ice_Blue2, Ice_Blue2, Ice_Blue3
};


// Add or remove palette names from this list to control which color
// palettes are used, and in what order.
const TProgmemRGBPalette16* ActivePaletteList[] = {
  // &RetroC9_p,
  &BlueWhite_p,
  // &RainbowColors_p,
  // &FairyLight_p,
  // &RedGreenWhite_p,
  // &PartyColors_p,
  // &RedWhite_p,
  &Snow_p,
  &Holly_p,
  &Ice_p  
};


// Advance to the next color palette in the list (above).
void chooseNextColorPalette( CRGBPalette16& pal)
{
  const uint8_t numberOfPalettes = sizeof(ActivePaletteList) / sizeof(ActivePaletteList[0]);
  static uint8_t whichPalette = -1; 
  whichPalette = addmod8( whichPalette, 1, numberOfPalettes);

  pal = *(ActivePaletteList[whichPalette]);
}
