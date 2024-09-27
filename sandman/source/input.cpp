#include "input.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <cerrno>
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
// object:	The JSON object representing a binding.
//	
// Returns:		True if the binding was read successfully, false otherwise.
//
bool InputBinding::ReadFromJSON(rapidjson::Value const& object)	
{
	if (object.IsObject() == false)
	{
		return false;
	}

	// There must be a keycode.
	auto const keyCodeIterator = object.FindMember("keyCode");

	if (keyCodeIterator == object.MemberEnd())
	{
		Logger::WriteFormattedLine("Input binding is missing a key code.");
		return false;
	}

	if (keyCodeIterator->value.IsInt() == false)
	{
		Logger::WriteFormattedLine("Input binding has a key code, but it is not an integer.");
		return false;
	}

	m_keyCode = keyCodeIterator->value.GetInt();

	// We must also have a control action.
	auto const controlActionIterator = object.FindMember("controlAction");

	if (controlActionIterator == object.MemberEnd())
	{
		Logger::WriteFormattedLine("Input binding is missing a control action.");
		return false;
	}
	
	if (m_controlAction.ReadFromJSON(controlActionIterator->value) == false) 
	{
		Logger::WriteFormattedLine("Input binding has a control action, but it could not be parsed.");
		return false;
	}

	return true;
}

// Input members

// Handle initialization.
//
// deviceName:	The name of the input device that this will manage.
// bindings:		A list of input bindings.
//
void Input::Initialize(char const* deviceName, std::vector<InputBinding> const& bindings)
{
	// Copy the device name.
	strncpy(m_deviceName, deviceName, kDeviceNameCapacity - 1);
	m_deviceName[kDeviceNameCapacity - 1] = '\0';
	
	// Populate the input bindings.
	m_bindings = bindings;
	
	// Use the bindings to populate the input to action mapping.
	for (const auto& binding : m_bindings) 
	{
		// Blindly insert. If the same key is bound more than once, the mapping will get overwritten 
		// with the last occurrence.
		m_inputToActionMap[binding.m_keyCode] = binding.m_controlAction;
	}
	
	// Display what we initialized.
	Logger::WriteFormattedLine("Initialized input device \'%s\' with input bindings:", m_deviceName);
	
	for (auto const& binding : m_bindings) 
	{
		auto* const actionText = 
			(binding.m_controlAction.m_action == Control::Actions::kActionMovingUp) ? "up" : "down";
		
		Logger::WriteFormattedLine("\tCode %i -> %s, %s", binding.m_keyCode, 
			binding.m_controlAction.m_controlName, actionText);
	}
	
	Logger::WriteLine();
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
	if (m_deviceFileHandle == kInvalidFileHandle) {
		
		// If we have failed before, see whether we have waited long enough before trying to open 
		// again.
		if (m_deviceOpenHasFailed == true)
		{		
			// Get elapsed time since the last failure.
			Time currentTime;
			TimerGetCurrent(currentTime);

			auto const elapsedTimeMS = TimerGetElapsedMilliseconds(m_lastDeviceOpenFailTime, 
				currentTime);
			
			if (elapsedTimeMS < kDeviceOpenRetryDelayMS)
			{
				return;
			}
		}

		// We open in nonblocking mode so that we don't hang waiting for input.
		m_deviceFileHandle = open(m_deviceName, O_RDONLY | O_NONBLOCK);
		
		if (m_deviceFileHandle < 0) 
		{
			// Record the time of the last open failure.
			TimerGetCurrent(m_lastDeviceOpenFailTime);
			CloseDevice(true, "Failed to open input device \'%s\'", m_deviceName);
			return;
		}
		
		// Try to get the name.
		char name[256];
		if (ioctl(m_deviceFileHandle, EVIOCGNAME(sizeof(name)), name) < 0)
		{	
			// Record the time of the last open failure.
			TimerGetCurrent(m_lastDeviceOpenFailTime);
					
			CloseDevice(true, "Failed to get name for input device \'%s\'", m_deviceName);
		}
		
		Logger::WriteFormattedLine("Input device \'%s\' is a \'%s\'", m_deviceName, name);
		
		// More device information.
		unsigned short deviceID[4];
		ioctl(m_deviceFileHandle, EVIOCGID, deviceID);
		
		Logger::WriteFormattedLine("Input device bus 0x%x, vendor 0x%x, product 0x%x, version 0x%x.", 
			deviceID[ID_BUS], deviceID[ID_VENDOR], deviceID[ID_PRODUCT], deviceID[ID_VERSION]);
			
		// Play controller connected notification.
		NotificationPlay("control_connected");
			
		m_deviceOpenHasFailed = false;
	}

	// Read up to 64 input events at a time.
	static constexpr std::size_t kEventsToReadCount{64};
	input_event events[kEventsToReadCount];
	static constexpr std::size_t kEventSize{sizeof(input_event)};
	static constexpr std::size_t kEventBufferSize{kEventsToReadCount * kEventSize};

	static_assert(kEventBufferSize <= SSIZE_MAX,
					  "In `man 'read(2)'`, DESCRIPTION: "
					  "\"According to POSIX.1, if count is greater than SSIZE_MAX, "
					  "the result is implementation-defined; see NOTES for the upper limit on Linux.\"");
	auto const readCount = read(m_deviceFileHandle, events, kEventBufferSize);

	// I think maybe this would happen if the device got disconnected?
	if (readCount < 0)
	{
		// When we are in nonblocking mode, this "error" means that there was no data and 
		// we need to check the device again.
		if (errno == EAGAIN)
		{
			return;
		}
		
		CloseDevice(true, "Failed to read from input device \'%s\'", m_deviceName);
		return;
	}
	
	// Process each of the input events.
	auto const eventCount = readCount / kEventSize;
	for (unsigned int eventIndex = 0; eventIndex < eventCount; eventIndex++)
	{
		auto const& event = events[eventIndex];
		
		// We are only handling keys/buttons for now.
		if (event.type != EV_KEY)
		{
			continue;
		}
		
		//Logger::WriteFormattedLine("Input event type %i, code %i, value %i", event.type, event.code, 
		//	event.value);
			
		// Try to find a control action corresponding to this input.
		auto result = m_inputToActionMap.find(event.code);
		
		if (result == m_inputToActionMap.end())
		{
			continue;
		}
		
		// The action is the second member of the pair.
		auto& controlAction = result->second;
	
		// Try to find the corresponding control.
		auto* control = controlAction.GetControl();
		
		if (control == nullptr) {
			
			Logger::WriteFormattedLine("Couldn't find control \'%s\' mapped to key code %i.", 
				controlAction.m_controlName, event.code);
			continue;
		}
		
		// Translate whether the key was pressed or not into the appropriate action.
		auto const action = (event.value == 1) ? controlAction.m_action : 
			Control::Actions::kActionStopped;
			
		// Manipulate the control.
		control->SetDesiredAction(action, Control::Modes::kModeManual);
	}	
}

// Determine whether the input device is connected.
//
bool Input::IsConnected() const
{
	return (m_deviceFileHandle != kInvalidFileHandle);
}
		
// Close the input device.
// 
// wasFailure:	Whether the device is being closed due to a failure or not.
// format:		Standard printf format string.
// ...:				Standard printf arguments.
//
void Input::CloseDevice(bool wasFailure, char const* format, ...)
{
	// Close the device.
	if (m_deviceFileHandle != kInvalidFileHandle)
	{
		close(m_deviceFileHandle);
		m_deviceFileHandle = kInvalidFileHandle;
	}
			
	// Only log a message/play sound on failure.
	if (wasFailure == false) {
		return;
	}
	
	// And only if it was the first failure.
	if (m_deviceOpenHasFailed == true)
	{
		return;
	}
	
	m_deviceOpenHasFailed = true;	
	
	// Log the message.
	va_list arguments;
	va_start(arguments, format);

	Logger::WriteFormattedVarArgsListLine(Shell::Red, format, arguments);

	va_end(arguments);
		
	// Play controller disconnected notification.
	NotificationPlay("control_disconnected");
}
