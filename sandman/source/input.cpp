#include "input.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "logger.h"
#include "notification.h"
#include "timer.h"

#define DATADIR		AM_DATADIR

// Constants
//


// Types
//


// Locals
//


// Functions
//


// InputBinding members

// Read an input binding from JSON. 
//
// p_Object:	The JSON object representing a binding.
//	
// Returns:		True if the binding was read successfully, false otherwise.
//
bool InputBinding::ReadFromJSON(rapidjson::Value const& p_Object)	
{
	if (p_Object.IsObject() == false)
	{
		return false;
	}

	// There must be a keycode.
	auto const l_KeyCodeIterator = p_Object.FindMember("keyCode");

	if (l_KeyCodeIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Input binding is missing a key code.");
		return false;
	}

	if (l_KeyCodeIterator->value.IsInt() == false)
	{
		LoggerAddMessage("Input binding has a key code, but it is not an integer.");
		return false;
	}

	m_KeyCode = l_KeyCodeIterator->value.GetInt();

	// We must also have a control action.
	auto const l_ControlActionIterator = p_Object.FindMember("controlAction");

	if (l_ControlActionIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Input binding is missing a control action.");
		return false;
	}
	
	if (m_ControlAction.ReadFromJSON(l_ControlActionIterator->value) == false) 
	{
		LoggerAddMessage("Input binding has a control action, but it could not be parsed.");
		return false;
	}

	return true;
}

// Input members

// Handle initialization.
//
// p_DeviceName:	The name of the input device that this will manage.
// p_Bindings:		A list of input bindings.
//
void Input::Initialize(char const* p_DeviceName, std::vector<InputBinding> const& p_Bindings)
{
	// Copy the device name.
	strncpy(m_DeviceName, p_DeviceName, ms_DeviceNameCapacity - 1);
	m_DeviceName[ms_DeviceNameCapacity - 1] = '\0';
	
	// Populate the input bindings.
	m_Bindings = p_Bindings;
	
	// Use the bindings to populate the input to action mapping.
	for (const auto& l_Binding : m_Bindings) 
	{
		// Blindly insert. If the same key is bound more than once, the mapping will get overwritten 
		// with the last occurrence.
		m_InputToActionMap[l_Binding.m_KeyCode] = l_Binding.m_ControlAction;
	}
	
	// Display what we initialized.
	LoggerAddMessage("Initialized input device \'%s\' with input bindings:", m_DeviceName);
	
	for (auto const& l_Binding : m_Bindings) 
	{
		auto* const l_ActionText = 
			(l_Binding.m_ControlAction.m_Action == Control::Actions::ACTION_MOVING_UP) ? "up" : "down";
		
		LoggerAddMessage("\tCode %i -> %s, %s", l_Binding.m_KeyCode, 
			l_Binding.m_ControlAction.m_ControlName, l_ActionText);
	}
	
	LoggerAddEmptyLine();
}

// Handle uninitialization.
//
void Input::Uninitialize()
{
	// Make sure the device file is closed.
	CloseDevice(false, nullptr);
}

// Process a tick.
//
void Input::Process()
{
	// See if we need to open the device.
	if (m_DeviceFileHandle == ms_InvalidFileHandle) {
		
		// If we have failed before, see whether we have waited long enough before trying to open 
		// again.
		if (m_DeviceOpenHasFailed == true)
		{		
			// Get elapsed time since the last failure.
			Time l_CurrentTime;
			TimerGetCurrent(l_CurrentTime);

			auto const l_ElapsedTimeMS = TimerGetElapsedMilliseconds(m_LastDeviceOpenFailTime, 
				l_CurrentTime);
			
			if (l_ElapsedTimeMS < ms_DeviceOpenRetryDelayMS)
			{
				return;
			}
		}

		// We open in nonblocking mode so that we don't hang waiting for input.
		m_DeviceFileHandle = open(m_DeviceName, O_RDONLY | O_NONBLOCK);
		
		if (m_DeviceFileHandle < 0) 
		{
			// Record the time of the last open failure.
			TimerGetCurrent(m_LastDeviceOpenFailTime);
			
			CloseDevice(true, "Failed to open input device \'%s\'", m_DeviceName);						
			return;
		}
		
		// Try to get the name.
		char l_Name[256];
		if (ioctl(m_DeviceFileHandle, EVIOCGNAME(sizeof(l_Name)), l_Name) < 0)
		{	
			// Record the time of the last open failure.
			TimerGetCurrent(m_LastDeviceOpenFailTime);
					
			CloseDevice(true, "Failed to get name for input device \'%s\'", m_DeviceName);
		}
		
		LoggerAddMessage("Input device \'%s\' is a \'%s\'", m_DeviceName, l_Name);
		
		// More device information.
		unsigned short l_DeviceID[4];
		ioctl(m_DeviceFileHandle, EVIOCGID, l_DeviceID);
		
		LoggerAddMessage("Input device bus 0x%x, vendor 0x%x, product 0x%x, version 0x%x.", 
			l_DeviceID[ID_BUS], l_DeviceID[ID_VENDOR], l_DeviceID[ID_PRODUCT], l_DeviceID[ID_VERSION]);
			
		// Play controller connected notification.
		NotificationPlay("control_connected");
			
		m_DeviceOpenHasFailed = false;
	}
	
	// Read up to 64 input events at a time.
	static unsigned int const s_EventsToReadCount = 64;
	input_event l_Events[s_EventsToReadCount];
	static auto constexpr s_EventSize = sizeof(input_event);
	static auto const s_EventBufferSize = s_EventsToReadCount * s_EventSize;
	
	auto const l_ReadCount = read(m_DeviceFileHandle, l_Events, s_EventBufferSize);
	
	// I think maybe this would happen if the device got disconnected?
	if (l_ReadCount < 0)
	{		
		// When we are in nonblocking mode, this "error" means that there was no data and 
		// we need to check the device again.
		if (errno == EAGAIN)
		{
			return;
		}
		
		CloseDevice(true, "Failed to read from input device \'%s\'", m_DeviceName);
		return;
	}
	
	// Process each of the input events.
	auto const l_EventCount = l_ReadCount / s_EventSize;
	for (unsigned int l_EventIndex = 0; l_EventIndex < l_EventCount; l_EventIndex++)
	{
		auto const& l_Event = l_Events[l_EventIndex];
		
		// We are only handling keys/buttons for now.
		if (l_Event.type != EV_KEY)
		{
			continue;
		}
		
		//LoggerAddMessage("Input event type %i, code %i, value %i", l_Event.type, l_Event.code, 
		//	l_Event.value);
			
		// Try to find a control action corresponding to this input.
		auto l_Result = m_InputToActionMap.find(l_Event.code);
		
		if (l_Result == m_InputToActionMap.end())
		{
			continue;
		}
		
		// The action is the second member of the pair.
		auto& l_ControlAction = l_Result->second;
	
		// Try to find the corresponding control.
		auto* l_Control = l_ControlAction.GetControl();
		
		if (l_Control == nullptr) {
			
			LoggerAddMessage("Couldn't find control \'%s\' mapped to key code %i.", 
				l_ControlAction.m_ControlName, l_Event.code);
			continue;
		}
		
		// Translate whether the key was pressed or not into the appropriate action.
		auto const l_Action = (l_Event.value == 1) ? l_ControlAction.m_Action : 
			Control::Actions::ACTION_STOPPED;
			
		// Manipulate the control.
		l_Control->SetDesiredAction(l_Action, Control::Modes::MODE_MANUAL);
	}	
}

// Determine whether the input device is connected.
//
bool Input::IsConnected() const
{
	return (m_DeviceFileHandle != ms_InvalidFileHandle);
}
		
// Close the input device.
// 
// p_WasFailure:	Whether the device is being closed due to a failure or not.
// p_Format:		Standard printf format string.
// ...:				Standard printf arguments.
//
void Input::CloseDevice(bool p_WasFailure, char const* p_Format, ...)
{
	// Close the device.
	if (m_DeviceFileHandle != ms_InvalidFileHandle)
	{
		close(m_DeviceFileHandle);
		m_DeviceFileHandle = ms_InvalidFileHandle;
	}
			
	// Only log a message/play sound on failure.
	if (p_WasFailure == false) {
		return;
	}
	
	// And only if it was the first failure.
	if (m_DeviceOpenHasFailed == true)
	{
		return;
	}
	
	m_DeviceOpenHasFailed = true;	
	
	// Log the message.
	va_list l_Arguments;
	va_start(l_Arguments, p_Format);

	LoggerAddMessage(p_Format, l_Arguments);	
	
	va_end(l_Arguments);
		
	// Play controller disconnected notification.
	NotificationPlay("control_disconnected");
}
