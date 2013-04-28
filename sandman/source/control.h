#pragma once

#include "timer.h"

// Types
//

// Forward declaraction.
class SerialConnection;

// States a control may be in.
enum ControlState
{
	CONTROL_STATE_IDLE = 0,
	CONTROL_STATE_MOVING,
	CONTROL_STATE_COOL_DOWN,    // A delay after moving before moving can occur again.
};

// An individual control.
class Control
{
	public:

		#if defined (USE_SERIAL_CONNECTION)

			// Handle initialization.
			//
			// p_SerialConn:	The serial connection to the micro.
			// p_CommandString:	The command string to send to the micro.
			//
			void Initialize(SerialConnection* p_SerialConn, char const* p_CommandString);

		#else
		
			// Handle initialization.
			//
			// p_GPIOPin:	The GPIO pin to use.
			//
			void Initialize(int p_GPIOPin);

		#endif // defined (USE_SERIAL_CONNECTION)

		// Handle uninitialization.
		//
		void Uninitialize();

		// Enable or disable all controls.
		//
		// p_Enable:	Whether to enable or disable all controls.
		//
		static void Enable(bool p_Enable);
		
		// Process a tick.
		//
		void Process();

		// Set moving desired for the next tick.
		//
		// p_MovingDesired:	Whether moving is desired.
		//
		void SetMovingDesired(bool p_MovingDesired);

	private:

		// The control state.
		ControlState m_State;

		// A record of when the state transition timer began.
		Time m_StateStartTime;

		// Whether movement is desired.
		bool m_MovingDesired;

		#if defined (USE_SERIAL_CONNECTION)

			// Serial connection to micro.
			SerialConnection* m_SerialConn;

			// The command string to send to the micro.
			char const* m_CommandString;

			// A record of when the last command was sent.
			Time m_LastCommandTime;

		#else
		
			// The GPIO pin to use.
			int m_GPIOPin;
		
		#endif // defined (USE_SERIAL_CONNECTION)
};
