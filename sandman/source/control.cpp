#include "control.h"

#include <stdio.h>
#include <string.h>
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
//#define COMMAND_INTERVAL_MS				(2 * 1000) // 2 sec.

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
	"stopped",		// ACTION_STOPPED
	"moving up",	// ACTION_MOVING_UP
	"moving down",	// ACTION_MOVING_DOWN
};

// The names of the modes.
static char const* const s_ControlModeNames[] =
{
	"manual",		// MODE_MANUAL
	"timed",		// MODE_TIMED
};

// The names of the states.
static char const* const s_ControlStateNames[] =
{
	"idle",			// STATE_IDLE
	"moving up",	// STATE_MOVING_UP
	"moving down",	// STATE_MOVING_DOWN
	"cool down",	// STATE_COOL_DOWN
};

// The notification names of the states.
static char const* const s_ControlStateNotificationNames[] =
{
	"",				// STATE_IDLE
	"moving_up",	// STATE_MOVING_UP
	"moving_down",	// STATE_MOVING_DOWN
	"stop",			// STATE_COOL_DOWN
};

// A list of registered controls.
static std::vector<Control> s_Controls;

#if defined ENABLE_GPIO

	// Whether controls can use GPIO or not.
	static bool s_EnableGPIO = true;

#endif // defined ENABLE_GPIO

// Control members

unsigned int Control::ms_MaxMovingDurationMS = MAX_MOVING_STATE_DURATION_MS;
unsigned int Control::ms_CoolDownDurationMS = MAX_COOL_DOWN_STATE_DURATION_MS;

// Functions
//

// Set the given GPIO pin mode to input.
//
// p_Pin:	The GPIO pin to set the mode of.
//
void SetGPIOPinModeInput(int p_Pin)
{
	#if defined ENABLE_GPIO

		if (s_EnableGPIO == true)
		{
			gpioSetMode(p_Pin, PI_INPUT);
		}
		else
		{
			LoggerAddMessage("Would have set GPIO %d mode to input, but it's not enabled.", p_Pin);
		}

	#else

		LoggerAddMessage("A Raspberry Pi would have set GPIO %d mode to input.", p_Pin);

	#endif // defined ENABLE_GPIO
}

// Set the given GPIO pin mode to output.
//
// p_Pin:	The GPIO pin to set the mode of.
//
void SetGPIOPinModeOutput(int p_Pin)
{
	#if defined ENABLE_GPIO

		if (s_EnableGPIO == true)
		{
			gpioSetMode(p_Pin, PI_OUTPUT);
		}
		else
		{
			LoggerAddMessage("Would have set GPIO %d mode to output, but it's not enabled.", p_Pin);
		}

	#else

		LoggerAddMessage("A Raspberry Pi would have set GPIO %d mode to output.", p_Pin);

	#endif // defined ENABLE_GPIO
}

// Set the given GPIO pin to the "on" value.
//
// p_Pin:	The GPIO pin to set the value of.
//
void SetGPIOPinOn(int p_Pin)
{
	#if defined ENABLE_GPIO

		if (s_EnableGPIO == true)
		{
			gpioWrite(p_Pin, CONTROL_ON_GPIO_VALUE);
		}
		else
		{
			LoggerAddMessage("Would have set GPIO %d to on, but it's not enabled.", p_Pin);
		}

	#else

		LoggerAddMessage("A Raspberry Pi would have set GPIO %d to on.", p_Pin);

	#endif // defined ENABLE_GPIO
}

// Set the given GPIO pin to the "off" value.
//
// p_Pin:	The GPIO pin to set the value of.
//
void SetGPIOPinOff(int p_Pin)
{
	#if defined ENABLE_GPIO
	
		if (s_EnableGPIO == true)
		{
			gpioWrite(p_Pin, CONTROL_OFF_GPIO_VALUE);
		}
		else
		{
			LoggerAddMessage("Would have set GPIO %d to off, but it's not enabled.", p_Pin);
		}

	#else

		LoggerAddMessage("A Raspberry Pi would have set GPIO %d to off.", p_Pin);

	#endif // defined ENABLE_GPIO
}

// ControlHandle members

// A private constructor.
ControlHandle::ControlHandle(unsigned short p_UID) : m_UID(p_UID)
{
}

// Read a control config from JSON. 
//
// p_Object:	The JSON object representing a control config.
//
// Returns:		True if the config was read successfully, false otherwise.
//
bool ControlConfig::ReadFromJSON(rapidjson::Value const& p_Object)
{
	if (p_Object.IsObject() == false)
	{
		LoggerAddMessage("Control config cannot be parsed because it is not an object.");
		return false;
	}

	// We must have a control name.
	auto const l_NameIterator = p_Object.FindMember("name");

	if (l_NameIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Control config is missing a name.");
		return false;
	}

	if (l_NameIterator->value.IsString() == false)
	{
		LoggerAddMessage("Control config has a name but it is not a string.");
		return false;
	}
	
	// Copy no more than the amount of text the buffer can hold.
	strncpy(m_Name, l_NameIterator->value.GetString(), sizeof(m_Name) - 1);
	m_Name[sizeof(m_Name) - 1] = '\0';

	// We must have an up pin.
	auto const l_UpPinIterator = p_Object.FindMember("upPin");

	if (l_UpPinIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Control config is missing an up pin.");
		return false;
	}

	if (l_UpPinIterator->value.IsInt() == false)
	{
		LoggerAddMessage("Control config has an up pin, but it is not an integer.");
		return false;
	}

	m_UpGPIOPin = l_UpPinIterator->value.GetInt();

	// We must also have a down pin.
	auto const l_DownPinIterator = p_Object.FindMember("downPin");

	if (l_DownPinIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Control config is missing a down pin.");
		return false;
	}

	if (l_DownPinIterator->value.IsInt() == false)
	{
		LoggerAddMessage("Control config has a down pin, but it is not an integer.");
		return false;
	}

	m_DownGPIOPin = l_DownPinIterator->value.GetInt();

	// We might also have a moving duration.
	auto const l_MovingDurationIterator = p_Object.FindMember("movingDurationMS");

	if (l_MovingDurationIterator != p_Object.MemberEnd())
	{
		if (l_MovingDurationIterator->value.IsInt() == true)
		{
			m_MovingDurationMS = l_MovingDurationIterator->value.GetInt();
		}
	}

	return true;
}

// Control members

// Handle initialization.
//
// p_Config:	Configuration parameters for the control.
//
void Control::Initialize(ControlConfig const& p_Config)
{
	// Copy the name.
	strncpy(m_Name, p_Config.m_Name, ms_NameCapacity - 1);
	m_Name[ms_NameCapacity - 1] = '\0';
	
	m_State = STATE_IDLE;
	TimerGetCurrent(m_StateStartTime);
	m_DesiredAction = ACTION_STOPPED;

	// Setup the pins and set them to off.
	m_UpGPIOPin = p_Config.m_UpGPIOPin;
	m_DownGPIOPin = p_Config.m_DownGPIOPin;
	
	SetGPIOPinModeOutput(m_UpGPIOPin);
	SetGPIOPinModeOutput(m_DownGPIOPin);
	
	SetGPIOPinOff(m_UpGPIOPin);
	SetGPIOPinOff(m_DownGPIOPin);
	
	// Set the individual control moving duration.
	m_StandardMovingDurationMS = p_Config.m_MovingDurationMS;
	
	LoggerAddMessage("Initialized control \'%s\' with GPIO pins (up %i, "
		"down %i) and duration %i ms.", m_Name, m_UpGPIOPin, m_DownGPIOPin, m_StandardMovingDurationMS);
}

// Handle uninitialization.
//
void Control::Uninitialize()
{
	// Revert to input.
	SetGPIOPinModeInput(m_UpGPIOPin);
	SetGPIOPinModeInput(m_DownGPIOPin);
}

// Process a tick.
//
void Control::Process()
{
	// Handle state transitions.
	switch (m_State) 
	{
		case STATE_IDLE:
		{
			// Wait until moving is desired to transition.
			if (m_DesiredAction == ACTION_STOPPED) {
				break;
			}

			// Transition to moving.
			if (m_DesiredAction == ACTION_MOVING_UP)
			{
				m_State = STATE_MOVING_UP;

				// Set the pin to on.
				SetGPIOPinOn(m_UpGPIOPin);
			}
			else
			{
				m_State = STATE_MOVING_DOWN;

				// Set the pin to on.
				SetGPIOPinOn(m_DownGPIOPin);
			}
			
			// Play the notification.
			PlayNotification();
			
			// Record when the state transition timer began.
			TimerGetCurrent(m_StateStartTime);

			LoggerAddMessage("Control \"%s\": State transition from \"%s\" to \"%s\" triggered.", 
				m_Name, s_ControlStateNames[STATE_IDLE], s_ControlStateNames[m_State]);
		}
		break;

		case STATE_MOVING_UP:	// Fall through...
		case STATE_MOVING_DOWN:
		{
			// Get elapsed time since state start.
			Time l_CurrentTime;
			TimerGetCurrent(l_CurrentTime);

			auto const l_ElapsedTimeMS = TimerGetElapsedMilliseconds(m_StateStartTime, l_CurrentTime);

			// Get the action corresponding to this state, as well as the one for the opposite state.
			auto const l_MatchingAction = (m_State == STATE_MOVING_UP) ? ACTION_MOVING_UP : 
				ACTION_MOVING_DOWN;
			auto const l_OppositeAction = (m_State == STATE_MOVING_UP) ? ACTION_MOVING_DOWN : 
				ACTION_MOVING_UP;
			
			// Wait until the desired action no longer matches or the time limit has run out.
			if ((m_DesiredAction == l_MatchingAction) && (l_ElapsedTimeMS < m_MovingDurationMS))
			{
				break;
			}

			// We are about to change the state, so keep track of the old one.
			auto const l_OldState = m_State;
			
			if (m_DesiredAction == l_OppositeAction)
			{
				// Transition to the opposite state.
				auto const l_OppositeState = (m_State == STATE_MOVING_UP) ? STATE_MOVING_DOWN :
					STATE_MOVING_UP;
				m_State = l_OppositeState;

				// Flip the pins.
				auto const l_OldStatePin = (m_State == STATE_MOVING_DOWN) ? m_UpGPIOPin : 
					m_DownGPIOPin;
				auto const l_NewStatePin = (m_State == STATE_MOVING_DOWN) ? m_DownGPIOPin : 
					m_UpGPIOPin;
				SetGPIOPinOff(l_OldStatePin);
				SetGPIOPinOn(l_NewStatePin);
			}
			else
			{
				// Transition to cool down.
				m_State = STATE_COOL_DOWN;

				// Set the pins to off.
				SetGPIOPinOff(m_UpGPIOPin);
				SetGPIOPinOff(m_DownGPIOPin);
			}
			
			// Play the notification.
			PlayNotification();
			
			// Record when the state transition timer began.
			TimerGetCurrent(m_StateStartTime);

			LoggerAddMessage("Control \"%s\": State transition from \"%s\" to \"%s\" triggered.", 
				m_Name, s_ControlStateNames[l_OldState], s_ControlStateNames[m_State]);
		}
		break;

		case STATE_COOL_DOWN:
		{
			// Clear the desired action.
			m_DesiredAction = ACTION_STOPPED;

			// Get elapsed time since state start.
			Time l_CurrentTime;
			TimerGetCurrent(l_CurrentTime);

			auto const l_ElapsedTimeMS = TimerGetElapsedMilliseconds(m_StateStartTime, l_CurrentTime);

			// Wait until the time limit has run out.
			if (l_ElapsedTimeMS < ms_CoolDownDurationMS)
			{
				break;
			}

			// Transition to idle.
			m_State = STATE_IDLE;

			// Set the pins to off.
			SetGPIOPinOff(m_UpGPIOPin);
			SetGPIOPinOff(m_DownGPIOPin);

			LoggerAddMessage("Control \"%s\": State transition from \"%s\" to \"%s\" triggered.", 
				m_Name, s_ControlStateNames[STATE_COOL_DOWN], s_ControlStateNames[m_State]);
		}
		break;

		default:
		{
			LoggerAddMessage("Control \"%d\": Unrecognized state %s in Process()", m_State, m_Name);
		}
		break;
	}
}

// Set the desired action.
//
// p_DesiredAction:		The desired action.
// p_Mode:					The mode of the action.
// p_DurationPercent:	(Optional) The percent of the normal duration to perform the action
//								for.
//
void Control::SetDesiredAction(Actions p_DesiredAction, Modes p_Mode, unsigned int p_DurationPercent)
{
	static_assert(
		std::is_same_v<decltype(p_DurationPercent), decltype(CommandToken::m_Parameter)>,
		"Assert the type of `p_DurationPercent` is the same as the type of `CommandToken::m_Parameter`. "
		"Currently, the main purpose of `CommandToken::m_Parameter` is to be used as `p_DurationPercent`, "
		"so this assertion serves as a notification for if the types become unsynchronized.");

	m_DesiredAction = p_DesiredAction;
	m_Mode = p_Mode;

	if (m_Mode == MODE_TIMED)
	{
		// Set the current moving duration based on the requested percentage of the standard amount.
		auto const l_DurationFraction = std::min(p_DurationPercent, 100u) / 100.0f;
		m_MovingDurationMS =
			static_cast<unsigned int>(m_StandardMovingDurationMS * l_DurationFraction);
	}
	else
	{
		m_MovingDurationMS = ms_MaxMovingDurationMS;
	}

	LoggerAddMessage("Control \"%s\": Setting desired action to \"%s\" with mode \"%s\" and "
		"duration %i ms.", m_Name, s_ControlActionNames[p_DesiredAction], 
		s_ControlModeNames[p_Mode], m_MovingDurationMS);
}

// Enable or disable all controls.
//
// p_Enable:	Whether to enable or disable all controls.
//
void Control::Enable(bool p_Enable)
{
	if (p_Enable == false)
	{
		// Revert to input.
		//pinMode(ENABLE_GPIO_PIN, INPUT);

		LoggerAddMessage("Controls disabled.");
	}
	else
	{
		// Setup the pin and set it to off.
		//pinMode(ENABLE_GPIO_PIN, OUTPUT);
		//SetGPIOPinOff(ENABLE_GPIO_PIN);

		LoggerAddMessage("Controls enabled.");
	}
}

// Set the durations.
//
// p_MovingDurationMS:		Duration of the moving state (in milliseconds).
// p_CoolDownDurationMS:	Duration of the cool down state (in milliseconds).
//
void Control::SetDurations(unsigned int p_MovingDurationMS, unsigned int p_CoolDownDurationMS)
{
	ms_MaxMovingDurationMS = p_MovingDurationMS;
	ms_CoolDownDurationMS = p_CoolDownDurationMS;
	
	LoggerAddMessage("Control durations set to moving - %i ms, cool down - %i ms.", 
						  p_MovingDurationMS, p_CoolDownDurationMS);
}

// Attempt to get the handle of a control based on its name.
//
// p_Name:	The unique name of the control.
//
// Returns:	A handle to the control, or an invalid handle if one with the given name could not be found.
//
ControlHandle Control::GetHandle(char const* p_Name)
{
	ControlHandle l_Handle;
	
	auto const l_ControlCount = static_cast<unsigned short>(s_Controls.size());
	for (unsigned short l_ControlIndex = 0; l_ControlIndex < l_ControlCount; l_ControlIndex++)
	{
		auto const& l_Control = s_Controls[l_ControlIndex];
		
		// Look for a control with the matching name.
		if (strcmp(l_Control.GetName(), p_Name) != 0)
		{
			continue;
		}
		
		// Found it, return the handle.
		l_Handle.m_UID = l_ControlIndex;
		break;
	}
	
	// Return the handle we found, or didn't.
	return l_Handle;
}

// Look up a control from its handle.
//
// p_Handle:	A handle to the control.
//
// Returns:		The control, or null if the handle is not valid.
//
Control* Control::GetFromHandle(ControlHandle const& p_Handle)
{
	// Sanity checking.
	if (p_Handle.IsValid() == false)
	{
		return nullptr;
	}
	
	auto const l_ControlCount = static_cast<unsigned short>(s_Controls.size());
	if (p_Handle.m_UID >= l_ControlCount)
	{
		return nullptr;
	}
	
	// Seems we have a valid handle, return the control.
	return &(s_Controls[p_Handle.m_UID]);
}
		
// Play a notification for the state.
//
void Control::PlayNotification()
{
	// Do not play a notification if the mode is manual.
	if (m_Mode == MODE_MANUAL)
	{
		return;
	}
	
	// Build the notification name.
	static constexpr unsigned int l_NotificationNameCapacity = 128;
	char l_NotificationName[l_NotificationNameCapacity];
	snprintf(l_NotificationName, l_NotificationNameCapacity, "%s_%s", m_Name,
		s_ControlStateNotificationNames[m_State]);

	NotificationPlay(l_NotificationName);
}

// ControlAction members

// A constructor for emplacing.
// 
ControlAction::ControlAction(char const* p_ControlName, Control::Actions p_Action)
	: m_Action(p_Action)
{
	// Copy the control name.
	strncpy(m_ControlName, p_ControlName, ms_ControlNameCapacity - 1);
	m_ControlName[ms_ControlNameCapacity - 1] = '\0';
}

// Try to find a control action that matches the input text.
//
// p_Action:		(Output) The action that was found.
// p_InputText:	The name of the action to look for. 
//
// Returns:		True if the action was found, false otherwise.
//
auto GetControlActionFromString(Control::Actions& p_Action, char const* p_InputText)
{
	// The names of the actions.
	static char const* const s_ActionNames[] =
	{
		"stop",	// ACTION_STOPPED
		"up",		// ACTION_MOVING_UP
		"down",	// ACTION_MOVING_DOWN
	};

	// Try to find a control action that matches this text.
	auto const l_ActionCount = Control::Actions::NUM_ACTIONS;
	for (unsigned int l_ActionIndex = 0; l_ActionIndex < l_ActionCount; l_ActionIndex++)
	{
		// Compare the text to the action name.
		auto const* l_ActionName = s_ActionNames[l_ActionIndex];
			
		if (strncmp(p_InputText, l_ActionName, strlen(l_ActionName)) != 0)			
		{
			continue;
		}
			
		// We found the action, so return it.
		p_Action = static_cast<Control::Actions>(l_ActionIndex);
		return true;
	}
		
	return false;
}

// Read a control action from JSON. 
//
// p_Object:	The JSON object representing a control action.
//
// Returns:		True if the action was read successfully, false otherwise.
//
bool ControlAction::ReadFromJSON(rapidjson::Value const& p_Object)
{
	if (p_Object.IsObject() == false)
	{
		LoggerAddMessage("Control action cannot be parsed because it is not an object.");
		return false;
	}

	// We must have a control name.
	auto const l_ControlIterator = p_Object.FindMember("control");

	if (l_ControlIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Control action is missing a control name.");
		return false;
	}

	if (l_ControlIterator->value.IsString() == false)
	{
		LoggerAddMessage("Control action has a control name, but it is not a string.");
		return false;
	}
	
	// Copy no more than the amount of text the buffer can hold.
	strncpy(m_ControlName, l_ControlIterator->value.GetString(), sizeof(m_ControlName) - 1);
	m_ControlName[sizeof(m_ControlName) - 1] = '\0';

	// We must also have an action.
	auto const l_ActionIterator = p_Object.FindMember("action");

	if (l_ActionIterator == p_Object.MemberEnd())
	{
		LoggerAddMessage("Control action does not have an action.");
		return false;
	}

	if (l_ActionIterator->value.IsString() == false)
	{
		LoggerAddMessage("Control action has an action, but it is not a string.");
		return false;
	}

	// Try to get the corresponding action.
	if (GetControlActionFromString(m_Action, l_ActionIterator->value.GetString()) == false)
	{
		LoggerAddMessage("Control action has an unrecognized action.");
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
// p_Configs:		Configuration parameters for the controls to add.
// p_EnableGPIO:	Whether to turn on GPIO or not.
//
void ControlsInitialize(std::vector<ControlConfig> const& p_Configs, bool const p_EnableGPIO)
{
	#if defined ENABLE_GPIO
	
		s_EnableGPIO = p_EnableGPIO;

		if (s_EnableGPIO == true)
		{
			LoggerAddMessage("Initializing GPIO support...");
	
			if (gpioInitialise() < 0)
			{
				LoggerAddMessage("\tfailed");
				return;
			}

			LoggerAddMessage("\tsucceeded");
		}
		else
		{
			LoggerAddMessage("GPIO support not enabled, initialization skipped.");
		}
		
		LoggerAddEmptyLine();

	#endif // defined ENABLE_GPIO

	for (auto const& l_Config : p_Configs)
	{
		ControlsCreateControl(l_Config);
	}
}

// Uninitialize all of the controls.
//
void ControlsUninitialize()
{
	for (auto& l_Control : s_Controls)
	{
		l_Control.Uninitialize();
	}
	
	// Get rid of all of the controls.
	s_Controls.clear();

	#if defined ENABLE_GPIO
	
		// Uninitialize GPIO support.
		if (s_EnableGPIO == true)
		{
			gpioTerminate();
		}
	
	#endif // defined ENABLE_GPIO
}

// Process all of the controls.
//
void ControlsProcess()
{
	for (auto& l_Control : s_Controls)
	{
		l_Control.Process();
	}	
}

// Create a new control with the provided config. Control names must be unique.
//
// p_Config:	Configuration parameters for the control.
//
// Returns:		True if the control was successfully created, false otherwise.
//
bool ControlsCreateControl(ControlConfig const& p_Config)
{
	// Check to see whether a control with this name already exists.
	if (Control::GetHandle(p_Config.m_Name).IsValid() == true)
	{		
		LoggerAddMessage("Control with name \"%s\" already exists.", p_Config.m_Name);
		return false;
	}
	
	// Add a new control.
	s_Controls.emplace_back(Control());
	
	// Then, initialize it.
	auto& l_Control = s_Controls.back();
	l_Control.Initialize(p_Config);
	
	return true;
}

// Stop all of the controls.
//
void ControlsStopAll()
{
	for (auto& l_Control : s_Controls)
	{
		l_Control.SetDesiredAction(Control::ACTION_STOPPED, Control::MODE_MANUAL);
	}
}
