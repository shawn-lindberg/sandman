#include <conio.h>
#include <stdio.h>
#include <Windows.h>

#include "control.h"
#include "serial_connection.h"
#include "timer.h"

// Types
//

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

int main()
{
	char const* const l_SerialPortName = "COM4";

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

	// Store a command here.
	unsigned int const l_CommandBufferSize = 8;
	char l_CommandBuffer[l_CommandBufferSize];
	unsigned int l_CommandLength = 0;

	// Store serial string data here.
	unsigned int const l_SerialStringBufferCapacity = 512;
	char l_SerialStringBuffer[l_SerialStringBufferCapacity];
	unsigned int l_SerialStringBufferSize = 0;

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
						s_Controls[CONTROL_HEAD_UP].SetMovingDesired(true);
					}
					else if (l_CommandBuffer[1] == 'd')
					{
						s_Controls[CONTROL_HEAD_DOWN].SetMovingDesired(true);
					}
				}
				else if (l_CommandBuffer[0] == 'k')
				{
					if (l_CommandBuffer[1] == 'u')
					{
						s_Controls[CONTROL_KNEE_UP].SetMovingDesired(true);
					}
					else if (l_CommandBuffer[1] == 'd')
					{
						s_Controls[CONTROL_KNEE_DOWN].SetMovingDesired(true);
					}
				}
				else if (l_CommandBuffer[0] == 'e')
				{
					if (l_CommandBuffer[1] == 'u')
					{
						s_Controls[CONTROL_ELEVATION_UP].SetMovingDesired(true);
					}
					else if (l_CommandBuffer[1] == 'd')
					{
						s_Controls[CONTROL_ELEVATION_DOWN].SetMovingDesired(true);
					}
				}
				else if (l_CommandBuffer[0] == 's')
				{
					// Stop controls.
					for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
					{
						s_Controls[l_ControlIndex].SetMovingDesired(false);
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

		// Read serial port text, if there is any.  Read onto the end of the current string, nuking the current terminating
		// character.
		unsigned long int const l_NumBytesToRead = l_SerialStringBufferCapacity - l_SerialStringBufferSize;
		unsigned long int l_NumBytesRead = 0;

		if (l_Serial.ReadString(l_NumBytesRead, &l_SerialStringBuffer[l_SerialStringBufferSize], l_NumBytesToRead) == false)
		{
			l_Serial.Uninitialize();
			return 0;
		}

		// Did we read any?
		if (l_NumBytesRead != 0)
		{
			// Record the new size.
			l_SerialStringBufferSize += l_NumBytesRead;

			// If a new line is present, we can output.
			char* l_LineEnd = strchr(l_SerialStringBuffer, '\n');

			while (l_LineEnd != NULL)
			{
				// Terminate the string at line's end.
				*l_LineEnd = '\0';

				// Display it.
				printf("%s: %s\n", l_SerialPortName, l_SerialStringBuffer);

				// Gobble a trailing carraige return.
				if (l_LineEnd[1] == '\r') 
				{
					l_LineEnd++;
				}

				// Shift the buffer.
				// NOTE: Reduce size by the line length plus the terminating character that replaced the new line.  Then move
				// everything past that character plus a terminating character to the front.
				l_SerialStringBufferSize -= (l_LineEnd - l_SerialStringBuffer + 1);
				memmove(l_SerialStringBuffer, l_LineEnd + 1, l_SerialStringBufferSize + 1);

				// Look for another new line.
				l_LineEnd = strchr(l_SerialStringBuffer, '\n');
			}

			// Full?
			if ((l_SerialStringBufferSize + 1) == l_SerialStringBufferSize)
			{
				// Display it.
				printf("%s: %s\n", l_SerialPortName, l_SerialStringBuffer);
				
				// Clear it.
				l_SerialStringBufferSize = 0;
			}
		}
	}

	// Close the serial port.
	l_Serial.Uninitialize();
}