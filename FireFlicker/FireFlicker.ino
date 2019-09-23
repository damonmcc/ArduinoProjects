#include <FastLED.h>

#define LED_PIN     2
#define COLOR_ORDER GRB
#define CHIPSET     WS2811
#define NUM_LEDS    30

#define BRIGHTNESS  200
#define FRAMES_PER_SECOND 60

bool gReverseDirection = false;

CRGB leds[NUM_LEDS];

void setup() {
  delay(3000); // sanity delay
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );
}

void loop() {
  FireFlicker(); // run simulation frame
  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}



void FireFlicker(){
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];
  int r = 255;
  int g = r-40;
  int b = 40;
  
  // computer flickers and add to rbg values
  for( int j = 0; j < NUM_LEDS; j++) {
    int pixelnumber;
    int flicker = random(0,50);
    int r1 = r-flicker;
    int g1 = g-flicker;
    int b1 = b-flicker;
//    print(r1+g1+b1)
    CRGB color = CRGB(r1, g1, b1);
    if( gReverseDirection ) {
      pixelnumber = (NUM_LEDS-1) - j;
    } else {
      pixelnumber = j;
    }
    leds[pixelnumber] = color;
  }

}

