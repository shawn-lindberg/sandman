#include "gpio.h"

#include <map>

#if defined ENABLE_GPIO
	#include <gpiod.h>
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

	// The interface with the pins through the chip they are controlled by.
	static gpiod_chip* s_chip = nullptr;

	// A mapping of all of the pins that we have acquired.
	static std::map<int, gpiod_line*> s_pinToLineMap;

#endif // defined ENABLE_GPIO

// Functions
//

// Initialize GPIO support.
//
// enableGPIO: Whether to turn on GPIO or not.
//
void GPIOInitialize([[maybe_unused]] bool const enableGPIO)
{
	#if defined ENABLE_GPIO
	
		s_enableGPIO = enableGPIO;

		if (s_enableGPIO == true)
		{
			Logger::WriteLine("Initializing GPIO support...");
	
			// RPI5 attempt.
			s_chip = gpiod_chip_open_by_name("gpiochip4");

			if (s_chip == nullptr)
			{
				s_chip = gpiod_chip_open_by_name("gpiochip0");
			}

			if (s_chip == nullptr)
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
	
		if (s_enableGPIO == false)
		{
			return;
		}

		if (s_chip == nullptr)
		{
			return;
		}

		// Release all of the pins.
		for (auto& pair : s_pinToLineMap)
		{
			gpiod_line_release(pair.second);
		}
		s_pinToLineMap.clear();

		gpiod_chip_close(s_chip);
		s_chip = nullptr;
	
	#endif // defined ENABLE_GPIO
}

// Acquire a GPIO pin as output.
//
// pin:	The GPIO pin to acquire as output.
// 
void GPIOAcquireOutputPin(int pin)
{
	#if defined ENABLE_GPIO
	
		if (s_enableGPIO == false)
		{
			Logger::WriteLine("Would have acquired GPIO ", pin, 
									" pin for output, but it's not enabled.");
			return;
		}

		if (s_chip == nullptr)
		{
			Logger::WriteLine(Shell::Red("No chip when attempting to acquire GPIO "), pin, 
									Shell::Red(" pin for output."));
			return;
		}

		if (s_pinToLineMap.find(pin) != s_pinToLineMap.end())
		{
			Logger::WriteLine(Shell::Yellow("Attempted to acquire GPIO "), pin, 
									Shell::Yellow(" pin for output, but it's already been acquired."));
			return;
		}

		auto* line = gpiod_chip_get_line(s_chip, pin);

		if (line == nullptr)
		{
			Logger::WriteLine(Shell::Red("Failed to get line when attempting to acquire GPIO "), pin,
									Shell::Red(" pin for output."));
			return;
		}

		if (gpiod_line_request_output(line, "sandman", 0) < 0)
		{
			Logger::WriteLine(Shell::Red("Failed to set pin to output when trying to acquire GPIO "),
									pin, Shell::Red(" pin for output."));
			gpiod_line_release(line);
			return;
		}

		// Add a record.
		s_pinToLineMap.insert({pin, line});

	#else

		Logger::WriteLine("A Raspberry Pi would have tried to acquire GPIO ", pin, 
								" pin for output.");

	#endif // defined ENABLE_GPIO
}

// Release a GPIO pin.
//
// pin:  The GPIO pin to release.
//
void GPIOReleasePin(int pin)
{
	#if defined ENABLE_GPIO

		if (s_enableGPIO == false)
		{
			Logger::WriteLine("Would have released GPIO ", pin, " pin, but it's not enabled.");
			return;
		}

		if (s_chip == nullptr)
		{
			Logger::WriteLine(Shell::Red("No chip when attempting to release GPIO "), pin, 
									Shell::Red(" pin."));
			return;
		}

		// See if we have acquired the pin.
		auto pinIterator = s_pinToLineMap.find(pin);
		if (pinIterator == s_pinToLineMap.end())
		{
			Logger::WriteLine(Shell::Yellow("Attempted to release GPIO "), pin, 
									Shell::Yellow(" pin, but hasn't been acquired."));
			return;
		}

		auto* line = pinIterator->second;
		gpiod_line_release(line);

		// Erase our record.
		s_pinToLineMap.erase(pinIterator);

	#else

		Logger::WriteLine("A Raspberry Pi would have tried to release GPIO ", pin, " pin.");

	#endif // defined ENABLE_GPIO
}

// Set the given GPIO pin to a specific value.
//
// pin:		The GPIO pin to set the value of.
// value:	The value to set the pin to.
//
static void GPIOSetPinValue(int pin, int value)
{
	const char* valueString = (value == kPinOffValue) ? "off" : "on";

	#if defined ENABLE_GPIO

		if (s_enableGPIO == false)
		{
			Logger::WriteLine("Would have set GPIO ", pin, " to ", valueString, 
									", but it's not enabled.");
			return;
		}

		if (s_chip == nullptr)
		{
			Logger::WriteLine(Shell::Red("No chip when attempting to set GPIO "), pin, 
									Shell::Red(" pin to "), valueString, Shell::Red("."));
			return;
		}

		// See if we have acquired the pin.
		auto pinIterator = s_pinToLineMap.find(pin);
		if (pinIterator == s_pinToLineMap.end())
		{
			Logger::WriteLine(Shell::Yellow("Attempted to set GPIO "), pin, 
									Shell::Yellow(" pin to "), valueString, 
									Shell::Yellow(", but hasn't been acquired."));
			return;
		}

		auto* line = pinIterator->second;
		if (gpiod_line_set_value(line, value) < 0)
		{
			Logger::WriteLine(Shell::Red("Attempted to set GPIO "), pin, 
									Shell::Red(" pin to "), valueString, 
									Shell::Red(", but there was an error."));
		}

	#else

		Logger::WriteLine("A Raspberry Pi would have set GPIO ", pin, " to ", valueString, ".");

	#endif // defined ENABLE_GPIO
}

// Set the given GPIO pin to the "on" value.
//
// pin:	The GPIO pin to set the value of.
//
void GPIOSetPinOn(int pin)
{
	GPIOSetPinValue(pin, kPinOnValue);
}

// Set the given GPIO pin to the "off" value.
//
// pin:	The GPIO pin to set the value of.
//
void GPIOSetPinOff(int pin)
{
	GPIOSetPinValue(pin, kPinOffValue);
}

