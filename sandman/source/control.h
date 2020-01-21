#pragma once

#include "timer.h"

// Types
//

// Configuration parameters to initialize a control.
struct ControlConfig
{
	// The name of the control.
	char const* m_Name;
		
	// The GPIO pins to use.
	int m_UpGPIOPin;
	int m_DownGPIOPin;
	
	// The duration of the moving state (in milliseconds) for this control.
	unsigned int m_MovingDurationMS;
};

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

		// Movement modes, either manual or timed for now.
		enum Modes
		{
			MODE_MANUAL = 0, 
			MODE_TIMED,
		};
		
		// Handle initialization.
		//
		// p_Config:	Configuration parameters for the control.
		//
		void Initialize(const ControlConfig& p_Config);

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
		// p_Mode:			The mode of the action.
		//
		void SetDesiredAction(Actions p_DesiredAction, Modes p_Mode);

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

		// The control movement mode.
		Modes m_Mode;
		
		// The GPIO pins to use.
		int m_UpGPIOPin;
		int m_DownGPIOPin;
		
		// The duration of the moving state (in milliseconds) for this control.
		unsigned int m_MovingDurationMS;

		// Maximum duration of the moving state (in milliseconds).
		static unsigned int ms_MovingDurationMS;
		
		// Maximum duration of the cool down state (in milliseconds).
		static unsigned int ms_CoolDownDurationMS;	
};
