#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	m_InputDeviceName[0] = '\0';
}

// Read the configuration from a file.
// 
// p_ConfigFileName:	The name of the config file.
//
// Returns:				True if successful, false otherwise.
//
bool Config::ReadFromFile(char const* p_ConfigFileName)
{
	if (p_ConfigFileName == nullptr)
	{
		return false;
	}
	
	// Open the config file.
	auto* l_ConfigFile = fopen(p_ConfigFileName, "r");

	if (l_ConfigFile == nullptr)
	{
		LoggerAddMessage("Failed to open the config file.");
		return false;
	}

	static constexpr unsigned int l_ReadBufferCapacity = 65536;
	char l_ReadBuffer[l_ReadBufferCapacity];
	rapidjson::FileReadStream l_ConfigFileStream(l_ConfigFile, l_ReadBuffer, sizeof(l_ReadBuffer));

	rapidjson::Document l_ConfigDocument;
	l_ConfigDocument.ParseStream(l_ConfigFileStream);

	if (l_ConfigDocument.HasParseError() == true)
	{
		LoggerAddMessage("Failed to parse the config file.");
		fclose(l_ConfigFile);
		return false;
	}
	
	// Find the control settings.
	auto const l_ControlSettingsIterator = l_ConfigDocument.FindMember("controlSettings");

	if (l_ControlSettingsIterator == l_ConfigDocument.MemberEnd())
	{
		LoggerAddMessage("Config is missing control settings.");
		fclose(l_ConfigFile);
		return false;		
	}

	// Read the control settings.
	if (ReadControlSettingsFromJSON(l_ControlSettingsIterator->value) == false)
	{
		LoggerAddMessage("Encountered error trying to read control settings.");
		fclose(l_ConfigFile);
		return false;	
	}

	// If there are input settings, try to read them.
	auto const l_InputSettingsIterator = l_ConfigDocument.FindMember("inputSettings");

	if (l_InputSettingsIterator != l_ConfigDocument.MemberEnd())
	{
		if (ReadInputSettingsFromJSON(l_InputSettingsIterator->value) == false)
		{
			LoggerAddMessage("Encountered error trying to read input settings.");
		}
	}

	fclose(l_ConfigFile);
	return true;
}

// Read control settings from JSON. 
//
// p_Object:	The JSON object representing the control settings.
//
// Returns:		True if the settings were read successfully, false otherwise.
//
bool Config::ReadControlSettingsFromJSON(rapidjson::Value const& p_Object)
{
	if (p_Object.IsObject() == false)
	{
		LoggerAddMessage("Config has a control settings, but it's not an object.");
		return false;
	}

	// Try to get the max moving duration.
	auto const l_MaxMovingIterator = p_Object.FindMember("maxMovingDurationMS");

	if (l_MaxMovingIterator != p_Object.MemberEnd())
	{
		if (l_MaxMovingIterator->value.IsInt() == true)
		{
			m_ControlMaxMovingDurationMS = l_MaxMovingIterator->value.GetInt();
		}
	}

	// Try to get to cool down duration.
	auto const l_CoolDownIterator = p_Object.FindMember("coolDownDurationMS");

	if (l_CoolDownIterator != p_Object.MemberEnd())
	{
		if (l_CoolDownIterator->value.IsInt() == true)
		{
			m_ControlCoolDownDurationMS = l_CoolDownIterator->value.GetInt();
		}
	}

	// A controls array is required, but it may be empty.
	m_ControlConfigs.clear();

	auto const l_ControlsIterator = p_Object.FindMember("controls");

	if (l_ControlsIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Config control settings is missing a control array.");
		return false;
	}

	if (l_ControlsIterator->value.IsArray() == false)
	{
		LoggerAddMessage("Config control settings has controls but it is not an array.");
		return false;
	}

	for (auto const& l_ControlObject : l_ControlsIterator->value.GetArray())
	{	
		// Try to read the control.
		ControlConfig l_ControlConfig;
		if (l_ControlConfig.ReadFromJSON(l_ControlObject) == false)
		{
			continue;
		}

		// If we successfully read a control config, add it to the list.
		m_ControlConfigs.push_back(l_ControlConfig);
	}

	return true;
}

// Read input settings from JSON. 
//
// p_Object:	The JSON object representing the input settings.
//
// Returns:		True if the settings were read successfully, false otherwise.
//
bool Config::ReadInputSettingsFromJSON(rapidjson::Value const& p_Object)
{
	if (p_Object.IsObject() == false)
	{
		LoggerAddMessage("Config has in input settings member, but it's not an object.");
		return false;
	}

	// We must have an array of input devices.
	auto const l_InputDevicesIterator = p_Object.FindMember("inputDevices");

	if (l_InputDevicesIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Config is missing in input devices member.");
		return false;
	}

	if (l_InputDevicesIterator->value.IsArray() == false)
	{
		LoggerAddMessage("Config has an input devices member, but it is not an array.");
		return false;
	}

	rapidjson::Value const& l_InputDevices = l_InputDevicesIterator->value;

	// In the future we may support multiple input devices, but for now we will just read the first
	// one.
	if (l_InputDevices.Size() == 0)
	{
		return true;
	}

	rapidjson::Value const& l_InputDevice = l_InputDevices[0];

	if (l_InputDevice.IsObject() == false)
	{
		LoggerAddMessage("Config has an input device that is not an object.");
		return false;
	}

	// We must have a device name.
	auto const l_DeviceIterator = l_InputDevice.FindMember("device");

	if (l_DeviceIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Config input device is missing the device name.");
		return false;
	}

	if (l_DeviceIterator->value.IsString() == false)
	{
		LoggerAddMessage("Config input device name is not a string.");
		return false;
	}
	
	// Copy no more than the amount of text the buffer can hold.
	strncpy(m_InputDeviceName, l_DeviceIterator->value.GetString(), sizeof(m_InputDeviceName) - 1);
	m_InputDeviceName[sizeof(m_InputDeviceName) - 1] = '\0';

	// We must have a bindings array, but it can be empty.
	m_InputBindings.clear();

	auto const l_BindingsIterator = l_InputDevice.FindMember("bindings");

	if (l_BindingsIterator == l_InputDevice.MemberEnd())
	{
		LoggerAddMessage("Config input device is missing a bindings array.");
		return false;
	}

	if (l_BindingsIterator->value.IsArray() == false)
	{
		LoggerAddMessage("Config input device bindings exists, but it is not an array.");
		return false;
	}

	for (auto const& l_BindingObject : l_BindingsIterator->value.GetArray())
	{
		// Try to read the binding.
		InputBinding l_Binding;
		if (l_Binding.ReadFromJSON(l_BindingObject) == false)
		{
			continue;
		}

		// If we successfully read a binding, add it to the list.
		m_InputBindings.push_back(l_Binding);
	}

	return true;
}