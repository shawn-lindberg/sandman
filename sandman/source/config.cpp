#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Constants
//


// Functions
//

template<class T>
T const& Min(T const& p_A, T const& p_B)
{
	return (p_A < p_B) ? p_A : p_B;
}

// Config members

Config::Config()
{
	m_InputDeviceName[0] = '\0';
	m_InputSampleRate = 0;
	m_PostSpeechDelaySec = 1.0f;
	m_ControlMovingDurationMS = 100000;
	m_ControlCoolDownDurationMS = 50000;
}

// Read the configuration from a file.
// 
// p_ConfigFileName:	The name of the config file.
//
// returns:				True if successful, false otherwise.
//
bool Config::ReadFromFile(char const* p_ConfigFileName)
{
	if (p_ConfigFileName == NULL)
	{
		return false;
	}
	
	// Open the config file.
	FILE* l_ConfigFile = fopen(p_ConfigFileName, "r");
	
	if (l_ConfigFile == NULL)
	{
		return false;
	}
	
	// Read each line in turn.
	unsigned int const l_LineBufferCapacity = 128;
	char l_LineBuffer[l_LineBufferCapacity];
	
	while (fgets(l_LineBuffer, l_LineBufferCapacity, l_ConfigFile) != NULL)
	{
		// Skip comments.
		if (l_LineBuffer[0] == '#')
		{
			continue;
		}
		
		// Settings take the form "key=value", so look for an =.
		char* l_Separator = strchr(l_LineBuffer, '=');
		
		if (l_Separator == NULL)
		{
			continue;
		}
		
		// For now, modify the string to split it.
		char const* l_KeyString = l_LineBuffer;
		char const* l_ValueString = l_Separator + 1;
		(*l_Separator) = '\0';
		
		unsigned int const l_ValueStringLength = strlen(l_ValueString);
		
		// The not-at-all-generic way of matching keys...
		if (strcmp(l_KeyString, "input_device") == 0)
		{
			// Copy the string.
			unsigned int const l_AmountToCopy = 
				Min(static_cast<unsigned int>(INPUT_DEVICE_NAME_CAPACITY) - 1, l_ValueStringLength);
			strncpy(m_InputDeviceName, l_ValueString, l_AmountToCopy);
			m_InputDeviceName[l_AmountToCopy] = '\0';
		}
		else if (strcmp(l_KeyString, "sample_rate") == 0)
		{
			// Convert to an integer.
			m_InputSampleRate = atoi(l_ValueString);
		}
		else if (strcmp(l_KeyString, "post_speech_delay") == 0)
		{
			// Convert to float.
			m_PostSpeechDelaySec = atof(l_ValueString);
		}
		else if (strcmp(l_KeyString, "moving_duration") == 0)
		{
			// Convert to an integer.
			m_ControlMovingDurationMS = atoi(l_ValueString);
		}
		else if (strcmp(l_KeyString, "cooldown_duration") == 0)
		{
			// Convert to an integer.
			m_ControlCoolDownDurationMS = atoi(l_ValueString);
		}
	}
	
	// Close the file.
	fclose(l_ConfigFile);
	
	return true;
}

