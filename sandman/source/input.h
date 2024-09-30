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
	InputBinding(unsigned short keyCode, ControlAction&& controlAction)
		: m_keyCode(keyCode), 
		m_controlAction(std::move(controlAction))
	{
	}
	
	// Read an input binding from JSON. 
	//
	// object:	The JSON object representing a binding.
	//	
	// Returns:		True if the binding was read successfully, false otherwise.
	//
	bool ReadFromJSON(rapidjson::Value const& object);	

	// The numeric code of the key that should trigger action.
	unsigned short		m_keyCode;
	
	// Action to perform when the input is given. 
	ControlAction		m_controlAction;
};

// Handles dealing with an input device.
//
class Input
{
	public:
		
		// Handle initialization.
		//
		// deviceName:	The name of the input device that this will manage.
		// bindings:		A list of input bindings.
		//
		void Initialize(char const* deviceName, std::vector<InputBinding> const& bindings);

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
		static constexpr unsigned int kDeviceNameCapacity{ 64u };

		// Used to detect when a file handle is invalid.
		static constexpr int	kInvalidFileHandle{ -1 };

		// The amount of time to wait between failing to open the device.
		static constexpr unsigned int kDeviceOpenRetryDelayMS{ 1'000u };

		// Close the input device.
		//
		// wasFailure:	Whether the device is being closed due to a failure or not.
		// message: message that may be logged
		//
		void CloseDevice(bool const wasFailure, std::string_view const message);

		// The name of the device to get input from.
		char m_deviceName[kDeviceNameCapacity];
		
		// The device file handle (file descriptor).
		int m_deviceFileHandle = kInvalidFileHandle;	
		
		// Indicates that the device open has failed before.
		bool m_deviceOpenHasFailed = false;
		
		// A time, so we can tell how long to wait before trying to open the device again.
		Time m_lastDeviceOpenFailTime;
				
		// The list of input bindings.
		std::vector<InputBinding> m_bindings;
		
		// A mapping from input to action.
		std::map<unsigned short, ControlAction> m_inputToActionMap;
};
