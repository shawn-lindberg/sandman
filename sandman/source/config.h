#pragma once

#include "input.h"

// Types
//

// Stores the configuration.
//
class Config
{
	public:
	
		Config();
		
		// Read the configuration from a file.
		// 
		// p_ConfigFileName:	The name of the config file.
		//
		// Returns:				True if successful, false otherwise.
		//
		bool ReadFromFile(char const* p_FileName);
		
		// Accessors.
		
		char const* GetInputDeviceName() const
		{
			return m_InputDeviceName;
		}
		
		std::vector<InputBinding> const& GetInputBindings() const
		{
			return m_InputBindings;
		}
		
		unsigned int GetControlMaxMovingDurationMS() const
		{
			return m_ControlMaxMovingDurationMS;
		}
	
		unsigned int GetControlCoolDownDurationMS() const
		{
			return m_ControlCoolDownDurationMS;
		}
		
		std::vector<ControlConfig> const& GetControlConfigs() const
		{
			return m_ControlConfigs;
		}
		
	private:
	
		// Read control settings from JSON. 
		//
		// p_Object:	The JSON object representing the control settings.
		//
		// Returns:		True if the settings were read successfully, false otherwise.
		//
		bool ReadControlSettingsFromJSON(rapidjson::Value const& p_Object);

		// Read input settings from JSON. 
		//
		// p_Object:	The JSON object representing the input settings.
		//
		// Returns:		True if the settings were read successfully, false otherwise.
		//
		bool ReadInputSettingsFromJSON(rapidjson::Value const& p_Object);

		// Constants.
		static constexpr unsigned int ms_InputDeviceNameCapacity = 64;
		
		// The name of the input device.
		char m_InputDeviceName[ms_InputDeviceNameCapacity];
							
		// The list of input bindings.
		std::vector<InputBinding> m_InputBindings;
		
		// The maximum duration a control can move for (in milliseconds).
		unsigned int m_ControlMaxMovingDurationMS = 100000;
				
		// The duration a control will be on cooldown (in milliseconds).
		unsigned int m_ControlCoolDownDurationMS = 50000;
		
		// The list of control configs.
		std::vector<ControlConfig> m_ControlConfigs;
};

