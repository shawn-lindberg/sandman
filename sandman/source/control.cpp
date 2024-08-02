#include "control.h"

#include <cstdio>
#include <cstring>
#include <vector>

#if defined ENABLE_GPIO
	#include <pigpio.h>
#endif // defined ENABLE_GPIO

#include "logger.h"
#include "notification.h"
#include "timer.h"
#include "command.h"


// Constants
//

// Maximum duration of the moving state.
#define MAX_MOVING_STATE_DURATION_MS		(100 * 1000) // 100 sec.

// Maximum duration of the cool down state.
#define MAX_COOL_DOWN_STATE_DURATION_MS	(50 * 1000) // 50 sec.

// Time between commands.
//#define COMMAND_INTERVAmS				(2 * 1000) // 2 sec.

// The pin to use for enabling controls.
#define ENABLE_GPIO_PIN							(7)

// GPIO values for on and off respectively.
#define CONTROL_ON_GPIO_VALUE					(0)
#define CONTROL_OFF_GPIO_VALUE				(1)

// Locals
//

// The names of the actions.
static char const* const s_ControlActionNames[] =
{
	"stopped",		// kActionStopped
	"moving up",	// kActionMovingUp
	"moving down",	// kActionMovingDown
};

// The names of the modes.
static char const* const s_ControlModeNames[] =
{
	"manual",		// kModeManual
	"timed",		// kModeTimed
};

// The names of the states.
static char const* const s_ControlStateNames[] =
{
	"idle",			// kStateIdle
	"moving up",	// kStateMovingUp
	"moving down",	// kStateMovingDown
	"cool down",	// kStateCoolDown
};

// The notification names of the states.
static char const* const s_ControlStateNotificationNames[] =
{
	"",				// kStateIdle
	"moving_up",	// kStateMovingUp
	"moving_down",	// kStateMovingDown
	"stop",			// kStateCoolDown
};

// A list of registered controls.
static std::vector<Control> s_Controls;

// Control members

unsigned int Control::ms_MaxMovingDurationMS = MAX_MOVING_STATE_DURATION_MS;
unsigned int Control::ms_CoolDownDurationMS = MAX_COOL_DOWN_STATE_DURATION_MS;

// Functions
//

// Set the given GPIO pin to the "on" value.
//
// pin:	The GPIO pin to set the value of.
//
void SetGPIOPinOn(int pin)
{
	#if defined ENABLE_GPIO

		gpioWrite(pin, CONTROL_ON_GPIO_VALUE);

	#else

		Logger::FormatWriteLine("A Raspberry Pi would have set GPIO %d to on.", pin);

	#endif // defined ENABLE_GPIO
}

// Set the given GPIO pin to the "off" value.
//
// pin:	The GPIO pin to set the value of.
//
void SetGPIOPinOff(int pin)
{
	#if defined ENABLE_GPIO
	
		gpioWrite(pin, CONTROL_OFF_GPIO_VALUE);

	#else

		Logger::FormatWriteLine("A Raspberry Pi would have set GPIO %d to off.", pin);

	#endif // defined ENABLE_GPIO
}

// ControlHandle members

// A private constructor.
ControlHandle::ControlHandle(unsigned short uID) : m_UID(uID)
{
}

// Read a control config from JSON. 
//
// object:	The JSON object representing a control config.
//
// Returns:		True if the config was read successfully, false otherwise.
//
bool ControlConfig::ReadFromJSON(rapidjson::Value const& object)
{
	if (object.IsObject() == false)
	{
		Logger::FormatWriteLine("Control config cannot be parsed because it is not an object.");
		return false;
	}

	// We must have a control name.
	auto const nameIterator = object.FindMember("name");

	if (nameIterator == object.MemberEnd())
	{
		Logger::FormatWriteLine("Control config is missing a name.");
		return false;
	}

	if (nameIterator->value.IsString() == false)
	{
		Logger::FormatWriteLine("Control config has a name but it is not a string.");
		return false;
	}
	
	// Copy no more than the amount of text the buffer can hold.
	strncpy(m_Name, nameIterator->value.GetString(), sizeof(m_Name) - 1);
	m_Name[sizeof(m_Name) - 1] = '\0';

	// We must have an up pin.
	auto const upPinIterator = object.FindMember("upPin");

	if (upPinIterator == object.MemberEnd())
	{
		Logger::FormatWriteLine("Control config is missing an up pin.");
		return false;
	}

	if (upPinIterator->value.IsInt() == false)
	{
		Logger::FormatWriteLine("Control config has an up pin, but it is not an integer.");
		return false;
	}

	m_UpGPIOPin = upPinIterator->value.GetInt();

	// We must also have a down pin.
	auto const downPinIterator = object.FindMember("downPin");

	if (downPinIterator == object.MemberEnd())
	{
		Logger::FormatWriteLine("Control config is missing a down pin.");
		return false;
	}

	if (downPinIterator->value.IsInt() == false)
	{
		Logger::FormatWriteLine("Control config has a down pin, but it is not an integer.");
		return false;
	}

	m_DownGPIOPin = downPinIterator->value.GetInt();

	// We might also have a moving duration.
	auto const movingDurationIterator = object.FindMember("movingDurationMS");

	if (movingDurationIterator != object.MemberEnd())
	{
		if (movingDurationIterator->value.IsInt() == true)
		{
			m_MovingDurationMS = movingDurationIterator->value.GetInt();
		}
	}

	return true;
}

// Control members

// Handle initialization.
//
// config:	Configuration parameters for the control.
//
void Control::Initialize(ControlConfig const& config)
{
	// Copy the name.
	strncpy(m_Name, config.m_Name, kNameCapacity - 1);
	m_Name[kNameCapacity - 1] = '\0';
	
	m_State = kStateIdle;
	TimerGetCurrent(m_StateStartTime);
	m_DesiredAction = kActionStopped;

	// Setup the pins and set them to off.
	m_UpGPIOPin = config.m_UpGPIOPin;
	m_DownGPIOPin = config.m_DownGPIOPin;
	
	#if defined ENABLE_GPIO
	
		gpioSetMode(m_UpGPIOPin, PI_OUTPUT);
		SetGPIOPinOff(m_UpGPIOPin);
	
		gpioSetMode(m_DownGPIOPin, PI_OUTPUT);
		SetGPIOPinOff(m_DownGPIOPin);

	#endif // defined ENABLE_GPIO
	
	// Set the individual control moving duration.
	m_StandardMovingDurationMS = config.m_MovingDurationMS;
	
	Logger::FormatWriteLine("Initialized control \'%s\' with GPIO pins (up %i, "
		"down %i) and duration %i ms.", m_Name, m_UpGPIOPin, m_DownGPIOPin, m_StandardMovingDurationMS);
}

// Handle uninitialization.
//
void Control::Uninitialize()
{
	#if defined ENABLE_GPIO

		// Revert to input.
		gpioSetMode(m_UpGPIOPin, PI_INPUT);
		gpioSetMode(m_DownGPIOPin, PI_INPUT);

	#endif // defined ENABLE_GPIO
}

// Process a tick.
//
void Control::Process()
{
	// Handle state transitions.
	switch (m_State) 
	{
		case kStateIdle:
		{
			// Wait until moving is desired to transition.
			if (m_DesiredAction == kActionStopped) {
				break;
			}

			// Transition to moving.
			if (m_DesiredAction == kActionMovingUp)
			{
				m_State = kStateMovingUp;

				// Set the pin to on.
				SetGPIOPinOn(m_UpGPIOPin);
			}
			else
			{
				m_State = kStateMovingDown;

				// Set the pin to on.
				SetGPIOPinOn(m_DownGPIOPin);
			}
			
			// Play the notification.
			PlayNotification();
			
			// Record when the state transition timer began.
			TimerGetCurrent(m_StateStartTime);

			Logger::FormatWriteLine("Control \"%s\": State transition from \"%s\" to \"%s\" triggered.", 
				m_Name, s_ControlStateNames[kStateIdle], s_ControlStateNames[m_State]);
		}
		break;

		case kStateMovingUp:	// Fall through...
		case kStateMovingDown:
		{
			// Get elapsed time since state start.
			Time currentTime;
			TimerGetCurrent(currentTime);

			auto const elapsedTimeMS = TimerGetElapsedMilliseconds(m_StateStartTime, currentTime);

			// Get the action corresponding to this state, as well as the one for the opposite state.
			auto const matchingAction = (m_State == kStateMovingUp) ? kActionMovingUp : 
				kActionMovingDown;
			auto const oppositeAction = (m_State == kStateMovingUp) ? kActionMovingDown : 
				kActionMovingUp;
			
			// Wait until the desired action no longer matches or the time limit has run out.
			if ((m_DesiredAction == matchingAction) && (elapsedTimeMS < m_MovingDurationMS))
			{
				break;
			}

			// We are about to change the state, so keep track of the old one.
			auto const oldState = m_State;
			
			if (m_DesiredAction == oppositeAction)
			{
				// Transition to the opposite state.
				auto const oppositeState = (m_State == kStateMovingUp) ? kStateMovingDown :
					kStateMovingUp;
				m_State = oppositeState;

				// Flip the pins.
				auto const oldStatePin = (m_State == kStateMovingDown) ? m_UpGPIOPin : 
					m_DownGPIOPin;
				auto const newStatePin = (m_State == kStateMovingDown) ? m_DownGPIOPin : 
					m_UpGPIOPin;
				SetGPIOPinOff(oldStatePin);
				SetGPIOPinOn(newStatePin);
			}
			else
			{
				// Transition to cool down.
				m_State = kStateCoolDown;

				// Set the pins to off.
				SetGPIOPinOff(m_UpGPIOPin);
				SetGPIOPinOff(m_DownGPIOPin);
			}
			
			// Play the notification.
			PlayNotification();
			
			// Record when the state transition timer began.
			TimerGetCurrent(m_StateStartTime);

			Logger::FormatWriteLine("Control \"%s\": State transition from \"%s\" to \"%s\" triggered.", 
				m_Name, s_ControlStateNames[oldState], s_ControlStateNames[m_State]);
		}
		break;

		case kStateCoolDown:
		{
			// Clear the desired action.
			m_DesiredAction = kActionStopped;

			// Get elapsed time since state start.
			Time currentTime;
			TimerGetCurrent(currentTime);

			auto const elapsedTimeMS = TimerGetElapsedMilliseconds(m_StateStartTime, currentTime);

			// Wait until the time limit has run out.
			if (elapsedTimeMS < ms_CoolDownDurationMS)
			{
				break;
			}

			// Transition to idle.
			m_State = kStateIdle;

			// Set the pins to off.
			SetGPIOPinOff(m_UpGPIOPin);
			SetGPIOPinOff(m_DownGPIOPin);

			Logger::FormatWriteLine("Control \"%s\": State transition from \"%s\" to \"%s\" triggered.", 
				m_Name, s_ControlStateNames[kStateCoolDown], s_ControlStateNames[m_State]);
		}
		break;

		default:
		{
			Logger::FormatWriteLine("Control \"%d\": Unrecognized state %s in Process()", m_State, m_Name);
		}
		break;
	}
}

// Set the desired action.
//
// desiredAction:		The desired action.
// mode:					The mode of the action.
// durationPercent:	(Optional) The percent of the normal duration to perform the action
//								for.
//
void Control::SetDesiredAction(Actions desiredAction, Modes mode, unsigned int durationPercent)
{
	static_assert(
		std::is_same_v<decltype(durationPercent), decltype(CommandToken::m_Parameter)>,
		"Assert the type of `durationPercent` is the same as the type of `CommandToken::m_Parameter`. "
		"Currently, the main purpose of `CommandToken::m_Parameter` is to be used as `durationPercent`, "
		"so this assertion serves as a notification for if the types become unsynchronized.");

	m_DesiredAction = desiredAction;
	m_Mode = mode;

	if (m_Mode == kModeTimed)
	{
		// Set the current moving duration based on the requested percentage of the standard amount.
		auto const durationFraction = std::min(durationPercent, 100u) / 100.0f;
		m_MovingDurationMS =
			static_cast<unsigned int>(m_StandardMovingDurationMS * durationFraction);
	}
	else
	{
		m_MovingDurationMS = ms_MaxMovingDurationMS;
	}

	Logger::FormatWriteLine("Control \"%s\": Setting desired action to \"%s\" with mode \"%s\" and "
		"duration %i ms.", m_Name, s_ControlActionNames[desiredAction], 
		s_ControlModeNames[mode], m_MovingDurationMS);
}

// Enable or disable all controls.
//
// enable:	Whether to enable or disable all controls.
//
void Control::Enable(bool enable)
{
	if (enable == false)
	{
		// Revert to input.
		//pinMode(ENABLE_GPIO_PIN, INPUT);

		Logger::FormatWriteLine("Controls disabled.");
	}
	else
	{
		// Setup the pin and set it to off.
		//pinMode(ENABLE_GPIO_PIN, OUTPUT);
		//SetGPIOPinOff(ENABLE_GPIO_PIN);

		Logger::FormatWriteLine("Controls enabled.");
	}
}

// Set the durations.
//
// movingDurationMS:		Duration of the moving state (in milliseconds).
// coolDownDurationMS:	Duration of the cool down state (in milliseconds).
//
void Control::SetDurations(unsigned int movingDurationMS, unsigned int coolDownDurationMS)
{
	ms_MaxMovingDurationMS = movingDurationMS;
	ms_CoolDownDurationMS = coolDownDurationMS;
	
	Logger::FormatWriteLine("Control durations set to moving - %i ms, cool down - %i ms.", movingDurationMS, coolDownDurationMS);
}

// Attempt to get the handle of a control based on its name.
//
// name:	The unique name of the control.
//
// Returns:	A handle to the control, or an invalid handle if one with the given name could not be found.
//
ControlHandle Control::GetHandle(char const* name)
{
	ControlHandle handle;
	
	auto const controlCount = static_cast<unsigned short>(s_Controls.size());
	for (unsigned short controlIndex = 0; controlIndex < controlCount; controlIndex++)
	{
		auto const& control = s_Controls[controlIndex];
		
		// Look for a control with the matching name.
		if (strcmp(control.GetName(), name) != 0)
		{
			continue;
		}
		
		// Found it, return the handle.
		handle.m_UID = controlIndex;
		break;
	}
	
	// Return the handle we found, or didn't.
	return handle;
}

// Look up a control from its handle.
//
// handle:	A handle to the control.
//
// Returns:		The control, or null if the handle is not valid.
//
Control* Control::GetFromHandle(ControlHandle const& handle)
{
	// Sanity checking.
	if (handle.IsValid() == false)
	{
		return nullptr;
	}
	
	auto const controlCount = static_cast<unsigned short>(s_Controls.size());
	if (handle.m_UID >= controlCount)
	{
		return nullptr;
	}
	
	// Seems we have a valid handle, return the control.
	return &(s_Controls[handle.m_UID]);
}
		
// Play a notification for the state.
//
void Control::PlayNotification()
{
	// Do not play a notification if the mode is manual.
	if (m_Mode == kModeManual)
	{
		return;
	}

	// Build the notification name.
	static constexpr std::size_t kNotificationNameCapacity{ 128u };
	char notificationName[kNotificationNameCapacity];
	std::snprintf(notificationName, kNotificationNameCapacity, "%s_%s", m_Name,
		s_ControlStateNotificationNames[m_State]);

	NotificationPlay(notificationName);
}

// ControlAction members

// A constructor for emplacing.
// 
ControlAction::ControlAction(char const* controlName, Control::Actions action)
	: m_Action(action)
{
	// Copy the control name.
	std::strncpy(m_ControlName, controlName, kControlNameCapacity - 1);
	m_ControlName[kControlNameCapacity - 1] = '\0';
}

// Try to find a control action that matches the input text.
//
// action:		(Output) The action that was found.
// inputText:	The name of the action to look for. 
//
// Returns:		True if the action was found, false otherwise.
//
auto GetControlActionFromString(Control::Actions& action, char const* inputText)
{
	// The names of the actions.
	static char const* const s_ActionNames[] =
	{
		"stop",	// kActionStopped
		"up",		// kActionMovingUp
		"down",	// kActionMovingDown
	};

	// Try to find a control action that matches this text.
	auto const actionCount = Control::Actions::kNumActions;
	for (unsigned int actionIndex = 0; actionIndex < actionCount; actionIndex++)
	{
		// Compare the text to the action name.
		auto const* actionName = s_ActionNames[actionIndex];
			
		if (strncmp(inputText, actionName, strlen(actionName)) != 0)			
		{
			continue;
		}
			
		// We found the action, so return it.
		action = static_cast<Control::Actions>(actionIndex);
		return true;
	}
		
	return false;
}

// Read a control action from JSON. 
//
// object:	The JSON object representing a control action.
//
// Returns:		True if the action was read successfully, false otherwise.
//
bool ControlAction::ReadFromJSON(rapidjson::Value const& object)
{
	if (object.IsObject() == false)
	{
		Logger::FormatWriteLine("Control action cannot be parsed because it is not an object.");
		return false;
	}

	// We must have a control name.
	auto const controlIterator = object.FindMember("control");

	if (controlIterator == object.MemberEnd())
	{
		Logger::FormatWriteLine("Control action is missing a control name.");
		return false;
	}

	if (controlIterator->value.IsString() == false)
	{
		Logger::FormatWriteLine("Control action has a control name, but it is not a string.");
		return false;
	}
	
	// Copy no more than the amount of text the buffer can hold.
	strncpy(m_ControlName, controlIterator->value.GetString(), sizeof(m_ControlName) - 1);
	m_ControlName[sizeof(m_ControlName) - 1] = '\0';

	// We must also have an action.
	auto const actionIterator = object.FindMember("action");

	if (actionIterator == object.MemberEnd())
	{
		Logger::FormatWriteLine("Control action does not have an action.");
		return false;
	}

	if (actionIterator->value.IsString() == false)
	{
		Logger::FormatWriteLine("Control action has an action, but it is not a string.");
		return false;
	}

	// Try to get the corresponding action.
	if (GetControlActionFromString(m_Action, actionIterator->value.GetString()) == false)
	{
		Logger::FormatWriteLine("Control action has an unrecognized action.");
		return false;
	}

	return true;
}

// Attempt to get the control corresponding to the control action.
//
// Returns:	The control if successful, null otherwise.
//
Control* ControlAction::GetControl()
{
	// Look up the handle, if we haven't done that yet.
	if (m_ControlHandle.IsValid() == false) {
		m_ControlHandle = Control::GetHandle(m_ControlName);
	}
	
	// Try to find the control.
	return Control::GetFromHandle(m_ControlHandle);
}
	
// Functions
//

// Initialize all of the controls.
//
// configs:	Configuration parameters for the controls to add.
//
void ControlsInitialize(std::vector<ControlConfig> const& configs)
{
	#if defined ENABLE_GPIO
	
		Logger::FormatWriteLine("Initializing GPIO support...");
	
		if (gpioInitialise() < 0)
		{
			Logger::FormatWriteLine("\tfailed");
			return;
		}

		Logger::WriteLine('\t', NCurses::Green("succeeded"));
		Logger::WriteLine();

	#endif // defined ENABLE_GPIO

	for (auto const& config : configs)
	{
		ControlsCreateControl(config);
	}
}

// Uninitialize all of the controls.
//
void ControlsUninitialize()
{
	for (auto& control : s_Controls)
	{
		control.Uninitialize();
	}
	
	// Get rid of all of the controls.
	s_Controls.clear();

	#if defined ENABLE_GPIO
	
		// Uninitialize GPIO support.
		gpioTerminate();
	
	#endif // defined ENABLE_GPIO
}

// Process all of the controls.
//
void ControlsProcess()
{
	for (auto& control : s_Controls)
	{
		control.Process();
	}	
}

// Create a new control with the provided config. Control names must be unique.
//
// config:	Configuration parameters for the control.
//
// Returns:		True if the control was successfully created, false otherwise.
//
bool ControlsCreateControl(ControlConfig const& config)
{
	// Check to see whether a control with this name already exists.
	if (Control::GetHandle(config.m_Name).IsValid() == true)
	{		
		Logger::FormatWriteLine("Control with name \"%s\" already exists.", config.m_Name);
		return false;
	}
	
	// Add a new control.
	s_Controls.emplace_back(Control());
	
	// Then, initialize it.
	auto& control = s_Controls.back();
	control.Initialize(config);
	
	return true;
}

// Stop all of the controls.
//
void ControlsStopAll()
{
	for (auto& control : s_Controls)
	{
		control.SetDesiredAction(Control::kActionStopped, Control::kModeManual);
	}
}
