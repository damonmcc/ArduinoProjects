// Sketch to accompany "Sipping Power With NeoPixels" guide.  Designed for
// Adafruit Circuit Playground and adapted to FastLED.

#include <FastLED.h>

#define LED_PIN     2 // Digital pin used for LED strip
#define COLOR_ORDER GRB
#define CHIPSET     WS2811  // Type of LED strip
#define NUM_LEDS    30

#define BRIGHTNESS  100
#define FRAMES_PER_SECOND 60

bool gReverseDirection = false;

CRGB leds[NUM_LEDS];

// The 32bit version of our coordinates
static uint16_t x;
static uint16_t z;

// We're using the x dimension for "time".  speed determines how fast time moves
// forward.  Try 1 for a very slow moving effect,
// or 60 for something that ends up looking like water.
// uint16_t speed = 1; // almost looks like a painting, moves very slowly
uint16_t speed = 1; // a nice starting speed, mixes well with a scale of 100
// uint16_t speed = 33;
// uint16_t speed = 100; // wicked fast!

// Scale determines how far apart the pixels in our noise matrix are.  Try
// changing these values around to see how it affects the motion of the display.  The
// higher the value of scale, the more "zoomed out" the noise iwll be.  A value
// of 1 will be so zoomed in, you'll mostly see solid colors.

// uint16_t scale = 1; // mostly just solid colors
// uint16_t scale = 4011; // very zoomed out and shimmery
uint16_t scale = 2011;
// This is the array that we keep our computed noise values in
#define MAX_DIMENSION NUM_LEDS
uint8_t noise[MAX_DIMENSION];

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

// These are used for 'debouncing' the left & right button inputs,
// for switching between modes.
boolean  prevStateLeft, prevStateRight;
uint32_t lastChangeTimeLeft = 0, lastChangeTimeRight = 0;
#define  DEBOUNCE_MS 15 // Button debounce time, in milliseconds

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
uint8_t  frame = 0;    // Frame count, results displayed every 256 frames
uint32_t sum   = 0;    // Cumulative current, for calculating average
//uint8_t *pixelPtr;     // -> NeoPixel color data

// This array lists each of the display/animation drawing functions
// (which appear later in this code) in the order they're selected with
// the right button.  Some functions appear repeatedly...for example,
// we return to "mode_off" at several points in the sequence.
void (*renderFunc[])(void) {
  mode_off, // Starts here, with LEDs off
  mode_white_max, mode_white_half_duty      , mode_off,
  mode_white_max, mode_white_half_perceptual, mode_off,
  mode_primaries, mode_colorwheel, mode_colorwheel_gamma,
  mode_half, mode_sparkle,
  mode_marquee, mode_sine, mode_sine_gamma, // 12, 13, 14
  mode_sine_gamma_noise, mode_white_noise, mode_sine_half_gamma // 15, 16, 17
};
#define N_MODES (sizeof(renderFunc) / sizeof(renderFunc[0]))
uint8_t mode = 17; // Index of current mode in table

// SETUP FUNCTION -- RUNS ONCE AT PROGRAM START ----------------------------

void setup() {
//  CircuitPlayground.begin();
//  CircuitPlayground.setBrightness(255); // NeoPixels at full brightness
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );
  // Initialize our coordinates to some random values
  x = random16();
//  pixelPtr = leds;

  Serial.begin(19200);

  //prevStateLeft  = CircuitPlayground.leftButton(); // Initial button states
  //prevStateRight = CircuitPlayground.rightButton();
}

// LOOP FUNCTION -- RUNS OVER AND OVER FOREVER -----------------------------

void loop() {
/*
  // Read and debounce left/right buttons
  uint32_t t = millis();
  if((t - lastChangeTimeLeft) >= DEBOUNCE_MS) {
    boolean b = CircuitPlayground.leftButton();
    if(b != prevStateLeft) {             // Left button state changed?
      prevStateLeft      = b;
      lastChangeTimeLeft = t;
      if(b) {                            // Left button pressed?
        if(mode) mode--;                 // Go to prior mode
        else     mode = N_MODES - 1;     // or "wrap around" to last mode
        frame = sum = 0;                 // Reset power calculation
      }
    }
  }
  if((t - lastChangeTimeRight) >= DEBOUNCE_MS) {
    boolean b = CircuitPlayground.rightButton();
    if(b != prevStateRight) {            // Right button state changed?
      prevStateRight      = b;
      lastChangeTimeRight = t;
      if(b) {                            // Right button pressed?
        if(mode < (N_MODES-1)) mode++;   // Advance to next mode
        else                   mode = 0; // or "wrap around" to start
        frame = sum = 0;                 // Reset power calc
      }
    }
  }
*/
  (*renderFunc[mode])();          // Render one frame in current mode
  FastLED.show(); // and update the NeoPixels to show it

//  // Accumulate total brightness value for all NeoPixels (assumes RGB).
//  for(uint8_t i=0; i<NUM_LEDS * 3; i++) {
//    sum += leds[i];
//  }
//  if(!++frame) { // Every 256th frame, estimate & print current
//    Serial.print(BASECURRENT + (sum * LEDCURRENT + 32640) / 65280);
//    Serial.println(" mA");
//    sum = 0; // Reset pixel accumulator
//  }
}

// RENDERING FUNCTIONS FOR EACH DISPLAY/ANIMATION MODE ---------------------

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
      r = random(10);                // Pick a new random pixel
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


// Fill the x/y array of 8-bit noise values using the inoise8 function.
void fillnoise8() {
  for(int i = 0; i < MAX_DIMENSION; i++) {
    int ioffset = scale * i;
    noise[i] = inoise8(x + ioffset, z);
  }
  z += speed;
}

// Sine wave with gamma correction and 1-D Perlin Simplex Noise
// added to led brightness
void mode_sine_gamma_noise() {
  uint32_t t = millis() / 4;
  for(uint8_t i=0; i<NUM_LEDS; i++) {
    uint8_t j = i + (i > 4);
    uint8_t k = pgm_read_byte(&sineTable[(t + j * 512 / 12) & 0xFF]);
    k = pgm_read_byte(&gammaTable[k]);
    leds[i] = CRGB(k * 0x000100L);
    leds[i].addToRGB(noise[i]);
  }
}

// All NeoPixels on at max: white (R+G+B) at 100% duty cycle
void mode_white_noise() {
  static uint8_t ihue=0;
  fillnoise8();
  for(int i = 0; i < NUM_LEDS; i++) {
    // We use the value at the (i,j) coordinate in the noise
    // array for our brightness, and the flipped value from (j,i)
    // for our pixel's hue.
    leds[i] = CHSV(noise[i],255,noise[i]);

    // You can also explore other ways to constrain the hue used, like below
//    leds[i] = CHSV(ihue + (noise[i]>>2),255,noise[i]);
  }
  ihue+=1;
//  for(uint8_t i=0; i<NUM_LEDS; i++) {
//    leds[i] = CRGB(0x7F7F7F);
//    leds[i].addToRGB(noise[i]);
//  }
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
    leds[i] = CRGB(k * 0x000100L);
  }
}
