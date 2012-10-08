#pragma once

#include <stdint.h>

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

		// Handle initialization.
		//
		// p_SerialConn:	The serial connection to the micro.
		// p_CommandString:	The command string to send to the micro.
		//
		void Initialize(SerialConnection* p_SerialConn, char const* p_CommandString);

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
		uint64_t m_StateStartTimeTicks;

		// Whether movement is desired.
		bool m_MovingDesired;

		#if defined (USE_SERIAL_CONNECTION)

			// Serial connection to micro.
			SerialConnection* m_SerialConn;

		#endif // defined (USE_SERIAL_CONNECTION)

		// The command string to send to the micro.
		char const* m_CommandString;

		// A record of when the last command was sent.
		uint64_t m_LastCommandTimeTicks;
};
