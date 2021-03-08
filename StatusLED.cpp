#include <FastLED.h>
#include "StatusLED.h"


void statusLedBlack() {
	leds[0] = CRGB::Black;
	FastLED.show();
}

void statusLedGreen() {
	leds[0] = CRGB::Green;
	FastLED.show();
}

void statusLedRed() {
	leds[0] = CRGB::Red;
	FastLED.show();
}

void statusLedOrange() {
	leds[0] = CRGB::Orange;
	FastLED.show();
}

void statusLedBlue() {
	leds[0] = CRGB::Blue;
	FastLED.show();
}

void statusLedYellow() {
	leds[0] = CRGB::Yellow;
	FastLED.show();
}

void statusLedMagenta() {
	leds[0] = CRGB::Magenta;
	FastLED.show();
}

void statusLedCyan() {
	leds[0] = CRGB::Cyan;
	FastLED.show();
}