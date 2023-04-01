#include <Adafruit_NeoPixel.h>
#define PIN_NEO_PIXEL  2   // Arduino pin that connects to NeoPixel
#define NUM_PIXELS     180  // The number of LEDs (pixels) on NeoPixel
#define LENGTH         64  //length of the bar, maximum 128
const int ledPins[5] = {2, 4, 6, 8, 10};

Adafruit_NeoPixel NeoPixel(NUM_PIXELS, PIN_NEO_PIXEL, NEO_GRB + NEO_KHZ800);

class BeatCounter {
  //counts midi beat-messages and calculates current number of beats
  private:
    int16_t counts; //counted timing messages since last beat
    int16_t beats; //number of fully passed beats
    const int16_t COUNTSPERBEAT = 24; //number of timing messages within a quater beat
    void (*onNewBeat)(int16_t); // Callback function to notify the caller about the end of a beat

  public:
    int16_t length; //length of the bar in beats

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
        onNewBeat(beats);
      }
      if(beats >= length){
        //length reached, reset counter
        Reset();
      } 
    }

    void Reset(){
      counts = 0;
      beats = 0;
      onNewBeat(0);
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

MidiReader midi;
BeatCounter beatCounter;
const int16_t MARKERS[8] = {0, 22, 45, 67, 90, 112, 135, 157}; //LEDs that are markers in 8th of the circle

void cb_onBeat(int16_t beats){
  //called at end of beat
  NeoPixel.clear();
  if(beats > 0){
    for(int i = 0; i < sizeof(MARKERS); i++){
      NeoPixel.setPixelColor(MARKERS[i], NeoPixel.Color(0,255,0));
    }
    uint16_t pos = (NUM_PIXELS * beats) / beatCounter.length;
    NeoPixel.fill(NeoPixel.Color(255,0,0), pos, 1);
  }
  NeoPixel.show();
}

void cb_onMidi(MidiMessageTypes cmd, int channel, int data1, int data2){
  //is called my MidiReader on every incoming midi message. Use MidiMessageTypes enum to check for specific messages
  //check for timing
  // Serial.print("Midi cmd: ");
  // Serial.print(cmd, HEX);
  // Serial.print(" - channel: ");
  // Serial.print(channel, HEX);
  // Serial.print(" - data1: ");
  // Serial.print(data1, HEX);
  // Serial.print(" - data2: ");
  // Serial.print(data2, HEX);
  // Serial.print(" - channel: " + (String)channel + " - data1: " + (String)data1 + " - data2: " + (String)data2 + "\n");
  switch(cmd){
    case Start:
      beatCounter.Reset();
    break;
    case Stop:
      beatCounter.Reset();
    break;
    case TimingClock:
      if(midi.running){
        beatCounter.Increment();
      }
      // Serial.print("Counts: " + (String)beatCounter.GetValue() + " - Beats: " + (String)beatCounter.GetBeats() + " \n");
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

Led leds[5];
int lastMlls;
int brightCounter = 0;
int currBrightness = 0;

void setup() {
  NeoPixel.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  NeoPixel.setPin(PIN_NEO_PIXEL);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(31250);
  midi = MidiReader(cb_onMidi);
  beatCounter = BeatCounter(LENGTH, cb_onBeat);

  for(int i=0; i<5; i++){
    Led ld(ledPins[i]);
    leds[i] = ld;
    leds[i].setState(true);
    leds[i].blink(1000, 150, -1);
  }
  lastMlls = millis();
  brightCounter = 500;
}

void loop() {
  midi.update();

  int mls = millis();
  int deltat = mls - lastMlls;
  lastMlls = mls;
  if(brightCounter > 0){
    brightCounter -= deltat;
    if(brightCounter <= 0){
      currBrightness++;
      brightCounter = 500;
      if(currBrightness > 10){
        currBrightness = 0;
      }
      for(int i=0; i<5; i++){
        leds[i].setBrightness(currBrightness*1);
      }
    }
  }
  for(int i=0; i<5; i++){
    leds[i].update();
  }    
}
