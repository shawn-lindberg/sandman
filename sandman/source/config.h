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
		// fileName:	The name of the config file.
		//
		// Returns:				True if successful, false otherwise.
		//
		bool ReadFromFile(char const* fileName);
		
		// Accessors.
		
		char const* GetInputDeviceName() const
		{
			return m_inputDeviceName;
		}
		
		std::vector<InputBinding> const& GetInputBindings() const
		{
			return m_inputBindings;
		}
		
		unsigned int GetControlMaxMovingDurationMS() const
		{
			return m_controlMaxMovingDurationMS;
		}
	
		unsigned int GetControlCoolDownDurationMS() const
		{
			return m_controlCoolDownDurationMS;
		}
		
		std::vector<ControlConfig> const& GetControlConfigs() const
		{
			return m_controlConfigs;
		}
		
	private:
	
		// Read control settings from JSON. 
		//
		// object:	The JSON object representing the control settings.
		//
		// Returns:		True if the settings were read successfully, false otherwise.
		//
		bool ReadControlSettingsFromJSON(rapidjson::Value const& object);

		// Read input settings from JSON. 
		//
		// object:	The JSON object representing the input settings.
		//
		// Returns:		True if the settings were read successfully, false otherwise.
		//
		bool ReadInputSettingsFromJSON(rapidjson::Value const& object);

		// Constants.
		static constexpr unsigned int kInputDeviceNameCapacity{ 64u };
		
		// The name of the input device.
		char m_inputDeviceName[kInputDeviceNameCapacity];
							
		// The list of input bindings.
		std::vector<InputBinding> m_inputBindings;
		
		// The maximum duration a control can move for (in milliseconds).
		unsigned int m_controlMaxMovingDurationMS = 100'000;
				
		// The duration a control will be on cooldown (in milliseconds).
		unsigned int m_controlCoolDownDurationMS = 50'000;
		
		// The list of control configs.
		std::vector<ControlConfig> m_controlConfigs;
};

