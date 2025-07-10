/**
 * Originally written by Skogaby
 * Modified by SpeedyPotato
 * Modified again by Stepdog/dumcatt
 * Adapted for an IIDX effector panel
 */

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

#include <Bounce2.h>
#include <FastLED.h>
#include <Joystick.h>
#include <ResponsiveAnalogRead.h>
#include "PluggableUSB.h"
#include "HID.h"

// Pin setup
#define ADC_MAX 1024
#define NUM_BUTTONS 4 // Reduced to 4 buttons
#define NUM_FADERS 5 // Changed to 5 faders
#define NUM_LED_STRIPS 2
#define NUM_LEDS_PER_STRIP 31
#define LED_TYPE WS2812B
#define LED_ORDER GRB

// HID lighting setup
#define NUMBER_OF_SINGLE 4 // Reduced to 4 single HID lights
#define NUMBER_OF_RGB 1

bool lightStates[NUMBER_OF_SINGLE] = { false };
unsigned long lightTimestamp = 0;

typedef struct {
  uint8_t brightness;
} SingleLED;

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} RGBLed;

// Main output
Joystick_ Joystick(JOYSTICK_DEFAULT_REPORT_ID, JOYSTICK_TYPE_GAMEPAD, NUM_BUTTONS, 5, // 5 for the faders as axes (X, Y, Z, Rx, Ry for example)
 true, true, true, true, true, false, false, false, false, false, false); // Enable more axes if needed

// Button pins (adjust these to your actual button connections)
const byte buttonLightPins[NUM_BUTTONS] = { 4, 5, 6, 7 }; // Example pins, adjust as needed
const byte buttonInputPins[NUM_BUTTONS] = { 0, 1, 2, 3 }; // Example pins, adjust as needed
// Fader pins (using Analog pins A0-A4)
const byte faderPin[NUM_FADERS] = { A0, A1, A2, A3, A4 };

const byte ledPin1 = 8; // Using the original LED pins if available and not conflicting with faders
const byte ledPin2 = 9;
CRGB leds[NUM_LED_STRIPS][NUM_LEDS_PER_STRIP];

// Button debounce objects
// Make sure to match the number of Bounce objects to NUM_BUTTONS
Bounce buttons[NUM_BUTTONS] = {
  Bounce(),
  Bounce(),
  Bounce(),
  Bounce()
};

// Objects to read the analog values of the faders responsively, but with smoothing
ResponsiveAnalogRead faders[NUM_FADERS] = {
  ResponsiveAnalogRead(faderPin[0], true),
  ResponsiveAnalogRead(faderPin[1], true),
  ResponsiveAnalogRead(faderPin[2], true),
  ResponsiveAnalogRead(faderPin[3], true),
  ResponsiveAnalogRead(faderPin[4], true)
};

void setAllLeds(CRGB color);

/**
 * Setup all the hardware pins, etc.
 */
void setup() {
  //Serial.begin(115200);
  //while (!Serial) { delay(10);
  //}
  
  // Setup the ADC registers to run more quickly
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  cbi(ADCSRA, ADPS0);

  // Joystick init
  Joystick.begin(false);
  // Set ranges for all 5 fader axes (X, Y, Z, Rx, Ry)
  Joystick.setXAxisRange(0, ADC_MAX);
  Joystick.setYAxisRange(0, ADC_MAX);
  Joystick.setZAxisRange(0, ADC_MAX);
  Joystick.setRxAxisRange(0, ADC_MAX);
  Joystick.setRyAxisRange(0, ADC_MAX);


  // Initialize the IO pins for buttons
  for (int i = 0; i < NUM_BUTTONS; i++) {
    // No need for the buttonInputPins[i] == 20 check as we have direct pins
    pinMode(buttonLightPins[i], OUTPUT);
    buttons[i].attach(buttonInputPins[i], INPUT_PULLUP);
    buttons[i].interval(5);
  }

  // Initialize the IO pins for faders
  for (int i = 0; i < NUM_FADERS; i++) {
    pinMode(faderPin[i], INPUT);
  }

  // Initialize the LED strips
  FastLED.addLeds<LED_TYPE, ledPin1, LED_ORDER>(leds[0], NUM_LEDS_PER_STRIP);
  FastLED.addLeds<LED_TYPE, ledPin2, LED_ORDER>(leds[1], NUM_LEDS_PER_STRIP);

  // Boot LED sequence
  FastLED.setBrightness(128);
  setAllLeds(CRGB::Blue);
  FastLED.show();
}

/**
 * Callback for HID lighting events.
 */
void light_update(SingleLED* single_leds, RGBLed* rgb_leds) {
  // Read the on/off non-RGB lights
  for(int i = 0; i < NUMBER_OF_SINGLE; i++) {
    bool isOn = single_leds[i].brightness > 128;
    lightStates[i] = isOn;
  }

  // Read the RGB lights
  CRGB color = CRGB(rgb_leds[0].r, rgb_leds[0].g, rgb_leds[0].b);
  setAllLeds(color);
  lightTimestamp = millis();
}

/**
 * Main runtime loop.
 */
void loop() {
  // Read the button states
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].update();
    Joystick.setButton(i, !buttons[i].read());
  }

  // Read the faders
  for (int i = 0; i < NUM_FADERS; i++) {
    faders[i].update();
  }

  // Output the joystick states for faders (mapping to available axes)
  Joystick.setXAxis(faders[0].getValue());
  Joystick.setYAxis(faders[1].getValue());
  Joystick.setZAxis(faders[2].getValue()); // Map 3rd fader to Z-axis
  Joystick.setRxAxis(faders[3].getValue()); // Map 4th fader to Rx-axis
  Joystick.setRyAxis(faders[4].getValue()); // Map 5th fader to Ry-axis
  
  Joystick.sendState();
  
  if (lightTimestamp + 1000 < millis()) {
    // Update the lights as reactive (tied to button presses)
    for (int i = 0; i < NUM_BUTTONS; i++) { // Loop through all NUM_BUTTONS for reactive lights
      digitalWrite(buttonLightPins[i], !buttons[i].read());
    }
  } else {
    // Update the lights as necessary (controlled by HID)
    for (int i = 0; i < NUMBER_OF_SINGLE; i++) { // Loop through all NUMBER_OF_SINGLE HID lights
      digitalWrite(buttonLightPins[i], lightStates[i]);
    }
  }
  FastLED.show();
}

/**
 * Helper function to set all the LEDs across both strips the same color.
 */
void setAllLeds(CRGB color) {
  for (int strip = 0; strip < NUM_LED_STRIPS; strip++) {
    for (int led = 0; led < NUM_LEDS_PER_STRIP; led++) {
      leds[strip][led] = color;
    }
  }
}

// ******************************
// don't need to edit below here

#define NUMBER_OF_LIGHTS (NUMBER_OF_SINGLE + NUMBER_OF_RGB*3)
#if NUMBER_OF_LIGHTS > 63
  #error You must have less than 64 lights
#endif

union {
  struct {
    SingleLED singles[NUMBER_OF_SINGLE];
    RGBLed rgb[NUMBER_OF_RGB];
  } leds;
  uint8_t raw[NUMBER_OF_LIGHTS];
} led_data;

static const uint8_t PROGMEM _hidReportLEDs[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x00,                    // USAGE (Undefined)
    0xa1, 0x01,                    // COLLECTION (Application)
    
// Globals
    0x95, NUMBER_OF_LIGHTS,        //   REPORT_COUNT
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
 
    0x05, 0x0a,                    //   USAGE_PAGE (Ordinals)
    // Locals
    0x19, 0x01,                    //   USAGE_MINIMUM (Instance 1)
    0x29, NUMBER_OF_LIGHTS,        //   USAGE_MAXIMUM (Instance n)
    0x91, 0x02,              
       //   OUTPUT (Data,Var,Abs)
    // BTools needs at least 1 input to work properly
    0x19, 0x01,                    //   USAGE_MINIMUM (Instance 1)
    0x29, 0x01,                    //   USAGE_MAXIMUM (Instance 1)
    0x81, 0x03,            
         //   INPUT (Cnst,Var,Abs)
    0xc0                           // END_COLLECTION
};

// This is almost entirely copied from NicoHood's wonderful RawHID example
// Trimmed to the bare minimum
// https://github.com/NicoHood/HID/blob/master/src/SingleReport/RawHID.cpp
class HIDLED_ : public PluggableUSBModule {

  uint8_t epType[1];
public:
    HIDLED_(void) : PluggableUSBModule(1, 1, epType) {
      epType[0] = EP_TYPE_INTERRUPT_IN;
      PluggableUSB().plug(this);
}

    int getInterface(uint8_t* interfaceCount) {
      *interfaceCount += 1;
// uses 1
      HIDDescriptor hidInterface = {
        D_INTERFACE(pluggedInterface, 1, USB_DEVICE_CLASS_HUMAN_INTERFACE, HID_SUBCLASS_NONE, HID_PROTOCOL_NONE),
        D_HIDREPORT(sizeof(_hidReportLEDs)),
        D_ENDPOINT(USB_ENDPOINT_IN(pluggedEndpoint), USB_ENDPOINT_TYPE_INTERRUPT, USB_EP_SIZE, 16)
      };
return USB_SendControl(0, &hidInterface, sizeof(hidInterface));
    }
    
    int getDescriptor(USBSetup& setup)
    {
      // Check if this is a HID Class Descriptor request
      if (setup.bmRequestType != REQUEST_DEVICETOHOST_STANDARD_INTERFACE) { return 0;
}
      if (setup.wValueH != HID_REPORT_DESCRIPTOR_TYPE) { return 0;
}
    
      // In a HID Class Descriptor wIndex contains the interface number
      if (setup.wIndex != pluggedInterface) { return 0;
}
    
      return USB_SendControl(TRANSFER_PGM, _hidReportLEDs, sizeof(_hidReportLEDs));
}
    
    bool setup(USBSetup& setup)
    {
      if (pluggedInterface != setup.wIndex) {
        return false;
}
    
      uint8_t request = setup.bRequest;
      uint8_t requestType = setup.bmRequestType;
if (requestType == REQUEST_DEVICETOHOST_CLASS_INTERFACE)
      {
        return true;
}
    
      if (requestType == REQUEST_HOSTTODEVICE_CLASS_INTERFACE) {
        if (request == HID_SET_REPORT) {
          if(setup.wValueH == HID_REPORT_TYPE_OUTPUT && setup.wLength == NUMBER_OF_LIGHTS){
            USB_RecvControl(led_data.raw, NUMBER_OF_LIGHTS);
light_update(led_data.leds.singles, led_data.leds.rgb);
            return true;
          }
        }
      }
    
      return false;
}
};

HIDLED_ HIDLeds;
