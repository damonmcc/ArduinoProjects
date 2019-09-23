// LEDs for capacitive Coffee timer
// Damon McCullough 2018

#include <FastLED.h>
#define LED_PIN     4 // Digital pin used for LED strip
#define COLOR_ORDER GRB
#define CHIPSET     WS2811  // Individual "Neopixels"
#define NUM_LEDS    5

#define BRIGHTNESS  10  // Max 255
#define FRAMES_PER_SECOND 5

bool gReverseDirection = false;

#define TOUCH_PIN     8 // Digital pin used for capacitice touch
uint8_t touchValue = 0;
uint8_t touchSum = 0;
uint32_t touchPress = 0;
#define TOUCH_MAX     3
// These are used for 'debouncing' the button inputs
#define  DEBOUNCE_MS 1000 // Button debounce time, in milliseconds
boolean TouchState, prevStateTouch;
uint8_t lasttChangeTimeTouch = 0;

uint8_t menuView = 0;         // int to select distinct menu
unsigned long tMenuNow, tMenuNext; // ints to hold current time and time of next menu event
unsigned long menuCounter = 5000;  // milis before moving on in menu

uint8_t alarmLength = 0;      // current length of alarm countdown 

CRGB leds[NUM_LEDS];
uint32_t countdownColor;

#include "pitches.h"
// notes in Hedwig's Theme:
// source: https://pianoletternotes.blogspot.com/2015/11/hedwigs-theme-harry-potter.html
int melodyHT[] = {
  NOTE_B4,
  NOTE_E4, NOTE_G4, NOTE_FS4,
  NOTE_E4, NOTE_B4,
  NOTE_A4,
  NOTE_FS4,
  
  NOTE_E4, NOTE_G4, NOTE_FS4,
  NOTE_D4, NOTE_F4,
  NOTE_B3,
};
// note durations: 400 = quarter note, 800 = eighth note, etc.:
int noteDurationsHT[] = {
  800,
  570, 1600, 800,
  400, 800,
  264,
  264,
  
  570, 1600, 800,
  400, 800,
  264
};
const int lengthHT = sizeof(melodyHT) / sizeof(melodyHT[0]);



// GLOBAL VARIABLES --------------------------------------------------------

// This bizarre construct isn't Arduino code in the conventional sense.
// It exploits features of GCC's preprocessor to generate a PROGMEM
// table (in flash memory) holding an 8-bit unsigned sine wave (0-255).
const int _SBASE_ = __COUNTER__ + 1; // Index of 1st __COUNTER__ ref below
#define _S1_ (sin((__COUNTER__ - _SBASE_) / 128.0 * M_PI) + 1.0) * 127.5 + 0.5,
#define _S2_ _S1_ _S1_ _S1_ _S1_ _S1_ _S1_ _S1_ _S1_ // Expands to 8 items
#define _S3_ _S2_ _S2_ _S2_ _S2_ _S2_ _S2_ _S2_ _S2_ // Expands to 64 items
const uint8_t PROGMEM sineTable[] = { _S3_ _S3_ _S3_ _S3_ }; // 256 items

// Similar to above, but for an 8-bit gamma-correction table.
#define _GAMMA_ 2.6
const int _GBASE_ = __COUNTER__ + 1; // Index of 1st __COUNTER__ ref below
#define _G1_ pow((__COUNTER__ - _GBASE_) / 255.0, _GAMMA_) * 255.0 + 0.5,
#define _G2_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ // Expands to 8 items
#define _G3_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ // Expands to 64 items
const uint8_t PROGMEM gammaTable[] = { _G3_ _G3_ _G3_ _G3_ }; // 256 items

// These are used in estimating (very approximately) the current draw of
// the board and NeoPixels.  BASECURRENT is the MINIMUM current (in mA)
// used by the entire system (microcontroller board plus NeoPixels) --
// keep in mind that even when "off," NeoPixels use a tiny amount of
// current (a bit under 1 milliamp each).  LEDCURRENT is the maximum
// additional current PER PRIMARY COLOR of ONE NeoPixel -- total current
// for an RGB NeoPixel could be up to 3X this.  The '3535' NeoPixels on
// Circuit Playground are smaller and use less current than the more
// common '5050' type used in NeoPixel strips and shapes.
#define BASECURRENT 10
#define LEDCURRENT  11 // Try using 18 for '5050' NeoPixels
uint32_t  frame = 0;    // Frame count, results displayed every 256 frames
uint32_t sum   = 0;    // Cumulative current, for calculating average
//uint8_t *pixelPtr;     // -> NeoPixel color data

// This array lists each of the display/animation drawing functions
// (which appear later in this code) in the order they're selected with
// the right button.  Some functions appear repeatedly...for example,
// we return to "mode_off" at several points in the sequence.
void (*renderFunc[])(void) {
  mode_off, // Starts here, with LEDs off
  mode_white_max, mode_white_half_duty      , mode_off,
  mode_white_max, mode_white_half_perceptual, mode_off,          // 4, *5*, 6
  mode_primaries, mode_colorwheel, mode_colorwheel_gamma,        // 7, 8, 9
  mode_half, mode_sparkle,                                       // 10, 11
  mode_marquee, mode_sine, mode_sine_gamma, mode_sine_half_gamma, // 12, *13*, 14, 15
  mode_coffee
};
#define N_MODES (sizeof(renderFunc) / sizeof(renderFunc[0]))


uint8_t mode = 16; // Index of current mode in table

// SETUP FUNCTION -- RUNS ONCE AT PROGRAM START ----------------------------

void setup() {
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );
  pinMode(13, OUTPUT);

  Serial.begin(19200);
  uint32_t sun = pgm_read_byte(&gammaTable[127]) * 0xC35831;
  uint32_t sky = pgm_read_byte(&gammaTable[127]) * 0xC1876B;
  countdownColor = pgm_read_byte(&gammaTable[127]) * 0x00FF00;
  tMenuNext = tMenuNow + menuCounter;
  
  static uint8_t riverS = 0;
  static uint8_t riverE = 5;
  
  Serial.print("sun: ");
  Serial.println(sun);
}

// LOOP FUNCTION -- RUNS OVER AND OVER FOREVER -----------------------------

void loop() {
  
//  leds[0] = CRGB(0xC35831);  // Custom
//  leds[1] = CRGB(0xC1876B);
//  leds[2] = CRGB(0xC1876B);
//  leds[0] = CRGB(0xFD0101);   // NPR
//  leds[1] = CRGB(0xFF00FF);
//  leds[2] = CRGB(0xFF00FF);
  
  touchValue = readCapacitivePin(TOUCH_PIN);
//  Serial.println(touchValue);
  // Read and debounce inouts
  uint32_t tNow = millis();
  if((tNow - lasttChangeTimeTouch) >= DEBOUNCE_MS) {
    boolean b = touchValue > 2;
    if(b != prevStateTouch) {            // Touch button state changed
      prevStateTouch      = b;
      lasttChangeTimeTouch = tNow;
      if(b) {                            // Touch button pressed
        digitalWrite(13, HIGH);
//        Serial.print("Pressed @");
//        Serial.print(touchPress);
//        Serial.print(" Now ");
//        Serial.println(touchPress + 1);
//        if(touchPress > TOUCH_MAX){
//          Serial.print("OW!!!!!");
//        }
        touchPress++;
      } else{digitalWrite(13, LOW);}
    }
  }

  
  (*renderFunc[mode])();          // Render one frame in current mode
  FastLED.show(); // and update the NeoPixels to show it
  frame++;
  coffeeClock();
//  Serial.println(frame);

  if(frame>265) { // Every 256th frame, estimate & print current
    Serial.print("Press #");
    Serial.print(touchPress);
    Serial.print(", Alarm: ");
    Serial.print(alarmLength);

    long alarmLeft = tMenuNext - millis();
    Serial.print(" _ Alarm time: ");
    Serial.print(tMenuNext);
    
    Serial.print(", left: ");
    Serial.println(alarmLeft);
    frame = 0;
  }
}

// RENDERING FUNCTIONS FOR EACH DISPLAY/ANIMATION MODE ---------------------


// Mode for coffee timer
void mode_coffee() {
  uint32_t t = millis() / FRAMES_PER_SECOND;
  tMenuNow = millis();
  FastLED.clear();
  
  switch (menuView){
    case 0:
      // Cool
//      mode_sparkle()
      // Do random sparkle
      static uint8_t randomPixel = 0;
      if(!(frame & 0x7F)) {              // Every 128 frames...
        uint8_t r;
        do {
          r = random(NUM_LEDS);                // Pick a new random pixel
        } while(r == randomPixel);       // but not the same as last time
        randomPixel = r;                 // Save new random pixel index
        leds[randomPixel] = CRGB(0x0000FF);
      }
      break;
      
    case 1:
      // Set-in  
      if(touchPress > 0){     // If desired alarm length is > 0
        if(touchPress <= NUM_LEDS){ // If desired alarm is within LED limit
          for(uint8_t i=0; i < touchPress; i++) {
            // Fading sin wave along current alarm length
            uint8_t j = i + (i > 4);
            uint8_t k = pgm_read_byte(&sineTable[(t + j * 512 / 12) & 0xFF]) / 2;
            k = pgm_read_byte(&gammaTable[k]);
    //        leds[i] = CRGB(k * 0x000100L);
    //        leds[i] = CRGB(k * 0x000001L);
            leds[i] = CRGB(k * 0x00F0F00);
    //        sun:  0xC1876B
            }
        }
        else{             // Went over the limit in choosing alarm length
          touchPress = 0; // Reset alarm count
          alarmLength = 0; 
        }
      }
      else{             // Set-in with an empty alarm length
        Serial.print("coolling_");
      }
      break;
      
    
    case 2:
      // Countdown
      // Glow all leds TODO fade as minute ticks down
      for(uint8_t i=0; i<alarmLength; i++) {
        leds[i] = CRGB(countdownColor);
      }
      break;
      
    case 3:
      // Alarm
      for(uint8_t i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB(0x7F7F7F);
      }
//      playMelody(melodyHT, noteDurationsHT, lengthHT);
      break;
  }

//  for(uint8_t i=riverS; i<riverE + 1; i++) {
//    uint8_t j = i + (i > 10);
//    uint8_t k = pgm_read_byte(&sineTable[(t + j * 512 / 12) & 0xFF]);
//    k = pgm_read_byte(&gammaTable[k]);
////    leds[i] = CRGB(k * 0x000100L);
//    leds[i] = CRGB(k * 0x010001L);
//  }
}


void coffeeClock(){
  //  Execute menu switching
  if(menuView == 0){      // in cool
    if(touchPress > 0){   // if tapped..
      // Go to set-in
      menuView = 1;
      touchPress = 1;
      // Reset set-in timer
      tMenuNow = millis();
      tMenuNext = tMenuNow + menuCounter;
    }
  }
  else if(menuView == 1){  // while in set-in
    tMenuNow = millis();
    if(tMenuNow<tMenuNext){
//      Serial.println("setting.");
      //      nothing and listening for touch to add to alarm
      alarmLength = touchPress;       // Set Alarm length
    }
    else{
      Serial.println("Finished set-in.");
      if(touchPress>0){   // If a time was chosen
        // Go to countdown
        menuView = 2;
        alarmLength = touchPress;       // Set Alarm length
        
        tMenuNow = millis();
        tMenuNext = tMenuNow + ((long)alarmLength*1000*60);  // Prepare Alarm timer
        
        Serial.print("from set-in to countdown, alarm:");
        Serial.println(alarmLength);
        Serial.print("tMenuNow: ");
        Serial.print(tMenuNow);
        Serial.print(" _ tMenuNext: ");
        Serial.println(tMenuNext);
      } else{
        // Time not chosen 
        // Cooling down
        Serial.println("from set-in to cool.");
        alarmLength = touchPress;       // Set Alarm length
        menuView = 0;
      }
    }
  }
  else if(menuView == 2){   // in countdown
    tMenuNow = millis();
    if(alarmLength != touchPress){ // If tapped after alarm was set
      // Go back to set-in with new time
      Serial.println("from countdown to set-in.");
      menuView = 1;
      // Reset set-in timer
      tMenuNext = tMenuNow + menuCounter;
    } else{
      // Countdown to alarm
      if(tMenuNow < tMenuNext){
          // TODO: make more accurate
//         if((tMenuNext - tMenuNow)%(long)(60*1000) < 10 && alarmLength > 1){
//          Serial.println("Hit minute mark");  // if 60*1000 above
//          touchPress -= 1;
//          alarmLength -= 1; 
//          tMenuNext = tMenuNow + ((long)alarmLength*1000*60);  // Prepare Alarm timer
//         }
      }
      else{
        // Ring alarm!
        Serial.println("from countdown to alarm.");
        menuView = 3;
      }
    }
  }
  
  else if(menuView == 3){ // Alarming
    if(alarmLength != touchPress){ // If tapped after alarm goes off..
      Serial.println("from Alarm to Cool.");
      menuView = 0;
      touchPress = 0;
      alarmLength = 0;
    }
    else{
     Serial.print("Alarming!!");
    }
  }
  
  else{   
    Serial.println("UNKNOWN MENU");
  }
}

 // readCapacitivePin
//  Input: Arduino pin number
//  Output: A number, from 0 to 17 expressing
//  how much capacitance is on the pin
//  When you touch the pin, or whatever you have
//  attached to it, the number will get higher
#include "pins_arduino.h" // Arduino pre-1.0 needs this
uint8_t readCapacitivePin(int pinToMeasure) {
  // Variables used to translate from Arduino to AVR pin naming
  volatile uint8_t* port;
  volatile uint8_t* ddr;
  volatile uint8_t* pin;
  // Here we translate the input pin number from
  //  Arduino pin number to the AVR PORT, PIN, DDR,
  //  and which bit of those registers we care about.
  byte bitmask;
  port = portOutputRegister(digitalPinToPort(pinToMeasure));
  ddr = portModeRegister(digitalPinToPort(pinToMeasure));
  bitmask = digitalPinToBitMask(pinToMeasure);
  pin = portInputRegister(digitalPinToPort(pinToMeasure));
  // Discharge the pin first by setting it low and output
  *port &= ~(bitmask);
  *ddr  |= bitmask;
  delay(1);
  uint8_t SREG_old = SREG; //back up the AVR Status Register
  // Prevent the timer IRQ from disturbing our measurement
  noInterrupts();
  // Make the pin an input with the internal pull-up on
  *ddr &= ~(bitmask);
  *port |= bitmask;

  // Now see how long the pin to get pulled up. This manual unrolling of the loop
  // decreases the number of hardware cycles between each read of the pin,
  // thus increasing sensitivity.
  uint8_t cycles = 17;
  if (*pin & bitmask) { cycles =  0;}
  else if (*pin & bitmask) { cycles =  1;}
  else if (*pin & bitmask) { cycles =  2;}
  else if (*pin & bitmask) { cycles =  3;}
  else if (*pin & bitmask) { cycles =  4;}
  else if (*pin & bitmask) { cycles =  5;}
  else if (*pin & bitmask) { cycles =  6;}
  else if (*pin & bitmask) { cycles =  7;}
  else if (*pin & bitmask) { cycles =  8;}
  else if (*pin & bitmask) { cycles =  9;}
  else if (*pin & bitmask) { cycles = 10;}
  else if (*pin & bitmask) { cycles = 11;}
  else if (*pin & bitmask) { cycles = 12;}
  else if (*pin & bitmask) { cycles = 13;}
  else if (*pin & bitmask) { cycles = 14;}
  else if (*pin & bitmask) { cycles = 15;}
  else if (*pin & bitmask) { cycles = 16;}

  // End of timing-critical section; 
  // turn interrupts back on if they were on before, 
  // or leave them off if they were off before
  SREG = SREG_old;

  // Discharge the pin again by setting it low and output
  //  It's important to leave the pins low if you want to 
  //  be able to touch more than 1 sensor at a time - if
  //  the sensor is left pulled high, when you touch
  //  two sensors, your body will transfer the charge between
  //  sensors.
  *port &= ~(bitmask);
  *ddr  |= bitmask;

  return cycles;
}


void playMelody(int melody[], int melodyDurations[], int melodyLength){
  /*
   * Plays an array of notes of given durations and total note count
   */
  for (int thisNote = 0; thisNote < melodyLength; thisNote++) {
    // to calculate the note duration, take one second (or anything)
    // divided by the note type.
    // e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 150000 / melodyDurations[thisNote];
    tone(8, melody[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    noTone(8);
  }
}




// All NeoPixels off
void mode_off() {
  FastLED.clear();
}

// All NeoPixels on at max: white (R+G+B) at 100% duty cycle
void mode_white_max() {
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    leds[i] = CRGB(0xFFFFFF);
  }
}

// All NeoPixels on at 50% duty cycle white.  Numerically speaking,
// this is half power, but perceptually it appears brighter than 50%.
void mode_white_half_duty() {
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    leds[i] = CRGB(0x7F7F7F);
  }
}

// All NeoPixels on at 50% perceptial brightness, using gamma table lookup.
// Though it visually appears to be about half brightness, numerically the
// duty cycle is much less, a bit under 20% -- meaning "half brightness"
// can actually be using 1/5 the power!
void mode_white_half_perceptual() {
  uint32_t c = pgm_read_byte(&gammaTable[127]) * 0x010101;
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    leds[i] = CRGB(c);
  }
}

// Cycle through primary colors (red, green, blue), full brightness.
// Because only a single primary color within each NeoPixel is turned on
// at any given time, this uses 1/3 the power of the "white max" mode.
void mode_primaries() {
  uint32_t c;
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    // This animation (and many of the rest) pretend spatially that there's
    // 12 equally-spaced NeoPixels, though in reality there's only 10 with
    // gaps at the USB and battery connectors.
    uint8_t j = i + (i > 4);     // Mind the gap
    j = ((millis() / 100) + j) % 12;
    if(j < 4)      c = 0xFF0000; // Bed
    else if(j < 8) c = 0x00FF00; // Green
    else           c = 0x0000FF; // Blue
    leds[i] = CRGB(c);
  }
}

// HSV (hue-saturation-value) to RGB function used for the next two modes.
uint32_t hsv2rgb(int32_t h, uint8_t s, uint8_t v, boolean gc=false) {
  uint8_t n, r, g, b;

  // Hue circle = 0 to 1530 (NOT 1536!)
  h %= 1530;           // -1529 to +1529
  if(h < 0) h += 1530; //     0 to +1529
  n  = h % 255;        // Angle within sextant; 0 to 254 (NOT 255!)
  switch(h / 255) {    // Sextant number; 0 to 5
   case 0 : r = 255    ; g =   n    ; b =   0    ; break; // R to Y
   case 1 : r = 254 - n; g = 255    ; b =   0    ; break; // Y to G
   case 2 : r =   0    ; g = 255    ; b =   n    ; break; // G to C
   case 3 : r =   0    ; g = 254 - n; b = 255    ; break; // C to B
   case 4 : r =   n    ; g =   0    ; b = 255    ; break; // B to M
   default: r = 255    ; g =   0    ; b = 254 - n; break; // M to R
  }

  uint32_t v1 =   1 + v; // 1 to 256; allows >>8 instead of /255
  uint16_t s1 =   1 + s; // 1 to 256; same reason
  uint8_t  s2 = 255 - s; // 255 to 0

  r = ((((r * s1) >> 8) + s2) * v1) >> 8;
  g = ((((g * s1) >> 8) + s2) * v1) >> 8;
  b = ((((b * s1) >> 8) + s2) * v1) >> 8;
  if(gc) { // Gamma correct?
    r = pgm_read_byte(&gammaTable[r]);
    g = pgm_read_byte(&gammaTable[g]);
    b = pgm_read_byte(&gammaTable[b]);
  }
  return ((uint32_t)r << 16) | ((uint16_t)g << 8) | b;
}

// Rotating color wheel, using 'raw' RGB values (no gamma correction).
// Average current use is about 1/2 of the max-all-white case.
void mode_colorwheel() {
  uint32_t t = millis();
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    uint8_t j = i + (i > 4);
    leds[i] = CRGB(
      hsv2rgb(t + j * 1530 / 12, 255, 255, false));
  }
}

// Color wheel using gamma-corrected values.  Current use is slightly less
// than the 'raw' case, but not tremendously so, as only 1/3 of pixels at
// any time are in transition cases (else 100% on or off).
void mode_colorwheel_gamma() {
  uint32_t t = millis();
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    uint8_t j = i + (i > 4);
    leds[i] = CRGB(
      hsv2rgb(t + j * 1530 / 12, 255, 255, true));
  }
}

// Cycle with half the pixels on, half off at any given time.
// Simple idea.  Half the pixels means half the power use.
void mode_half() {
  uint32_t t = millis() / 4;
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    uint8_t j = t + i * 256 / 10;
    j = (j >> 7) * 255;
    leds[i] = CRGB(j * 0x010000);
  }
}

// Blue sparkles.  Randomly turns on ONE pixel at a time.  This demonstrates
// minimal power use while still doing something "catchy."  And because it's
// a primary color, power use is even minimal-er (see 'primaries' above).
void mode_sparkle() {
  static uint8_t randomPixel = 0;
  if(!(frame & 0x7F)) {              // Every 128 frames...
    FastLED.clear(); // Clear pixels
    uint8_t r;
    do {
      r = random(NUM_LEDS);                // Pick a new random pixel
    } while(r == randomPixel);       // but not the same as last time
    randomPixel = r;                 // Save new random pixel index
    leds[randomPixel] = CRGB(0x0000FF);
  }
}

// Simple on-or-off "marquee" animation w/ about 50% of pixels lit at once.
// Not much different than the 'half' animation, but provides a conceptual
// transition into the examples that follow.
void mode_marquee() {
  uint32_t t = millis() / 4;
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    uint8_t j = i + (i > 4);
    uint8_t k = (t + j * 256 / 12) & 0xFF;
    k = ((k >> 6) & 1) * 255;
    leds[i] = CRGB(k * 0x000100L);
  }
}

// Sine wave marquee, no gamma correction.  Avg. overall duty cycle is 50%,
// and combined with being a primary color, uses about 1/6 the max current.
void mode_sine() {
  uint32_t t = millis() / 4;
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    uint8_t j = i + (i > 4);
    uint8_t k = pgm_read_byte(&sineTable[(t + j * 512 / 12) & 0xFF]);
    leds[i] = CRGB(k * 0x000100L);
  }
}

// Sine wave with gamma correction.  Because nearly all the pixels have
// "in-between" values (not 100% on or off), there's considerable power
// savings to gamma correction, in addition to looking more "correct."
void mode_sine_gamma() {
  uint32_t t = millis() / 4;
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    uint8_t j = i + (i > 4);
    uint8_t k = pgm_read_byte(&sineTable[(t + j * 512 / 12) & 0xFF]);
    k = pgm_read_byte(&gammaTable[k]);
    leds[i] = CRGB(k * 0x000100L);
  }
}

// Perceptually half-brightness gamma-corrected sine wave.  Sometimes you
// don't need things going to peak brightness all the time.  Combined with
// gamma and primary color use, it's super effective!
void mode_sine_half_gamma() {
  uint32_t t = millis() / 4;
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    uint8_t j = i + (i > 4);
    uint8_t k = pgm_read_byte(&sineTable[(t + j * 512 / 12) & 0xFF]) / 2;
    k = pgm_read_byte(&gammaTable[k]);
//    leds[i] = CRGB(k * 0x000100L);
    leds[i] = CRGB(k * 0x000001L);
  }
}



