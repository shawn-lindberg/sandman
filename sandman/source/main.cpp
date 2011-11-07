#include <conio.h>
#include <stdio.h>
#include <Windows.h>

// TODO:
//
// - Move TODO out of source. ;]
// - Timing (QueryPerformanceCounter?) and testing.
// - Cleaning up a bit.
// - Fix reading and printing from serial.
// - Implement a stop command.

// Constants
//

// Maximum duration of the moving state.
#define MAX_MOVING_STATE_DURATION_MS	(100 * 1000) // 100 sec.

// Maximum duration of the cool down state.
#define MAX_COOL_DOWN_STATE_DURATION_MS	(50 * 1000) // 50 sec.

// Time between commands.
#define COMMAND_INTERVAL_MS				(2 * 1000) // 2 sec.

// Types
//

// Hides the details of a serial connection.
//
class SerialConnection
{
	public:

		// Initialize the connection.
		//
		// p_PortName:	The name of the serial port.
		//
		// returns:		True for success, false otherwise.
		//
		bool Initialize(char const* const p_PortName);

		// Uninitialize the connection.
		//
		void Uninitialize();

		// Reads a string from the serial connection.
		//
		// p_NumBytesRead:				(Output) The number of bytes actually read.
		// p_StringBuffer:				(Output) A buffer to hold the string that is read.
		// p_StringBufferCapacityBytes:	The capacity of the string buffer.
		//
		// returns:						True for success, false otherwise.
		//
		bool SerialConnection::ReadString(unsigned long int& p_NumBytesRead, char* p_StringBuffer, 
			unsigned long int const p_StringBufferCapacityBytes);

		// Write a string to the serial connection.
		//
		// p_NumBytesWritten:	(Output) The number of bytes actually written.
		// p_String:			The string to write.
		//
		// returns:				True for success, false otherwise.
		//
		bool WriteString(unsigned long int& p_NumBytesWritten, char const* const p_Text);

	private:

		// Constants.
		enum
		{
			PORT_NAME_MAX_LENGTH = 16,
		};

		// Mostly for debugging.
		char m_PortName[PORT_NAME_MAX_LENGTH];

		// Windows file handle.
		HANDLE m_Handle;
};

// Types of controls.
enum ControlTypes
{
	CONTROL_HEAD_UP = 0,
	CONTROL_HEAD_DOWN,
	CONTROL_KNEE_UP,
	CONTROL_KNEE_DOWN,
	CONTROL_ELEVATION_UP,
	CONTROL_ELEVATION_DOWN,

	NUM_CONTROL_TYPES,
};

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
		void MovingDesired();

	private:

		// The control state.
		ControlState m_State;

		// A record of when the state transition timer began.
		__int64 m_StateStartTimeTicks;

		// Whether movement is desired.
		bool m_MovingDesired;

		// Serial connection to micro.
		SerialConnection* m_SerialConn;

		// The command string to send to the micro.
		char const* m_CommandString;

		// A record of when the last command was sent.
		__int64 m_LastCommandTimeTicks;
};

// Locals
//

// The command string to send to the micro for each control.
static char const* const s_ControlCommandStrings[] =
{
	"hu ",	// CONTROL_HEAD_UP
	"hd ",	// CONTROL_HEAD_DOWN
	"ku ",	// CONTROL_KNEE_UP
	"kd ",	// CONTROL_KNEE_DOWN
	"eu ",	// CONTROL_ELEVATION_UP
	"ed ",	// CONTROL_ELEVATION_DOWN
};

// The controls.
static Control s_Controls[NUM_CONTROL_TYPES];

// Functions
//

// SerialConnection members

// Initialize the connection.
//
// p_PortName:	The name of the serial port.
//
// returns:		True for success, false otherwise.
//
bool SerialConnection::Initialize(char const* const p_PortName)
{
	// Copy the port name.
	strncpy_s(m_PortName, p_PortName, PORT_NAME_MAX_LENGTH);
	m_PortName[PORT_NAME_MAX_LENGTH - 1] = 0;

	// Open the serial port.
	m_Handle = CreateFile(p_PortName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (m_Handle == INVALID_HANDLE_VALUE)
	{
		printf("Failed to open serial port %s: error code 0x%x\n", m_PortName, GetLastError());
		return false;
	}
	
	// Set some serial params.
	DCB l_SerialParams;
	if (GetCommState(m_Handle, &l_SerialParams) == false)
	{
		printf("Failed to get serial port %s params: error code 0x%x\n", m_PortName, GetLastError());
		CloseHandle(m_Handle);
		return false;
	}

	// Just set baud rate.
	l_SerialParams.BaudRate = CBR_9600;

	if (SetCommState(m_Handle, &l_SerialParams) == false)
	{
		printf("Failed to set serial port %s params: error code 0x%x\n", m_PortName, GetLastError());
		CloseHandle(m_Handle);
		return false;
	}

	// Set some serial timeouts.  We want non-blocking reads and normal writes.
	COMMTIMEOUTS l_SerialTimeouts;
	{
		l_SerialTimeouts.ReadIntervalTimeout = MAXDWORD;
		l_SerialTimeouts.ReadTotalTimeoutConstant = 0;
		l_SerialTimeouts.ReadTotalTimeoutMultiplier = 0;
		l_SerialTimeouts.WriteTotalTimeoutConstant = 50;
		l_SerialTimeouts.WriteTotalTimeoutMultiplier = 10;
	}

	if (SetCommTimeouts(m_Handle, &l_SerialTimeouts) == false)
	{
		printf("Failed to set serial port %s timeouts: error code 0x%x\n", m_PortName, GetLastError());
		CloseHandle(m_Handle);
		return false;
	}

	return true;
}

// Uninitialize the connection.
//
void SerialConnection::Uninitialize()
{
	if (m_Handle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	CloseHandle(m_Handle);
	m_Handle = INVALID_HANDLE_VALUE;
}

// Reads a string from the serial connection.
//
// p_NumBytesRead:				(Output) The number of bytes actually read.
// p_StringBuffer:				(Output) A buffer to hold the string that is read.
// p_StringBufferCapacityBytes:	The capacity of the string buffer.
//
// returns:						True for success, false otherwise.
//
bool SerialConnection::ReadString(unsigned long int& p_NumBytesRead, char* p_StringBuffer,
	unsigned long int const p_StringBufferCapacityBytes)
{
	p_NumBytesRead = 0;
	
	// Sanity check.
	if (m_Handle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// Read serial port text, if there is any.
	if (ReadFile(m_Handle, p_StringBuffer, p_StringBufferCapacityBytes - 1, &p_NumBytesRead, NULL) == false)
	{
		printf("Failed to read from serial port %s: error code 0x%x\n", m_PortName, GetLastError());
		return false;
	}

	// Terminate it.
	p_StringBuffer[p_NumBytesRead] = 0;

	return true;
}

// Write a string to the serial connection.
//
// p_NumBytesWritten:	(Output) The number of bytes actually written.
// p_String:			The string to write.
//
// returns:				True for success, false otherwise.
//
bool SerialConnection::WriteString(unsigned long int& p_NumBytesWritten, char const* const p_String)
{
	p_NumBytesWritten = 0;

	// Sanity check.
	if (m_Handle == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// Write the string.
	unsigned long int const l_NumBytesToWrite = strlen(p_String);

	if (WriteFile(m_Handle, p_String, l_NumBytesToWrite, &p_NumBytesWritten, NULL) == false)
	{
		printf("Failed to write to serial port %s: error code 0x%x\n", m_PortName, GetLastError());
		return false;
	}

	// Did we write it all?
	if (p_NumBytesWritten != l_NumBytesToWrite)
	{
		printf("Failed to write full data to serial port %s: error code 0x%x\n", m_PortName);
	}

	return true;
}

// Get the current time in ticks.
//
__int64 TimerGetTicks()
{
	// NOTE - STL 2011/10/31 - This can fail.
	LARGE_INTEGER l_Ticks;
	QueryPerformanceCounter(&l_Ticks);

	return l_Ticks.QuadPart;
}

// Get the elapsed time in milliseconds between to times.
//
// p_StartTicks:	Start time in ticks.
// p_EndTicks:		End time in ticks.
//
float TimerGetElapsedMilliseconds(__int64 const p_StartTicks, __int64 const p_EndTicks)
{
	// Get tick frequency in ticks/second.
	// NOTE - STL 2011/10/31 - This can fail.
	LARGE_INTEGER l_Frequency;
	QueryPerformanceFrequency(&l_Frequency);

	float l_ElapsedTimeMS = (p_EndTicks - p_StartTicks) * (1000.0f / l_Frequency.QuadPart); 
	return l_ElapsedTimeMS;
}

// Control members

// Handle initialization.
//
// p_SerialConn:	The serial connection to the micro.
// p_CommandString:	The command string to send to the micro.
//
void Control::Initialize(SerialConnection* p_SerialConn, char const* p_CommandString)
{
	m_State = CONTROL_STATE_IDLE;
	m_StateStartTimeTicks = 0;
	m_MovingDesired = false;
	m_SerialConn = p_SerialConn;
	m_CommandString = p_CommandString;
	m_LastCommandTimeTicks = TimerGetTicks();
}

// Process a tick.
//
void Control::Process()
{
	// Handle state transitions.
	switch (m_State) 
	{
		case CONTROL_STATE_IDLE:
		{
			// Wait until moving is desired to transition.
			if (m_MovingDesired != true) {
				break;
			}

			// Transition to moving.
			m_State = CONTROL_STATE_MOVING;

			// Record when the state transition timer began.
			m_StateStartTimeTicks = TimerGetTicks();

			printf("Control @ 0x%x: State transition from idle to moving triggered.\n",	reinterpret_cast<unsigned int>(this));           
		}
		break;

		case CONTROL_STATE_MOVING:
		{
			// Get elapsed time since state start.
			__int64 l_CurrentTimeTicks = TimerGetTicks();

			float l_ElapsedTimeMS = TimerGetElapsedMilliseconds(m_StateStartTimeTicks, l_CurrentTimeTicks);

			// Wait until moving isn't desired or the time limit has run out.
			if ((m_MovingDesired == true) && (l_ElapsedTimeMS < MAX_MOVING_STATE_DURATION_MS))
			{
				break;
			}

			// Transition to cool down.
			m_State = CONTROL_STATE_COOL_DOWN;

			// Record when the state transition timer began.
			m_StateStartTimeTicks = TimerGetTicks();

			printf("Control @ 0x%x: State transition from moving to cool down triggered.\n",
				reinterpret_cast<unsigned int>(this));                      
		}
		break;

		case CONTROL_STATE_COOL_DOWN:
		{
			// Clear moving desired.
			m_MovingDesired = false;

			// Get elapsed time since state start.
			__int64 l_CurrentTimeTicks = TimerGetTicks();

			float l_ElapsedTimeMS = TimerGetElapsedMilliseconds(m_StateStartTimeTicks, l_CurrentTimeTicks);

			// Wait until the time limit has run out.
			if (l_ElapsedTimeMS < MAX_COOL_DOWN_STATE_DURATION_MS)
			{
				break;
			}

			// Transition to idle.
			m_State = CONTROL_STATE_IDLE;

			printf("Control @ 0x%x: State transition from cool down to idle triggered.\n",
				reinterpret_cast<unsigned int>(this));                                 
		}
		break;

		default:
		{
			printf("Control @ 0x%x: Unrecognized state %d in Process()\n", m_State, reinterpret_cast<unsigned int>(this));                      
		}
		break;
	}

	// Manipulate micro based on state.
	if ((m_SerialConn == NULL) || (m_CommandString == NULL))
	{
		return;
	}

	if (m_State == CONTROL_STATE_MOVING)
	{
		// Get elapsed time since last command.
		__int64 l_CurrentTimeTicks = TimerGetTicks();

		float l_ElapsedTimeMS = TimerGetElapsedMilliseconds(m_LastCommandTimeTicks, l_CurrentTimeTicks);

		// See if enough time has passed since last command.
		if (l_ElapsedTimeMS < COMMAND_INTERVAL_MS)
		{
			return;
		}

		unsigned long int l_NumBytesWritten = 0;
		m_SerialConn->WriteString(l_NumBytesWritten, m_CommandString);

		// Record when the last command was sent.
		m_LastCommandTimeTicks = TimerGetTicks();
	}
}

// Set moving desired for the next tick.
//
void Control::MovingDesired()
{
	m_MovingDesired = true;

	printf("Control @ 0x%x: Setting move desired to true\n", reinterpret_cast<unsigned int>(this));                      
}

int main()
{
	// Temp.
	__int64 l_StartTicks = TimerGetTicks();

	char const* const l_SerialPortName = "COM3";

	// Open the serial connection to the micro.
	SerialConnection l_Serial;
	if (l_Serial.Initialize(l_SerialPortName) == false)
	{
		return 0;
	}

	// Initialize controls.
	for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
	{
		s_Controls[l_ControlIndex].Initialize(&l_Serial, s_ControlCommandStrings[l_ControlIndex]);
	}

	// Temp.
	__int64 l_EndTicks = TimerGetTicks();

	float l_ElapsedTimeMS = TimerGetElapsedMilliseconds(l_StartTicks, l_EndTicks); 

	printf("Elapsed time is %f ms.\n", l_ElapsedTimeMS);

	// Store a command here.
	unsigned int const l_CommandBufferSize = 8;
	char l_CommandBuffer[l_CommandBufferSize];
	unsigned int l_CommandLength = 0;

	bool l_Done = false;
	while (l_Done == false)
	{
		// Try to get keyboard commands.
		if (_kbhit() != 0)
		{
			// Get the character.
			char l_NextChar = static_cast<char>(_getch());

			// Accumulate characters until we get a terminating character or we run out of space.
			if ((l_NextChar != ' ') && (l_CommandLength < (l_CommandBufferSize - 1)))
			{
				l_CommandBuffer[l_CommandLength] = l_NextChar;
				l_CommandLength++;
			}
			else
			{
				// Terminate the command.
				l_CommandBuffer[l_CommandLength] = '\0';

				// Echo the command back.
				printf("Command input: \"%s\"\n", l_CommandBuffer);

				// Command handling.
				if (l_CommandBuffer[0] == 'h')
				{
					if (l_CommandBuffer[1] == 'u')
					{
						s_Controls[CONTROL_HEAD_UP].MovingDesired();
					}
					else if (l_CommandBuffer[1] == 'd')
					{
						s_Controls[CONTROL_HEAD_DOWN].MovingDesired();
					}
				}
				else if (l_CommandBuffer[0] == 'k')
				{
					if (l_CommandBuffer[1] == 'u')
					{
						s_Controls[CONTROL_KNEE_UP].MovingDesired();
					}
					else if (l_CommandBuffer[1] == 'd')
					{
						s_Controls[CONTROL_KNEE_DOWN].MovingDesired();
					}
				}
				else if (l_CommandBuffer[0] == 'e')
				{
					if (l_CommandBuffer[1] == 'u')
					{
						s_Controls[CONTROL_ELEVATION_UP].MovingDesired();
					}
					else if (l_CommandBuffer[1] == 'd')
					{
						s_Controls[CONTROL_ELEVATION_DOWN].MovingDesired();
					}
				}
				else if (l_CommandBuffer[0] == 'q')
				{
					l_Done = true;
				}

				// Prepare for a new command.
				l_CommandLength = 0;
			}
		}

		// Process controls.
		for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
		{
			s_Controls[l_ControlIndex].Process();
		}

		// Read serial port text, if there is any.
		unsigned long int const l_NumBytesToRead = 256;
		char l_StringBuffer[l_NumBytesToRead];
		unsigned long int l_NumBytesRead = 0;

		if (l_Serial.ReadString(l_NumBytesRead, l_StringBuffer, l_NumBytesToRead) == false)
		{
			l_Serial.Uninitialize();
			return 0;
		}

		// Did we read any?
		if (l_NumBytesRead != 0)
		{
			// Display it.
			printf("%s: %s\n", l_SerialPortName, l_StringBuffer);
		}
	}

	// Close the serial port.
	l_Serial.Uninitialize();
}