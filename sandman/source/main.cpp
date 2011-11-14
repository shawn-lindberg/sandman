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

// Audio input parameters.
static unsigned int const s_NumChannels = 2;
static unsigned int const s_BitsPerSample = 16;
static unsigned int const s_SampleRate = 44100;
static unsigned int const s_RecordLengthSec = 10;

static unsigned int const s_AudioInputBufferCapacityBytes = s_NumChannels * (s_BitsPerSample / 8) * s_SampleRate * s_RecordLengthSec;
static char s_AudioInputBuffer[s_AudioInputBufferCapacityBytes];

// Set when the audio buffer fills.
static bool s_AudioInputBufferFull = false;

// Functions
//

// Callback to handle audio input events.
//
// p_AudioInputHandle:
// p_Message:
// p_CallbackData:
// p_MessageParam1:
// p_MessageParam2:
//
static void CALLBACK AudioInputCallback(HWAVEIN p_AudioInputHandle, unsigned int p_Message, unsigned int p_CallbackData,
	unsigned int p_MessageParam1, unsigned int p_MessageParam2)
{
	// Unreferenced parameters.
	p_CallbackData;
	p_MessageParam1;
	p_MessageParam2;

	if (p_Message == WIM_DATA)
	{
		printf("Audio input message for 0x%x: %d\n", p_AudioInputHandle, p_Message);
		s_AudioInputBufferFull = true;
	}
}

int main()
{
	// List the audio input options.
	unsigned int const l_NumAudioInputDevices = waveInGetNumDevs();

	printf("%d audio input devices:\n", l_NumAudioInputDevices);
	
	for (unsigned int l_DeviceIndex = 0; l_DeviceIndex < l_NumAudioInputDevices; l_DeviceIndex++)
	{
		WAVEINCAPS l_AudioInputDeviceInfo;
		if (waveInGetDevCaps(l_DeviceIndex, &l_AudioInputDeviceInfo, sizeof(l_AudioInputDeviceInfo)) != 0)
		{
			continue;
		}

		printf("\t%d - %s\n", l_DeviceIndex + 1, l_AudioInputDeviceInfo.szPname);
	}

	// Select an audio input device.
	unsigned int l_SelectedAudioInputDevice = l_NumAudioInputDevices;

	while (l_SelectedAudioInputDevice >= l_NumAudioInputDevices)
	{
		printf("\nEnter the number of the audio input device to use (1-%d): ", l_NumAudioInputDevices);

		// Get a line of input.
		unsigned int const l_InputBufferCapacity = 32;
		char l_InputBuffer[l_InputBufferCapacity];

		if (gets_s(l_InputBuffer, l_InputBufferCapacity) == NULL)
		{
			continue;
		}

		// See if the input was a number.
		unsigned int l_NumParsed = sscanf_s(l_InputBuffer, "%d", &l_SelectedAudioInputDevice);

		if (l_NumParsed != 1)
		{
			printf("Invalid selection.\n");
			continue;
		}

		// Make it zero based.
		l_SelectedAudioInputDevice--;

		if (l_SelectedAudioInputDevice >= l_NumAudioInputDevices)
		{
			printf("Invalid selection.\n");
			continue;
		}

		// Made a selection.
		WAVEINCAPS l_AudioInputDeviceInfo;
		if (waveInGetDevCaps(l_SelectedAudioInputDevice, &l_AudioInputDeviceInfo, sizeof(l_AudioInputDeviceInfo)) != 0)
		{
			printf("Error with selected device.\n");
			continue;
		}

		printf("Selected %s.\n\n", l_AudioInputDeviceInfo.szPname);
	}

	// Test - record 10 sec of audio.

	printf("Opening audio input device...\n");

	// Define the format.
	WAVEFORMATEX l_AudioFormat;
	{
		l_AudioFormat.wFormatTag = WAVE_FORMAT_PCM;
		l_AudioFormat.nChannels = s_NumChannels;
		l_AudioFormat.wBitsPerSample = s_BitsPerSample;
		l_AudioFormat.nSamplesPerSec = s_SampleRate;
		l_AudioFormat.nBlockAlign = l_AudioFormat.nChannels * (l_AudioFormat.wBitsPerSample / 8);
		l_AudioFormat.nAvgBytesPerSec = l_AudioFormat.nBlockAlign *	l_AudioFormat.nSamplesPerSec;
		l_AudioFormat.cbSize = 0;
	}

	// Open the device.
	HWAVEIN l_AudioInputHandle;
	MMRESULT l_AudioResult = waveInOpen(&l_AudioInputHandle, l_SelectedAudioInputDevice, &l_AudioFormat,
		reinterpret_cast<unsigned int>(AudioInputCallback), 0, CALLBACK_FUNCTION);

	if (l_AudioResult != MMSYSERR_NOERROR) {

		printf("Error opening audio input device: %d\n", l_AudioResult);
		return 0;
	}

	printf("\tsuccess\n");

	// Prepare a buffer.
	WAVEHDR l_AudioInputHeader;
	{
		memset(&l_AudioInputHeader, 0, sizeof(l_AudioInputHeader)); 
		l_AudioInputHeader.lpData = s_AudioInputBuffer;
		l_AudioInputHeader.dwBufferLength = s_AudioInputBufferCapacityBytes;
	}

	l_AudioResult = waveInPrepareHeader(l_AudioInputHandle, &l_AudioInputHeader, sizeof(l_AudioInputHeader));

	if (l_AudioResult != MMSYSERR_NOERROR) {

		printf("Error preparing audio input buffer: %d\n", l_AudioResult);
		return 0;
	}

	// Add the buffer.
	l_AudioResult = waveInAddBuffer(l_AudioInputHandle, &l_AudioInputHeader, sizeof(l_AudioInputHeader));

	if (l_AudioResult != MMSYSERR_NOERROR) {

		printf("Error preparing audio input buffer: %d\n", l_AudioResult);
		return 0;
	}

	printf("\nRecording audio...\n");

	// Start recording.
	l_AudioResult = waveInStart(l_AudioInputHandle);

	if (l_AudioResult != MMSYSERR_NOERROR) {

		printf("Error starting audio input: %d\n", l_AudioResult);
		return 0;
	}

	while (s_AudioInputBufferFull == false)
	{
		// Yay, busy loop!
	}

	printf("\tdone\n");

	// Unprepare the buffer.
	l_AudioResult = waveInUnprepareHeader(l_AudioInputHandle, &l_AudioInputHeader, sizeof(l_AudioInputHeader));

	if (l_AudioResult != MMSYSERR_NOERROR) {

		printf("Error unpreparing audio input buffer: %d\n", l_AudioResult);
		return 0;
	}
	
	// Reset and close the audio input device.  I'm not checking return values because if these fail, I don't think I can do
	// anything about it.
	waveInReset(l_AudioInputHandle);
	waveInClose(l_AudioInputHandle);

	// A pause.
	printf("\nPress any key to continue...");
	int dummy = _getch();
	dummy++;
	printf("\n\n");

	// Test - play back the audio.

	// Open the audio output device.
	HWAVEOUT l_AudioOutputHandle;
	l_AudioResult = waveOutOpen(&l_AudioOutputHandle, WAVE_MAPPER, &l_AudioFormat, 0, 0, WAVE_FORMAT_DIRECT);

	if (l_AudioResult != MMSYSERR_NOERROR) {

		printf("Error opening audio output device: %d\n", l_AudioResult);
		return 0;
	}

	// Prepare a buffer.
	WAVEHDR l_AudioOutputHeader;
	{
		memset(&l_AudioOutputHeader, 0, sizeof(l_AudioOutputHeader)); 
		l_AudioOutputHeader.lpData = s_AudioInputBuffer;
		l_AudioOutputHeader.dwBufferLength = s_AudioInputBufferCapacityBytes;
	}

	l_AudioResult = waveOutPrepareHeader(l_AudioOutputHandle, &l_AudioOutputHeader, sizeof(l_AudioOutputHeader));

	if (l_AudioResult != MMSYSERR_NOERROR) {

		printf("Error preparing audio output buffer: %d\n", l_AudioResult);
		return 0;
	}

	printf("Playing audio...\n");

	// Play the audio.
	l_AudioResult = waveOutWrite(l_AudioOutputHandle, &l_AudioOutputHeader, sizeof(l_AudioOutputHeader));

	if (l_AudioResult != MMSYSERR_NOERROR) {

		printf("Error preparing audio output buffer: %d\n", l_AudioResult);
		return 0;
	}

	// Wait for it to finish.
	do
	{
		l_AudioResult = waveOutUnprepareHeader(l_AudioOutputHandle, &l_AudioOutputHeader, sizeof(l_AudioOutputHeader));
	}
	while (l_AudioResult == WAVERR_STILLPLAYING);

	printf("\tdone\n");

	// Reset and close the audio output device.  I'm not checking return values because if these fail, I don't think I can do
	// anything about it.
	waveOutReset(l_AudioOutputHandle);
	waveOutClose(l_AudioOutputHandle);
	
	printf("\nOpening serial port...\n");

	char const* const l_SerialPortName = "COM4";

	// Open the serial connection to the micro.
	SerialConnection l_Serial;
	if (l_Serial.Initialize(l_SerialPortName) == false)
	{
		return 0;
	}

	printf("\tsuccess\n");

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