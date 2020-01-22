#pragma once

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
		
		unsigned int GetInputSampleRate() const
		{
			return m_InputSampleRate;
		}
	
		float GetPostSpeechDelaySec() const
		{
			return m_PostSpeechDelaySec;
		}
		
		unsigned int GetControlMaxMovingDurationMS() const
		{
			return m_ControlMaxMovingDurationMS;
		}
	
		unsigned int GetControlCoolDownDurationMS() const
		{
			return m_ControlCoolDownDurationMS;
		}
		
	private:
	
		// Constants.
		enum {
			
			INPUT_DEVICE_NAME_CAPACITY = 64,
		};
		
		// The name of the input device.
		char m_InputDeviceName[INPUT_DEVICE_NAME_CAPACITY];

		// The input sample rate.
		unsigned int m_InputSampleRate;
		
		// How long to wait with no new voice to end an utterance in seconds.
		float m_PostSpeechDelaySec;
		
		// The maximum duration a control can move for (in milliseconds).
		unsigned int m_ControlMaxMovingDurationMS;
				
		// The duration a control will be on cooldown (in milliseconds).
		unsigned int m_ControlCoolDownDurationMS;
};

