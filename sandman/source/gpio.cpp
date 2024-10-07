#include "gpio.h"

#if defined ENABLE_GPIO
	#include <pigpio.h>
#endif // defined ENABLE_GPIO

#include "logger.h"

// Constants
//

// GPIO values for on and off respectively.
static constexpr int kPinOnValue = 0;
static constexpr int kPinOffValue = 1;

// Locals
//

#if defined ENABLE_GPIO

	// Whether controls can use GPIO or not.
	static bool s_enableGPIO = true;

#endif // defined ENABLE_GPIO

// Functions
//

// Initialize GPIO support.
//
// enableGPIO: Whether to turn on GPIO or not.
//
void GPIOInitialize(bool const enableGPIO)
{
	#if defined ENABLE_GPIO
	
		s_enableGPIO = enableGPIO;

		if (s_enableGPIO == true)
		{
			Logger::WriteLine("Initializing GPIO support...");
	
			if (gpioInitialise() < 0)
			{
				Logger::WriteLine('\t', Shell::Red("failed"));
				return;
			}

			Logger::WriteLine('\t', Shell::Green("succeeded"));
		}
		else
		{
			Logger::WriteLine("GPIO support not enabled, initialization skipped.");
		}
		
		Logger::WriteLine();

	#endif // defined ENABLE_GPIO
}

// Uninitialize GPIO support.
// 
void GPIOUninitialize()
{
	#if defined ENABLE_GPIO
	
		// Uninitialize GPIO support.
		if (s_enableGPIO == true)
		{
			gpioTerminate();
		}
	
	#endif // defined ENABLE_GPIO
}

// Set the given GPIO pin mode to input.
//
// pin:	The GPIO pin to set the mode of.
//
void GPIOSetPinModeInput(int pin)
{
	#if defined ENABLE_GPIO

		if (s_enableGPIO == true)
		{
			gpioSetMode(pin, PI_INPUT);
		}
		else
		{
			Logger::WriteLine("Would have set GPIO ", pin, " mode to input, but it's not enabled.");
		}

	#else

		Logger::WriteLine("A Raspberry Pi would have set GPIO ", pin, " mode to input.");

	#endif // defined ENABLE_GPIO
}

// Set the given GPIO pin mode to output.
//
// pin:	The GPIO pin to set the mode of.
//
void GPIOSetPinModeOutput(int pin)
{
	#if defined ENABLE_GPIO

		if (s_enableGPIO == true)
		{
			gpioSetMode(pin, PI_OUTPUT);
		}
		else
		{
			Logger::WriteLine("Would have set GPIO ", pin, " mode to output, but it's not enabled.");
		}

	#else

		Logger::WriteLine("A Raspberry Pi would have set GPIO ", pin, " mode to output.");

	#endif // defined ENABLE_GPIO
}

// Set the given GPIO pin to the "on" value.
//
// pin:	The GPIO pin to set the value of.
//
void GPIOSetPinOn(int pin)
{
	#if defined ENABLE_GPIO

		if (s_enableGPIO == true)
		{
			gpioWrite(pin, kPinOnValue);
		}
		else
		{
			Logger::WriteLine("Would have set GPIO ", pin, " to on, but it's not enabled.");
		}

	#else

		Logger::WriteLine("A Raspberry Pi would have set GPIO ", pin, " to on.");

	#endif // defined ENABLE_GPIO
}

// Set the given GPIO pin to the "off" value.
//
// pin:	The GPIO pin to set the value of.
//
void GPIOSetPinOff(int pin)
{
	#if defined ENABLE_GPIO
	
		if (s_enableGPIO == true)
		{
			gpioWrite(pin, kPinOffValue);
		}
		else
		{
			Logger::WriteLine("Would have set GPIO ", pin, " to off, but it's not enabled.");
		}

	#else

		Logger::WriteLine("A Raspberry Pi would have set GPIO ", pin, " to off.");

	#endif // defined ENABLE_GPIO
}

