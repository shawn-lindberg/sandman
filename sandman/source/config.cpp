#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include "logger.h"

// Constants
//


// Functions
//

// Config members

Config::Config()
{
	m_inputDeviceName[0] = '\0';
}

// Read the configuration from a file.
// 
// configFileName:	The name of the config file.
//
// Returns:				True if successful, false otherwise.
//
bool Config::ReadFromFile(char const* configFileName)
{
	if (configFileName == nullptr)
	{
		return false;
	}
	
	// Open the config file.
	auto* configFile = fopen(configFileName, "r");

	if (configFile == nullptr)
	{
		Logger::WriteLine(Shell::Red("Failed to open the config file."));
		return false;
	}

	static constexpr std::size_t kReadBufferCapacity{ 65536u };
	char readBuffer[kReadBufferCapacity];
	rapidjson::FileReadStream configFileStream(configFile, readBuffer, sizeof(readBuffer));

	rapidjson::Document configDocument;
	configDocument.ParseStream(configFileStream);

	if (configDocument.HasParseError() == true)
	{
		Logger::WriteLine(Shell::Red("Failed to parse the config file."));
		fclose(configFile);
		return false;
	}
	
	// Find the control settings.
	auto const controlSettingsIterator = configDocument.FindMember("controlSettings");

	if (controlSettingsIterator == configDocument.MemberEnd())
	{
		Logger::WriteLine(Shell::Red("Config is missing control settings."));
		fclose(configFile);
		return false;		
	}

	// Read the control settings.
	if (ReadControlSettingsFromJSON(controlSettingsIterator->value) == false)
	{
		Logger::WriteLine(Shell::Red("Encountered error trying to read control settings."));
		fclose(configFile);
		return false;	
	}

	// If there are input settings, try to read them.
	auto const inputSettingsIterator = configDocument.FindMember("inputSettings");

	if (inputSettingsIterator != configDocument.MemberEnd())
	{
		if (ReadInputSettingsFromJSON(inputSettingsIterator->value) == false)
		{
			Logger::WriteLine(Shell::Red("Encountered error trying to read input settings."));
		}
	}

	fclose(configFile);
	return true;
}

// Read control settings from JSON. 
//
// object:	The JSON object representing the control settings.
//
// Returns:		True if the settings were read successfully, false otherwise.
//
bool Config::ReadControlSettingsFromJSON(rapidjson::Value const& object)
{
	if (object.IsObject() == false)
	{
		Logger::WriteLine(Shell::Red("Config has a control settings, but it's not an object."));
		return false;
	}

	// Try to get the max moving duration.
	auto const maxMovingIterator = object.FindMember("maxMovingDurationMS");

	if (maxMovingIterator != object.MemberEnd())
	{
		if (maxMovingIterator->value.IsInt() == true)
		{
			m_controlMaxMovingDurationMS = maxMovingIterator->value.GetInt();
		}
	}

	// Try to get to cool down duration.
	auto const coolDownIterator = object.FindMember("coolDownDurationMS");

	if (coolDownIterator != object.MemberEnd())
	{
		if (coolDownIterator->value.IsInt() == true)
		{
			m_controlCoolDownDurationMS = coolDownIterator->value.GetInt();
		}
	}

	// A controls array is required, but it may be empty.
	m_controlConfigs.clear();

	auto const controlsIterator = object.FindMember("controls");

	if (controlsIterator == object.MemberEnd())
	{
		Logger::WriteLine(Shell::Red("Config control settings is missing a control array."));
		return false;
	}

	if (controlsIterator->value.IsArray() == false)
	{
		Logger::WriteLine(
			Shell::Red("Config control settings has controls but it is not an array."));
		return false;
	}

	for (auto const& controlObject : controlsIterator->value.GetArray())
	{	
		// Try to read the control.
		ControlConfig controlConfig;
		if (controlConfig.ReadFromJSON(controlObject) == false)
		{
			continue;
		}

		// If we successfully read a control config, add it to the list.
		m_controlConfigs.push_back(controlConfig);
	}

	return true;
}

// Read input settings from JSON. 
//
// object:	The JSON object representing the input settings.
//
// Returns:		True if the settings were read successfully, false otherwise.
//
bool Config::ReadInputSettingsFromJSON(rapidjson::Value const& object)
{
	if (object.IsObject() == false)
	{
		Logger::WriteLine(
			Shell::Red("Config has in input settings member, but it's not an object."));
		return false;
	}

	// We must have an array of input devices.
	auto const inputDevicesIterator = object.FindMember("inputDevices");

	if (inputDevicesIterator == object.MemberEnd())
	{
		Logger::WriteLine(Shell::Red("Config is missing in input devices member."));
		return false;
	}

	if (inputDevicesIterator->value.IsArray() == false)
	{
		Logger::WriteLine(
			Shell::Red("Config has an input devices member, but it is not an array."));
		return false;
	}

	rapidjson::Value const& inputDevices = inputDevicesIterator->value;

	// In the future we may support multiple input devices, but for now we will just read the first
	// one.
	if (inputDevices.Size() == 0)
	{
		return true;
	}

	rapidjson::Value const& inputDevice = inputDevices[0];

	if (inputDevice.IsObject() == false)
	{
		Logger::WriteLine(Shell::Red("Config has an input device that is not an object."));
		return false;
	}

	// We must have a device name.
	auto const deviceIterator = inputDevice.FindMember("device");

	if (deviceIterator == object.MemberEnd())
	{
		Logger::WriteLine(Shell::Red("Config input device is missing the device name."));
		return false;
	}

	if (deviceIterator->value.IsString() == false)
	{
		Logger::WriteLine(Shell::Red("Config input device name is not a string."));
		return false;
	}
	
	// Copy no more than the amount of text the buffer can hold.
	strncpy(m_inputDeviceName, deviceIterator->value.GetString(), sizeof(m_inputDeviceName) - 1);
	m_inputDeviceName[sizeof(m_inputDeviceName) - 1] = '\0';

	// We must have a bindings array, but it can be empty.
	m_inputBindings.clear();

	auto const bindingsIterator = inputDevice.FindMember("bindings");

	if (bindingsIterator == inputDevice.MemberEnd())
	{
		Logger::WriteLine(Shell::Red("Config input device is missing a bindings array."));
		return false;
	}

	if (bindingsIterator->value.IsArray() == false)
	{
		Logger::WriteLine(
			Shell::Red("Config input device bindings exists, but it is not an array."));
		return false;
	}

	for (auto const& bindingObject : bindingsIterator->value.GetArray())
	{
		// Try to read the binding.
		InputBinding binding;
		if (binding.ReadFromJSON(bindingObject) == false)
		{
			continue;
		}

		// If we successfully read a binding, add it to the list.
		m_inputBindings.push_back(binding);
	}

	return true;
}