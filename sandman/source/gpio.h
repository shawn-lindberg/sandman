#pragma once


// Functions
//

// Initialize GPIO support.
//
// enableGPIO: Whether to turn on GPIO or not.
//
void GPIOInitialize(bool const enableGPIO);

// Uninitialize GPIO support.
// 
void GPIOUninitialize();

// Acquire a GPIO pin as output.
//
// pin:	The GPIO pin to acquire as output.
//
void GPIOAcquireOutputPin(int pin);

// Release a GPIO pin.
//
// pin:  The GPIO pin to release.
//
void GPIOReleasePin(int pin);

// Set the given GPIO pin to the "on" value.
//
// pin:	The GPIO pin to set the value of.
//
void GPIOSetPinOn(int pin);

// Set the given GPIO pin to the "off" value.
//
// pin:	The GPIO pin to set the value of.
//
void GPIOSetPinOff(int pin);

