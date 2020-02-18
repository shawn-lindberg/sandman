#include "control.h"

#include <stdio.h>
#include <string.h>
#include <vector>

#include "logger.h"
#include "sound.h"
#include "timer.h"

#include "wiringPi.h"

#define DATADIR		AM_DATADIR

// Constants
//

// Maximum duration of the moving state.
#define MAX_MOVING_STATE_DURATION_MS	(100 * 1000) // 100 sec.

// Maximum duration of the cool down state.
#define MAX_COOL_DOWN_STATE_DURATION_MS	(50 * 1000) // 50 sec.

// Time between commands.
//#define COMMAND_INTERVAL_MS				(2 * 1000) // 2 sec.

// The pin to use for enabling controls.
#define ENABLE_GPIO_PIN					(7)

// GPIO values for on and off respectively.
#define CONTROL_ON_GPIO_VALUE			(LOW)
#define CONTROL_OFF_GPIO_VALUE			(HIGH)

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

// The file names of the states.
static char const* const s_ControlStateFileNames[] =
{
	"",				// STATE_IDLE
	"moving_up",	// STATE_MOVING_UP
	"moving_down",	// STATE_MOVING_DOWN
	"stop",			// STATE_COOL_DOWN
};

// A list of registered controls.
static std::vector<Control> s_Controls;

// Control members

unsigned int Control::ms_MovingDurationMS = MAX_MOVING_STATE_DURATION_MS;
unsigned int Control::ms_CoolDownDurationMS = MAX_COOL_DOWN_STATE_DURATION_MS;

// Functions
//

template<class T>
T const& Min(T const& p_A, T const& p_B)
{
	return (p_A < p_B) ? p_A : p_B;
}

// Set the given GPIO pin to the "on" value.
//
// p_Pin:	The GPIO pin to set the value of.
//
void SetGPIOPinOn(int p_Pin)
{
	digitalWrite(p_Pin, CONTROL_ON_GPIO_VALUE);
}

// Set the given GPIO pin to the "off" value.
//
// p_Pin:	The GPIO pin to set the value of.
//
void SetGPIOPinOff(int p_Pin)
{
	digitalWrite(p_Pin, CONTROL_OFF_GPIO_VALUE);
}

// Control members

// Handle initialization.
//
// p_Config:	Configuration parameters for the control.
//
void Control::Initialize(ControlConfig const& p_Config)
{
	// Copy the name.
	unsigned int const l_AmountToCopy = 
		Min(static_cast<unsigned int>(NAME_CAPACITY) - 1, strlen(p_Config.m_Name));
	strncpy(m_Name, p_Config.m_Name, l_AmountToCopy);
	m_Name[l_AmountToCopy] = '\0';
	
	m_State = STATE_IDLE;
	TimerGetCurrent(m_StateStartTime);
	m_DesiredAction = ACTION_STOPPED;

	// Setup the pins and set them to off.
	m_UpGPIOPin = p_Config.m_UpGPIOPin;
	pinMode(m_UpGPIOPin, OUTPUT);
	SetGPIOPinOff(m_UpGPIOPin);
	
	m_DownGPIOPin = p_Config.m_DownGPIOPin;
	pinMode(m_DownGPIOPin, OUTPUT);
	SetGPIOPinOff(m_DownGPIOPin);
	
	// Set the individual control moving duration.
	m_MovingDurationMS = p_Config.m_MovingDurationMS;
	
	LoggerAddMessage("Initialized control \'%s\' with GPIO pins (up %i, "
		"down %i) and duration %i ms.", m_Name, m_UpGPIOPin, m_DownGPIOPin, m_MovingDurationMS);
}

// Handle uninitialization.
//
void Control::Uninitialize()
{
	// Revert to input.
	pinMode(m_UpGPIOPin, INPUT);
	pinMode(m_DownGPIOPin, INPUT);
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
	ms_MovingDurationMS = p_MovingDurationMS;
	ms_CoolDownDurationMS = p_CoolDownDurationMS;
	
	LoggerAddMessage("Control durations set to moving - %i ms, cool down - %i ms.", p_MovingDurationMS, p_CoolDownDurationMS);
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
			
			// Queue the sound.
			QueueSound();
			
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

			// Get the duration based on the mode.
			auto const l_MovingDurationMS = (m_Mode == MODE_TIMED) ? m_MovingDurationMS : 
				ms_MovingDurationMS;
			
			// Wait until the desired action no longer matches or the time limit has run out.
			if ((m_DesiredAction == l_MatchingAction) && (l_ElapsedTimeMS < l_MovingDurationMS))
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
			
			// Queue the sound.
			QueueSound();

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
			LoggerAddMessage("Control \"%s\": Unrecognized state %d in Process()", m_State, m_Name);
		}
		break;
	}
}

// Set the desired action.
//
// p_DesiredAction:	The desired action.
// p_Mode:			The mode of the action.
//
void Control::SetDesiredAction(Actions p_DesiredAction, Modes p_Mode)
{
	m_DesiredAction = p_DesiredAction;
	m_Mode = p_Mode;

	LoggerAddMessage("Control \"%s\": Setting desired action to \"%s\" with mode \"%s\".", m_Name, 
		s_ControlActionNames[p_DesiredAction], s_ControlModeNames[p_Mode]);
}

// Queue sound for the state.
//
void Control::QueueSound()
{
	// Build the file name.
	unsigned int l_SoundFileNameCapacity = 128;
	char l_SoundFileName[l_SoundFileNameCapacity];
	snprintf(l_SoundFileName, l_SoundFileNameCapacity, DATADIR "audio/%s_%s.wav", m_Name,
		s_ControlStateFileNames[m_State]);

	SoundAddToQueue(l_SoundFileName);
}


// Functions
//

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
	if (ControlsFindControl(p_Config.m_Name) != nullptr)
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

// Try to find the control with the given name.
//
// p_Name:	The name of the control being searched for.
//
// Returns:	A pointer to the control, or null if it was not found.
//
Control* ControlsFindControl(char const* p_Name)
{
	for (auto& l_Control : s_Controls)
	{
		// Look for a control with the matching name.
		if (strcmp(l_Control.GetName(), p_Name) != 0)
		{
			continue;
		}
		
		// Found it.
		return &l_Control;
	}
	
	return nullptr;
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
