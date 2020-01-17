#include "control.h"

#include <stdio.h>
#include <string.h>

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
#define COMMAND_INTERVAL_MS				(2 * 1000) // 2 sec.

// The pin to use for enabling controls.
#define ENABLE_GPIO_PIN					(7)

// GPIO values for on and off respectively.
#define CONTROL_ON_GPIO_VALUE			(LOW)
#define CONTROL_OFF_GPIO_VALUE			(HIGH)

// Locals
//

// The names of the actions.
char const* const s_ControlActionNames[] =
{
	"stopped",		// ACTION_STOPPED
	"moving up",	// ACTION_MOVING_UP
	"moving down",	// ACTION_MOVING_DOWN
};

// The names of the states.
char const* const s_ControlStateNames[] =
{
	"idle",			// STATE_IDLE
	"moving up",	// STATE_MOVING_UP
	"moving down",	// STATE_MOVING_DOWN
	"cool down",	// STATE_COOL_DOWN
};

// The file names of the states.
char const* const s_ControlStateFileNames[] =
{
	"",				// STATE_IDLE
	"moving_up",	// STATE_MOVING_UP
	"moving_down",	// STATE_MOVING_DOWN
	"stop",			// STATE_COOL_DOWN
};

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
// p_Name:			The name.
// p_UpGPIOPin:		The GPIO pin to use to move up.
// p_DownGPIOPin:	The GPIO pin to use to move down.
//
void Control::Initialize(char const* p_Name, int p_UpGPIOPin, int p_DownGPIOPin)
{
	// Copy the name.
	unsigned int const l_AmountToCopy = 
		Min(static_cast<unsigned int>(NAME_CAPACITY) - 1, strlen(p_Name));
	strncpy(m_Name, p_Name, l_AmountToCopy);
	m_Name[l_AmountToCopy] = '\0';
	
	m_State = STATE_IDLE;
	TimerGetCurrent(m_StateStartTime);
	m_DesiredAction = ACTION_STOPPED;

	// Setup the pins and set them to off.
	m_UpGPIOPin = p_UpGPIOPin;
	pinMode(p_UpGPIOPin, OUTPUT);
	SetGPIOPinOff(p_UpGPIOPin);
	
	m_DownGPIOPin = p_DownGPIOPin;
	pinMode(p_DownGPIOPin, OUTPUT);
	SetGPIOPinOff(p_DownGPIOPin);
	
	LoggerAddMessage("Initialized control \'%s\' with GPIO pins (up %i, "
		"down %i)", m_Name, m_UpGPIOPin, m_DownGPIOPin);
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
		pinMode(ENABLE_GPIO_PIN, INPUT);

		LoggerAddMessage("Controls disabled.");
	}
	else
	{
		// Setup the pin and set it to off.
		pinMode(ENABLE_GPIO_PIN, OUTPUT);
		SetGPIOPinOff(ENABLE_GPIO_PIN);

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

		case STATE_MOVING_UP:
		{
			// Get elapsed time since state start.
			Time l_CurrentTime;
			TimerGetCurrent(l_CurrentTime);

			float l_ElapsedTimeMS = TimerGetElapsedMilliseconds(m_StateStartTime, l_CurrentTime);

			// Wait until moving up isn't desired or the time limit has run out.
			if ((m_DesiredAction == ACTION_MOVING_UP) && (l_ElapsedTimeMS < ms_MovingDurationMS))
			{
				break;
			}

			if (m_DesiredAction == ACTION_MOVING_DOWN)
			{
				// Transition to moving down.
				m_State = STATE_MOVING_DOWN;

				// Flip the pins.
				SetGPIOPinOff(m_UpGPIOPin);
				SetGPIOPinOn(m_DownGPIOPin);
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
				m_Name, s_ControlStateNames[STATE_MOVING_UP], s_ControlStateNames[m_State]);
		}
		break;

		case STATE_MOVING_DOWN:
		{
			// Get elapsed time since state start.
			Time l_CurrentTime;
			TimerGetCurrent(l_CurrentTime);

			float l_ElapsedTimeMS = TimerGetElapsedMilliseconds(m_StateStartTime, l_CurrentTime);

			// Wait until moving down isn't desired or the time limit has run out.
			if ((m_DesiredAction == ACTION_MOVING_DOWN) && (l_ElapsedTimeMS < ms_MovingDurationMS))
			{
				break;
			}

			if (m_DesiredAction == ACTION_MOVING_UP)
			{
				// Transition to moving up.
				m_State = STATE_MOVING_UP;

				// Flip the pins.
				SetGPIOPinOn(m_UpGPIOPin);
				SetGPIOPinOff(m_DownGPIOPin);
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
				m_Name, s_ControlStateNames[STATE_MOVING_DOWN], s_ControlStateNames[m_State]);
		}
		break;

		case STATE_COOL_DOWN:
		{
			// Clear the desired action.
			m_DesiredAction = ACTION_STOPPED;

			// Get elapsed time since state start.
			Time l_CurrentTime;
			TimerGetCurrent(l_CurrentTime);

			float l_ElapsedTimeMS = TimerGetElapsedMilliseconds(m_StateStartTime, l_CurrentTime);

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
//
void Control::SetDesiredAction(Actions p_DesiredAction)
{
	m_DesiredAction = p_DesiredAction;

	LoggerAddMessage("Control \"%s\": Setting desired action to \"%s\".", m_Name, 
		s_ControlActionNames[p_DesiredAction]);
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
