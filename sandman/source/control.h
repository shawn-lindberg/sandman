#pragma once

#include "timer.h"

// Types
//

// An individual control.
class Control
{
	public:

		// Actions a control may be desired to perform.
		enum Actions
		{
			ACTION_STOPPED = 0,
			ACTION_MOVING_UP,
			ACTION_MOVING_DOWN,
			
			NUM_ACTIONS,
		};

		// Handle initialization.
		//
		// p_Name:			The name.
		// p_UpGPIOPin:		The GPIO pin to use to move up.
		// p_DownGPIOPin:	The GPIO pin to use to move down.
		//
		void Initialize(char const* p_Name, int p_UpGPIOPin, int p_DownGPIOPin);

		// Handle uninitialization.
		//
		void Uninitialize();

		// Enable or disable all controls.
		//
		// p_Enable:	Whether to enable or disable all controls.
		//
		static void Enable(bool p_Enable);
		
		// Set the durations.
		//
		// p_MovingDurationMS:		Duration of the moving state (in milliseconds).
		// p_CoolDownDurationMS:	Duration of the cool down state (in milliseconds).
		//
		static void SetDurations(unsigned int p_MovingDurationMS, unsigned int p_CoolDownDurationMS);
		
		// Process a tick.
		//
		void Process();

		// Set the desired action.
		//
		// p_DesiredAction:	The desired action.
		//
		void SetDesiredAction(Actions p_DesiredAction);

		// Get the name.
		//
		char const* GetName() const
		{
			return m_Name;
		}
		
	private:

		// Constants.
		enum
		{
			NAME_CAPACITY = 32,
		};

		// States a control may be in.
		enum State
		{
			STATE_IDLE = 0,
			STATE_MOVING_UP,
			STATE_MOVING_DOWN,
			STATE_COOL_DOWN,    // A delay after moving before moving can occur again.
		};

		// Queue sound for the state.
		//
		void QueueSound();
		
		// The name of the control.
		char m_Name[NAME_CAPACITY];
		
		// The control state.
		State m_State;

		// A record of when the state transition timer began.
		Time m_StateStartTime;

		// The desired action.
		Actions m_DesiredAction;

		// The GPIO pins to use.
		int m_UpGPIOPin;
		int m_DownGPIOPin;

		// Maximum duration of the moving state (in milliseconds).
		static unsigned int ms_MovingDurationMS;
		
		// Maximum duration of the cool down state (in milliseconds).
		static unsigned int ms_CoolDownDurationMS;	
};
