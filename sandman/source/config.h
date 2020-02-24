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
		
		char const* GetSpeechInputDeviceName() const
		{
			return m_SpeechInputDeviceName;
		}
		
		unsigned int GetInputSampleRate() const
		{
			return m_InputSampleRate;
		}
	
		float GetPostSpeechDelaySec() const
		{
			return m_PostSpeechDelaySec;
		}
		
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
	
		// Constants.
		static constexpr unsigned int ms_InputDeviceNameCapacity = 64;
		
		// The name of the speech input device.
		char m_SpeechInputDeviceName[ms_InputDeviceNameCapacity];

		// The input sample rate.
		unsigned int m_InputSampleRate;
		
		// How long to wait with no new voice to end an utterance in seconds.
		float m_PostSpeechDelaySec;
		
		// The name of the input device.
		char m_InputDeviceName[ms_InputDeviceNameCapacity];
							
		// The list of input bindings.
		std::vector<InputBinding> m_InputBindings;
		
		// The maximum duration a control can move for (in milliseconds).
		unsigned int m_ControlMaxMovingDurationMS;
				
		// The duration a control will be on cooldown (in milliseconds).
		unsigned int m_ControlCoolDownDurationMS;
		
		// The list of control configs.
		std::vector<ControlConfig> m_ControlConfigs;
};

