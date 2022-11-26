// ++++++++++++++++++++++++++
//
// Color Explorer
// Jonathan Moyes 2022
//
// ++++++++++++++++++++++++++
//
// DESCRIPTION
//
// Firmware to support the Color Explorer Board. The board hosts 3 sliding potentiometers,
// 4 Neopixel LEDs, and a pin jumper. Three of the LEDs are associated with each of the sliders.
// Adjusting one of the potentiometers adjusts the brightness of it's associated color (red slider up -> Red LED brighter)
// If the pin jumper is installed, the fourth LED displays the summation of the currently configured constituent colors.
// For example, with all three sliders at their maximum position, you'll see a bright white light from the fourth "Mixed Light" LED.
// Removing the jumper extinguishes the mixed light LED. 
//
// ++++++++++++++++++++++++++ 
//
// BUILDING
//
// This was written to work in Arduino IDE.
//
// You'll need the ATtiny85 Arduino Board support package. Add the following in a new row inside "Additional Boards Manager URLs," 
// found in Preferences inside Arduino IDE:
//  
//      https://raw.githubusercontent.com/damellis/attiny/ide-1.6.x-boards-manager/package_damellis_attiny_index.json
//
// Configure the Tools menu as follows:
//
// Board -> ATtiny Microcontrollers -> ATtiny25/45/85
// Processor: ATtiny85
// Clock: Internal 16MHz
// Programmer: [Your ISCP programmer - AVRISP, TinyUSB, ArduinoISP, etc]
//
// You'll also need a the Neopixel library: 
// 1. From the Sketch Menu, Include Library -> Manage Libraries 
// 2. Search for, and install "Adafruit NeoPixel"/
//
// ++++++++++++++++++++++++++ 
//
// INITIAL PROGRAMMING
//
// You need to set the fuses to what the Arduino IDE is expectiing. Ensure that you have "Internal 16MHz" selected in the 
// Clock configuration (as above), and have your programmer type selected in the Programmer configration (as above). 
// Then, go to Sketch-> Burn Bootloader. This will set the fuses on your microcontroller. Once this is done, you can use
// the "Upload" button in Arduino IDE to upload the code.
//  
// ++++++++++++++++++++++++++
//
// RESOURCES
//
//   1. ATTINY85 Datasheet: https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2586-AVR-8-bit-Microcontroller-ATtiny25-ATtiny45-ATtiny85_Datasheet.pdf
//   2. NeoPixel Library: https://github.com/adafruit/Adafruit_NeoPixel
//
// ++++++++++++++++++++++++++
//
// TODO
//
//   1. Find off-by-one bug: sliders are mismatched with LEDs when properly configured. Seems related to order of analogRead()'s 
//          Is mixing not working the way I think it is?
//   2. The ATTiny85 should be able to do 10-bit ADC, which would help with the significant discretisation error when scaling the 8-bit 
//          ADC values to feed them into the Neopixels, which, unscaled, have 8-bit resolution. That is, the LEDs have much
//          higher resolution than we're taking advantage of when scaling for MAX_BRIGHTNESS
//   3. Use internal pullup on MIX_ENABLE pin. v1.2 hardware will drop the external.
//   4. Can we remove/reduce the delay between ADC MUX selection, and starting the conversion? Might be a development crutch that can be removed.
//   5. Don't overwrite the entire ADMUX register inside get_desired_brightness_by_channel(), only modify MUX[0-3] via bitwise op.
//   6. Improve init_adc() with bitwise ops to improve readability.

// ++++++++++++++++++++++++++


#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h>
#endif

// Neopixel Configuration
#define NEOPIXEL_PIN 0  // PB0
#define NUM_PIXELS   4
#define MAX_BRIGHTNESS  25 // (254 max). Slider values will be scaled to this max value|}

// Define the order of colors in the Neopixel chain
#define LED_INDEX_BLUE 0
#define LED_INDEX_GREEN 1
#define LED_INDEX_RED 2
#define LED_INDEX_MIX 3

// What pin to use for Mixed Light LED enable/disable?
#define PIN_MIX_ENABLE 1 // PB1

// !!! BUG WORKAROUND: These don't match the schematic, and I'm not (yet) sure where the 
// off-by-one error is coming in. But this works for now for basic bring-up.
#define ADC_CHANNEL_BLUE  2 // 1 in schematic
#define ADC_CHANNEL_GREEN 1 // 3 in schematic
#define ADC_CHANNEL_RED   3 // 2 in schematic

// Adafruit's NeoPixel library is awesome!
Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Declare a place to store the brightness values for the three constituent colors for use in mixing
uint8_t desired_brightness_G;
uint8_t desired_brightness_R;
uint8_t desired_brightness_B;

void setup() {
  init_neopixels();
  init_adc();
}

void loop() {

  get_desired_brightness_by_channel(ADC_CHANNEL_RED, &desired_brightness_R);
  pixels.setPixelColor(LED_INDEX_RED, pixels.Color(desired_brightness_R, 0, 0));
  
  get_desired_brightness_by_channel(ADC_CHANNEL_GREEN, &desired_brightness_G);
  pixels.setPixelColor(LED_INDEX_GREEN, pixels.Color(0, desired_brightness_G, 0));

  get_desired_brightness_by_channel(ADC_CHANNEL_BLUE, &desired_brightnessnalog_B);
  pixels.setPixelColor(LED_INDEX_BLUE, pixels.Color(0, 0, desired_brightness_B));

  if ( digitalRead(PIN_MIX_ENABLE) == LOW ){
    pixels.setPixelColor(LED_INDEX_MIX, pixels.Color(analogData_R, analogData_G, analogData_B));
  }
  else{
    pixels.setPixelColor(LED_INDEX_MIX, pixels.Color(0, 0, 0));
  }
  
  pixels.show(); 
  delay(1); // Some breathing time for the micro - no need to hammer it.
}

void init_neopixels(){

  // Set prescaler for compatibility with 16MHz ATTINY85
  clock_prescale_set(clock_div_1); 

  // Initialize the pixel chain and turn off any lights that might be on
  pixels.begin();
  pixels.clear();
  pixels.show(); 
}

void init_adc(){

  // REFS[1:0]: 0b00 to select VCC (REFS2 don't care)
  // ADLAR = 0b1 to Left adjust results
  ADMUX  = 0b00100000;  // Use VCC as Vref, input ADC0 (not important), left adjust

  // ADEN = 0b1 to enable ADC
  // ADSC = 0b0 to use single shot mode
  // ADPS[2:0] - Prescaler 8 for 125KHz ADC
  //   ADPS0 = 0b0
  //   ADPS1 = 0b1
  //   ADPS2 = 0b1
  ADCSRA = 0b10000011;  // Enable ADC, ADC single-shot mode, clock div 8 = 125kHz ADC clock
}

void get_desired_brightness_by_channel(uint8_t channel, uint8_t *destination){

  // TODO: Change to Bitwise to manipulate only MUX[0:3], and 
  
  switch( channel ){
    case 0:
      // 0bxxxx0000 = ADC0
      ADMUX = 0b00100000;
      break;
      
    case 1:
      // 0bxxxx0001 = ADC1
      ADMUX = 0b00100001;
      break;
      
    case 2:
      // 0bxxxx0010 = ADC2
      ADMUX = 0b00100010;
      break;
      
    case 3:
      // 0bxxxx0011 = ADC3
      ADMUX = 0b00100011;
      break;
     
    default:
      // channel not supported
      return;
  }

  delay(20); // Do we need this?
  
  ADCSRA |= (1 << ADSC); // Starts a conversion

  // Scale value for configured max brightness, and invert direction
  *destination = map(ADCH, 0, 254, MAX_BRIGHTNESS, 0);
 
}
