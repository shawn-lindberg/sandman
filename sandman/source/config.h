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
		// returns:				True if successful, false otherwise.
		//
		bool ReadFromFile(char const* p_FileName);
		
		// Accessors.
		
		char const* GetInputDeviceName() const
		{
			return m_InputDeviceName;
		}
		
		unsigned int GetSampleRate() const
		{
			return m_SampleRate;
		}
		
	private:
	
		// Constants.
		enum {
			
			INPUT_DEVICE_NAME_CAPACITY = 64,
		};
		
		// The name of the input device.
		char m_InputDeviceName[INPUT_DEVICE_NAME_CAPACITY];

		// The input sample rate.
		unsigned int m_SampleRate;
};

