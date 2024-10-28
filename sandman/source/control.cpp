#include "control.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "gpio.h"
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
//#define COMMAND_INTERVAL_MS				(2 * 1000) // 2 sec.

// The names of the actions.
static constexpr char const* const kControlActionNames[] =
{
	"stopped",		// kActionStopped
	"moving up",	// kActionMovingUp
	"moving down",	// kActionMovingDown
};

// The names of the modes.
static constexpr char const* const kControlModeNames[] =
{
	"manual",	// kModeManual
	"timed",		// kModeTimed
};

// The names of the states.
static constexpr char const* const kControlStateNames[] =
{
	"idle",			// kStateIdle
	"moving up",	// kStateMovingUp
	"moving down",	// kStateMovingDown
	"cool down",	// kStateCoolDown
};

// The notification names of the states.
static constexpr char const* const kControlStateNotificationNames[] =
{
	"",				// kStateIdle
	"moving_up",	// kStateMovingUp
	"moving_down",	// kStateMovingDown
	"stop",			// kStateCoolDown
};

// Locals
//

// A list of registered controls.
static std::vector<Control> s_controls;

// A mapping of control name to control index.
static std::map<std::string, unsigned int> s_controlNameToIndexMap;

// Control members

unsigned int Control::ms_maxMovingDurationMS = MAX_MOVING_STATE_DURATION_MS;
unsigned int Control::ms_coolDownDurationMS = MAX_COOL_DOWN_STATE_DURATION_MS;

// Functions
//

// ControlConfig members

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
		Logger::WriteLine("Control config cannot be parsed because it is not an object.");
		return false;
	}

	// We must have a control name.
	auto const nameIterator = object.FindMember("name");

	if (nameIterator == object.MemberEnd())
	{
		Logger::WriteLine("Control config is missing a name.");
		return false;
	}

	if (nameIterator->value.IsString() == false)
	{
		Logger::WriteLine("Control config has a name but it is not a string.");
		return false;
	}
	
	// Copy no more than the amount of text the buffer can hold.
	strncpy(m_name, nameIterator->value.GetString(), sizeof(m_name) - 1);
	m_name[sizeof(m_name) - 1] = '\0';

	// We must have an up pin.
	auto const upPinIterator = object.FindMember("upPin");

	if (upPinIterator == object.MemberEnd())
	{
		Logger::WriteLine("Control config is missing an up pin.");
		return false;
	}

	if (upPinIterator->value.IsInt() == false)
	{
		Logger::WriteLine("Control config has an up pin, but it is not an integer.");
		return false;
	}

	m_upGPIOPin = upPinIterator->value.GetInt();

	// We must also have a down pin.
	auto const downPinIterator = object.FindMember("downPin");

	if (downPinIterator == object.MemberEnd())
	{
		Logger::WriteLine("Control config is missing a down pin.");
		return false;
	}

	if (downPinIterator->value.IsInt() == false)
	{
		Logger::WriteLine("Control config has a down pin, but it is not an integer.");
		return false;
	}

	m_downGPIOPin = downPinIterator->value.GetInt();

	// We might also have a moving duration.
	auto const movingDurationIterator = object.FindMember("movingDurationMS");

	if (movingDurationIterator != object.MemberEnd())
	{
		if (movingDurationIterator->value.IsInt() == true)
		{
			m_movingDurationMS = movingDurationIterator->value.GetInt();
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
	strncpy(m_name, config.m_name, kNameCapacity - 1);
	m_name[kNameCapacity - 1] = '\0';
	
	m_state = kStateIdle;
	TimerGetCurrent(m_stateStartTime);
	m_desiredAction = kActionStopped;

	// Setup the pins and set them to off.
	m_upGPIOPin = config.m_upGPIOPin;
	m_downGPIOPin = config.m_downGPIOPin;
	
	GPIOAcquireOutputPin(m_upGPIOPin);
	GPIOAcquireOutputPin(m_downGPIOPin);
	
	GPIOSetPinOff(m_upGPIOPin);
	GPIOSetPinOff(m_downGPIOPin);
	
	// Set the individual control moving duration.
	m_standardMovingDurationMS = config.m_movingDurationMS;

	Logger::WriteLine("Initialized control \'", m_name, "\' with GPIO pins (up ", m_upGPIOPin,
							", down ", m_downGPIOPin, ") and duration ", m_standardMovingDurationMS,
							" ms.");
}

// Handle uninitialization.
//
void Control::Uninitialize()
{
	// Release pins.
	GPIOReleasePin(m_upGPIOPin);
	GPIOReleasePin(m_downGPIOPin);
}

// Process a tick.
//
void Control::Process()
{
	// Handle state transitions.
	switch (m_state) 
	{
		case kStateIdle:
		{
			// Wait until moving is desired to transition.
			if (m_desiredAction == kActionStopped) {
				break;
			}

			// Transition to moving.
			if (m_desiredAction == kActionMovingUp)
			{
				m_state = kStateMovingUp;

				// Set the pin to on.
				GPIOSetPinOn(m_upGPIOPin);
			}
			else
			{
				m_state = kStateMovingDown;

				// Set the pin to on.
				GPIOSetPinOn(m_downGPIOPin);
			}
			
			// Play the notification.
			PlayNotification();

			// Record when the state transition timer began.
			TimerGetCurrent(m_stateStartTime);

			Logger::WriteLine("Control \"", m_name, "\": State transition from \"",
									kControlStateNames[kStateIdle], "\" to \"", kControlStateNames[m_state],
									"\" triggered.");
		}
		break;

		case kStateMovingUp:	// Fall through...
		case kStateMovingDown:
		{
			// Get elapsed time since state start.
			Time currentTime;
			TimerGetCurrent(currentTime);

			auto const elapsedTimeMS = TimerGetElapsedMilliseconds(m_stateStartTime, currentTime);

			// Get the action corresponding to this state, as well as the one for the opposite state.
			auto const matchingAction = (m_state == kStateMovingUp) ? kActionMovingUp : 
				kActionMovingDown;
			auto const oppositeAction = (m_state == kStateMovingUp) ? kActionMovingDown : 
				kActionMovingUp;
			
			// Wait until the desired action no longer matches or the time limit has run out.
			if ((m_desiredAction == matchingAction) && (elapsedTimeMS < m_movingDurationMS))
			{
				break;
			}

			// We are about to change the state, so keep track of the old one.
			auto const oldState = m_state;
			
			if (m_desiredAction == oppositeAction)
			{
				// Transition to the opposite state.
				auto const oppositeState = (m_state == kStateMovingUp) ? kStateMovingDown :
					kStateMovingUp;
				m_state = oppositeState;

				// Flip the pins.
				auto const oldStatePin = (m_state == kStateMovingDown) ? m_upGPIOPin : 
					m_downGPIOPin;
				auto const newStatePin = (m_state == kStateMovingDown) ? m_downGPIOPin : 
					m_upGPIOPin;
				GPIOSetPinOff(oldStatePin);
				GPIOSetPinOn(newStatePin);
			}
			else
			{
				// Transition to cool down.
				m_state = kStateCoolDown;

				// Set the pins to off.
				GPIOSetPinOff(m_upGPIOPin);
				GPIOSetPinOff(m_downGPIOPin);
			}
			
			// Play the notification.
			PlayNotification();
			
			// Record when the state transition timer began.
			TimerGetCurrent(m_stateStartTime);

			Logger::WriteLine("Control \"", m_name, "\": State transition from \"",
									kControlStateNames[oldState], "\" to \"", kControlStateNames[m_state],
									"\" triggered.");
		}
		break;

		case kStateCoolDown:
		{
			// Clear the desired action.
			m_desiredAction = kActionStopped;

			// Get elapsed time since state start.
			Time currentTime;
			TimerGetCurrent(currentTime);

			auto const elapsedTimeMS = TimerGetElapsedMilliseconds(m_stateStartTime, currentTime);

			// Wait until the time limit has run out.
			if (elapsedTimeMS < ms_coolDownDurationMS)
			{
				break;
			}

			// Transition to idle.
			m_state = kStateIdle;

			// Set the pins to off.
			GPIOSetPinOff(m_upGPIOPin);
			GPIOSetPinOff(m_downGPIOPin);

			Logger::WriteLine("Control \"", m_name, "\": State transition from \"",
									kControlStateNames[kStateCoolDown], "\" to \"",
									kControlStateNames[m_state], "\" triggered.");
		}
		break;

		default:
		{
			Logger::WriteLine("Control \"", m_state, "\": Unrecognized state ", m_name,
									" in Process()");
		}
		break;
	}
}

// Set the desired action.
//
// desiredAction:		The desired action.
// mode:					The mode of the action.
// durationPercent:	(Optional) The percent of the normal duration to perform the action for.
//
void Control::SetDesiredAction(Actions desiredAction, Modes mode, unsigned int durationPercent)
{
	static_assert(std::is_same_v<decltype(durationPercent), decltype(CommandToken::m_parameter)>,
					  "Assert the type of `durationPercent` "
					  "is the same as the type of `CommandToken::m_parameter`. "
					  "Currently, the main purpose of `CommandToken::m_parameter` "
					  "is to be used as `durationPercent`, "
					  "so this assertion serves as a notification for if "
					  "the types become unsynchronized.");

	m_desiredAction = desiredAction;
	m_mode = mode;

	if (m_mode == kModeTimed)
	{
		// Set the current moving duration based on the requested percentage of the standard amount.
		auto const durationFraction = std::min(durationPercent, 100u) / 100.0f;
		m_movingDurationMS =
			static_cast<unsigned int>(m_standardMovingDurationMS * durationFraction);
	}
	else
	{
		m_movingDurationMS = ms_maxMovingDurationMS;
	}

	Logger::WriteLine("Control \"", m_name, "\": Setting desired action to \"",
							kControlActionNames[desiredAction], "\" with mode \"",
							kControlModeNames[mode], "\" and duration ", m_movingDurationMS, " ms.");
}

// Enable or disable all controls.
//
// enable:	Whether to enable or disable all controls.
//
void Control::Enable(bool enable)
{
	if (enable == false)
	{
		Logger::WriteLine("Controls disabled.");
	}
	else
	{
		Logger::WriteLine("Controls enabled.");
	}
}

// Set the durations.
//
// movingDurationMS:		Duration of the moving state (in milliseconds).
// coolDownDurationMS:	Duration of the cool down state (in milliseconds).
//
void Control::SetDurations(unsigned int movingDurationMS, unsigned int coolDownDurationMS)
{
	ms_maxMovingDurationMS = movingDurationMS;
	ms_coolDownDurationMS = coolDownDurationMS;

	Logger::WriteLine("Control durations set to moving - ", movingDurationMS, " ms, cool down - ",
							coolDownDurationMS, " ms.");
}

// Look up a control by its name.
//
// name:	The name of the control.
//
// Returns:		The control, or null if one with the name could not be found.
//
Control* Control::GetByName(std::string const& name)
{
	auto controlIterator = s_controlNameToIndexMap.find(name);
	if (controlIterator == s_controlNameToIndexMap.end())
	{
		return nullptr;
	}
	
	auto const controlIndex = controlIterator->second;
	return &s_controls[controlIndex];
}
		
// Play a notification for the state.
//
void Control::PlayNotification()
{
	// Do not play a notification if the mode is manual.
	if (m_mode == kModeManual)
	{
		return;
	}

	// Build the notification name.
	static constexpr std::size_t kNotificationNameCapacity{ 128u };
	char notificationName[kNotificationNameCapacity];
	std::snprintf(notificationName, kNotificationNameCapacity, "%s_%s", m_name,
		kControlStateNotificationNames[m_state]);

	NotificationPlay(notificationName);
}

// ControlAction members

// A constructor for emplacing.
// 
ControlAction::ControlAction(char const* controlName, Control::Actions action)
	: m_action(action)
{
	// Copy the control name.
	std::strncpy(m_controlName, controlName, kControlNameCapacity - 1);
	m_controlName[kControlNameCapacity - 1] = '\0';
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
	static constexpr char const* const kActionNames[] =
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
		auto const* actionName = kActionNames[actionIndex];
			
		if (std::strncmp(inputText, actionName, std::strlen(actionName)) != 0)
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
		Logger::WriteLine("Control action cannot be parsed because it is not an object.");
		return false;
	}

	// We must have a control name.
	auto const controlIterator = object.FindMember("control");

	if (controlIterator == object.MemberEnd())
	{
		Logger::WriteLine("Control action is missing a control name.");
		return false;
	}

	if (controlIterator->value.IsString() == false)
	{
		Logger::WriteLine("Control action has a control name, but it is not a string.");
		return false;
	}
	
	// Copy no more than the amount of text the buffer can hold.
	strncpy(m_controlName, controlIterator->value.GetString(), sizeof(m_controlName) - 1);
	m_controlName[sizeof(m_controlName) - 1] = '\0';

	// We must also have an action.
	auto const actionIterator = object.FindMember("action");

	if (actionIterator == object.MemberEnd())
	{
		Logger::WriteLine("Control action does not have an action.");
		return false;
	}

	if (actionIterator->value.IsString() == false)
	{
		Logger::WriteLine("Control action has an action, but it is not a string.");
		return false;
	}

	// Try to get the corresponding action.
	if (GetControlActionFromString(m_action, actionIterator->value.GetString()) == false)
	{
		Logger::WriteLine("Control action has an unrecognized action.");
		return false;
	}

	return true;
}

// Attempt to get the control corresponding to the control action.
//
// Returns:	The control if successful, null otherwise.
//
Control* ControlAction::GetControl() const
{
	return Control::GetByName(m_controlName);
}
	
// Functions
//

// Initialize all of the controls.
//
// configs: Configuration parameters for the controls to add.
//
void ControlsInitialize(std::vector<ControlConfig> const& configs)
{
	for (auto const& config : configs)
	{
		ControlsCreateControl(config);
	}
}

// Uninitialize all of the controls.
//
void ControlsUninitialize()
{
	for (auto& control : s_controls)
	{
		control.Uninitialize();
	}
	
	// Get rid of all of the controls.
	s_controls.clear();
}

// Process all of the controls.
//
void ControlsProcess()
{
	for (auto& control : s_controls)
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
	if (s_controlNameToIndexMap.find(config.m_name) != s_controlNameToIndexMap.end())
	{
		Logger::WriteLine("Control with name \"", config.m_name, "\" already exists.");
		return false;
	}
	
	// Add a new control.
	s_controls.emplace_back(Control());
	
	// Then, initialize it.
	unsigned int const controlIndex = s_controls.size() - 1;
	s_controls[controlIndex].Initialize(config);
	
	// And add it to the map.
	s_controlNameToIndexMap.insert({config.m_name, controlIndex});

	return true;
}

// Stop all of the controls.
//
void ControlsStopAll()
{
	for (auto& control : s_controls)
	{
		control.SetDesiredAction(Control::kActionStopped, Control::kModeManual);
	}
}
