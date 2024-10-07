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

// Set the given GPIO pin mode to input.
//
// pin:	The GPIO pin to set the mode of.
//
void GPIOSetPinModeInput(int pin);

// Set the given GPIO pin mode to output.
//
// pin:	The GPIO pin to set the mode of.
//
void GPIOSetPinModeOutput(int pin);

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

