#pragma once

#include <map>
#include <vector>

#include "control.h"
#include "timer.h"

// Types
//

// The description of an input binding and its associated action.
struct InputBinding
{	
	InputBinding() = default;
	
	// A constructor for emplacing.
	//
	InputBinding(unsigned short p_KeyCode, ControlAction&& p_ControlAction)
		: m_KeyCode(p_KeyCode), 
		m_ControlAction(std::move(p_ControlAction))
	{
	}
	
	// Read an input binding from JSON. 
	//
	// p_Object:	The JSON object representing a binding.
	//	
	// Returns:		True if the binding was read successfully, false otherwise.
	//
	bool ReadFromJSON(rapidjson::Value const& p_Object);	

	// The numeric code of the key that should trigger action.
	unsigned short		m_KeyCode;
	
	// Action to perform when the input is given. 
	ControlAction		m_ControlAction;
};

// Handles dealing with an input device.
//
class Input
{
	public:
		
		// Handle initialization.
		//
		// p_DeviceName:	The name of the input device that this will manage.
		// p_Bindings:		A list of input bindings.
		//
		void Initialize(char const* p_DeviceName, std::vector<InputBinding> const& p_Bindings);

		// Handle uninitialization.
		//
		void Uninitialize();
		
		// Process a tick.
		//
		void Process();
		
		// Determine whether the input device is connected.
		//
		bool IsConnected() const;
		
	private:

		// Constants.
		
		// The maximum length of the device name.
		static constexpr unsigned int ms_DeviceNameCapacity = 64;
		
		// Used to detect when a file handle is invalid.
		static constexpr int	ms_InvalidFileHandle = -1;
		
		// The amount of time to wait between failing to open the device.
		static constexpr unsigned int ms_DeviceOpenRetryDelayMS = 1000;
		
		// Close the input device.
		// 
		// p_WasFailure:	Whether the device is being closed due to a failure or not.
		// p_Format:		Standard printf format string.
		// ...:				Standard printf arguments.
		//
		void CloseDevice(bool p_WasFailure, char const* p_Format, ...);
		
		// The name of the device to get input from.
		char m_DeviceName[ms_DeviceNameCapacity];
		
		// The device file handle (file descriptor).
		int m_DeviceFileHandle = ms_InvalidFileHandle;	
		
		// Indicates that the device open has failed before.
		bool m_DeviceOpenHasFailed = false;
		
		// A time, so we can tell how long to wait before trying to open the device again.
		Time m_LastDeviceOpenFailTime;
				
		// The list of input bindings.
		std::vector<InputBinding> m_Bindings;
		
		// A mapping from input to action.
		std::map<unsigned short, ControlAction> m_InputToActionMap;
};
