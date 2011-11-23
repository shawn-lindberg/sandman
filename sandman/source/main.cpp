#include <conio.h>
#include <stdio.h>
#include <Windows.h>

#include <sphinxbase/ad.h>
#include <sphinxbase/cont_ad.h>
#include <pocketsphinx.h>

#include "control.h"
#include "serial_connection.h"
#include "timer.h"

// Types
//

// Types of command tokens.
enum CommandTokenTypes
{
	COMMAND_TOKEN_SAND,
	COMMAND_TOKEN_MAN,
	COMMAND_TOKEN_HEAD,
	COMMAND_TOKEN_KNEE,
	COMMAND_TOKEN_ELEVATION,
	COMMAND_TOKEN_UP,
	COMMAND_TOKEN_DOWN,
	COMMAND_TOKEN_STOP,

	NUM_COMMAND_TOKEN_TYPES,
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

// Locals
//

// Names for each command token.
static char const* const s_CommandTokenNames[] = 
{
	"sand",			// COMMAND_TOKEN_SAND
	"man",			// COMMAND_TOKEN_MAN
	"head",			// COMMAND_TOKEN_HEAD
	"knee",			// COMMAND_TOKEN_KNEE
	"elevation",	// COMMAND_TOKEN_ELEVATION
	"up",			// COMMAND_TOKEN_UP
	"down",			// COMMAND_TOKEN_DOWN
	"stop",			// COMMAND_TOKEN_STOP
};

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

// How long to wait with no new voice to end an utterance in seconds.
static float const s_UtteranceTrailingSilenceThresholdSec = 1.0f;

// Functions
//

// Uninitialize program components.
//
// p_AudioRecorder:			(Input/Output) The audio input device.
// p_VoiceActivityDetector:	(Input/Output) The voice activity detector.
// p_SpeechDecoder:			(Input/Output) The speech decoder.
// p_Serial:				(Input/Output) The serial connection.
//
static void Uninitialize(ad_rec_t* p_AudioRecorder, cont_ad_t* p_VoiceActivityDetector, ps_decoder_t* p_SpeechDecoder,
	SerialConnection* p_Serial)
{
	// Uninitialize the serial connection.
	if (p_Serial != NULL)
	{
		p_Serial->Uninitialize();
	}
	
	// Uninitialize the speech decoder.
	if (p_SpeechDecoder != NULL)
	{
		ps_free(p_SpeechDecoder);
	}

	// Uninitialize the voice activity detector.
	if (p_VoiceActivityDetector != NULL)
	{
		cont_ad_close(p_VoiceActivityDetector);
	}

	// Uninitialize the audio input device.
	if (p_AudioRecorder != NULL)
	{
		ad_close(p_AudioRecorder);
	}
}

int main()
{
	printf("\nInitializing default audio input device...\n");

	// Open the default audio device for recording.
	ad_rec_t* l_AudioRecorder = ad_open();

	if (l_AudioRecorder == NULL)
	{
		printf("\tfailed\n");
		return 0;
	}

	printf("\tsucceeded\n");

	printf("\nInitializing voice activity detection...\n");

	// Initialize the voice activity detector.
	cont_ad_t* l_VoiceActivityDetector = cont_ad_init(l_AudioRecorder, ad_read);

	if (l_VoiceActivityDetector == NULL)
	{
		Uninitialize(l_AudioRecorder, NULL, NULL, NULL);

		printf("\tfailed\n");
		return 0;
	}

	printf("\tsucceeded\n");

	printf("\nCalibrating the voice activity detection...\n");

	// Calibrate the voice activity detector.
	if (ad_start_rec(l_AudioRecorder) < 0) 
	{
		Uninitialize(l_AudioRecorder, l_VoiceActivityDetector, NULL, NULL);

		printf("\tfailed\n");
		return 0;
	}

	if (cont_ad_calib(l_VoiceActivityDetector) < 0) 
	{
		Uninitialize(l_AudioRecorder, l_VoiceActivityDetector, NULL, NULL);

		printf("\tfailed\n");
		return 0;
	}

	printf("\tsucceeded\n");

	printf("\nInitializing the speech decoder...\n");

	// Initialize the speech decoder.
	cmd_ln_t* l_SpeechDecoderConfig = cmd_ln_init(NULL, ps_args(), TRUE, 
		"-hmm", "data/hmm/en_US/hub4wsj_sc_8k", "-lm", "data/lm/en_US/sandman.lm",
		"-dict", "data/dict/en_US/sandman.dic", NULL);

	if (l_SpeechDecoderConfig == NULL)
	{
		Uninitialize(l_AudioRecorder, l_VoiceActivityDetector, NULL, NULL);

		printf("\tfailed\n");
		return 0;
	}

	ps_decoder_t* l_SpeechDecoder = ps_init(l_SpeechDecoderConfig);
	
	if (l_SpeechDecoder == NULL)
	{
		Uninitialize(l_AudioRecorder, l_VoiceActivityDetector, NULL, NULL);

		printf("\tfailed\n");
		return 0;
	}

	printf("\tsucceeded\n");

	printf("\nOpening serial port...\n");

	char const* const l_SerialPortName = "COM4";

	// Open the serial connection to the micro.
	SerialConnection l_Serial;
	if (l_Serial.Initialize(l_SerialPortName) == false)
	{
		Uninitialize(l_AudioRecorder, l_VoiceActivityDetector, l_SpeechDecoder, NULL);

		printf("\tfailed\n");
		return 0;
	}

	printf("\tsucceeded\n\n");

	// Initialize controls.
	for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
	{
		s_Controls[l_ControlIndex].Initialize(&l_Serial, s_ControlCommandStrings[l_ControlIndex]);
	}

	// Track utterances.
	bool l_InUtterance = false;
	int l_LastVoiceSampleCount = 0;

	// Store a keyboard input here.
	unsigned int const l_KeyboardInputBufferCapacity = 8;
	char l_KeyboardInputBuffer[l_KeyboardInputBufferCapacity];
	unsigned int l_KeyboardInputBufferSize = 0;

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
			if ((l_NextChar != ' ') && (l_KeyboardInputBufferSize < (l_KeyboardInputBufferCapacity - 1)))
			{
				l_KeyboardInputBuffer[l_KeyboardInputBufferSize] = l_NextChar;
				l_KeyboardInputBufferSize++;
			}
			else
			{
				// Terminate the command.
				l_KeyboardInputBuffer[l_KeyboardInputBufferSize] = '\0';

				// Echo the command back.
				printf("Keyboard command input: \"%s\"\n", l_KeyboardInputBuffer);

				// Command handling.
				if (l_KeyboardInputBuffer[0] == 'h')
				{
					if (l_KeyboardInputBuffer[1] == 'u')
					{
						s_Controls[CONTROL_HEAD_UP].SetMovingDesired(true);
					}
					else if (l_KeyboardInputBuffer[1] == 'd')
					{
						s_Controls[CONTROL_HEAD_DOWN].SetMovingDesired(true);
					}
				}
				else if (l_KeyboardInputBuffer[0] == 'k')
				{
					if (l_KeyboardInputBuffer[1] == 'u')
					{
						s_Controls[CONTROL_KNEE_UP].SetMovingDesired(true);
					}
					else if (l_KeyboardInputBuffer[1] == 'd')
					{
						s_Controls[CONTROL_KNEE_DOWN].SetMovingDesired(true);
					}
				}
				else if (l_KeyboardInputBuffer[0] == 'e')
				{
					if (l_KeyboardInputBuffer[1] == 'u')
					{
						s_Controls[CONTROL_ELEVATION_UP].SetMovingDesired(true);
					}
					else if (l_KeyboardInputBuffer[1] == 'd')
					{
						s_Controls[CONTROL_ELEVATION_DOWN].SetMovingDesired(true);
					}
				}
				else if (l_KeyboardInputBuffer[0] == 's')
				{
					// Stop controls.
					for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
					{
						s_Controls[l_ControlIndex].SetMovingDesired(false);
					}
				}
				else if (l_KeyboardInputBuffer[0] == 'q')
				{
					l_Done = true;
				}

				// Prepare for a new command.
				l_KeyboardInputBufferSize = 0;
			}
		}

		// Speech recognition.
		{
			// Read some more audio looking for voice.
			unsigned int const l_VoiceDataBufferCapacity = 4096;
			short int l_VoiceDataBuffer[l_VoiceDataBufferCapacity];

			int l_NumSamplesRead = cont_ad_read(l_VoiceActivityDetector, l_VoiceDataBuffer, l_VoiceDataBufferCapacity);

			if (l_NumSamplesRead < 0)
			{
				printf("Error reading audio for voice activity detection.\n");
				l_Done = true;
			}

			if (l_NumSamplesRead > 0)
			{
				// An utterance has begun.
				if (l_InUtterance == false)
				{
					printf("Beginning of utterance detected.\n");

					// NULL here means automatically choose an utterance ID.
					if (ps_start_utt(l_SpeechDecoder, NULL) < 0)
					{
						printf("Error starting utterance.\n");
						l_Done = true;
					}

					l_InUtterance = true;
				}

				// Process the voice ddata.
				if (ps_process_raw(l_SpeechDecoder, l_VoiceDataBuffer, l_NumSamplesRead, 0, 0) < 0)
				{
					printf("Error decoding speech data.\n");
					l_Done = true;
				}

				// New voice samples, update count.
				l_LastVoiceSampleCount = l_VoiceActivityDetector->read_ts;
			}

			if ((l_NumSamplesRead == 0) && (l_InUtterance == true))
			{
				// Get the time since last voice sample.
				float l_TimeSinceVoiceSec = 
					(l_VoiceActivityDetector->read_ts - l_LastVoiceSampleCount) / static_cast<float>(l_AudioRecorder->sps);

				if (l_TimeSinceVoiceSec >= s_UtteranceTrailingSilenceThresholdSec)
				{
					// An utterance has ended.
					printf("End of utterance detected.\n");

					if (ps_end_utt(l_SpeechDecoder) < 0)
					{
						printf("Error ending utterance.\n");
						l_Done = true;
					}

					// Reset accumulated data for the voice activity detector.
					cont_ad_reset(l_VoiceActivityDetector);

					l_InUtterance = false;

					// Get the speech for the utterance.
					int l_Score = 0;
					char const* l_UtteranceID = NULL;
					char const* l_RecognizedSpeech = ps_get_hyp(l_SpeechDecoder, &l_Score, &l_UtteranceID);

					printf("Recognized: \"%s\", score %d, utterance ID \"%s\"\n", l_RecognizedSpeech, l_Score, l_UtteranceID);

					// Parse a command.

					// Store command tokens here.
					unsigned int const l_CommandTokenBufferCapacity = 32;
					CommandTokenTypes l_CommandTokenBuffer[l_CommandTokenBufferCapacity];
					unsigned int l_CommandTokenBufferSize = 0;

					// Store token strings here.
					unsigned int const l_TokenStringBufferCapacity = 32;
					char l_TokenStringBuffer[l_TokenStringBufferCapacity];

					// Get the first token string start.
					char const* l_NextTokenStringStart = l_RecognizedSpeech;

					while (l_NextTokenStringStart != NULL)
					{
						// Get the next token string end.
						char const* l_NextTokenStringEnd = strchr(l_NextTokenStringStart, ' ');

						// Get the token string length.
						unsigned int l_TokenStringLength = 0;
						if (l_NextTokenStringEnd != NULL)
						{
							l_TokenStringLength = l_NextTokenStringEnd - l_NextTokenStringStart;
						}
						else 
						{
							l_TokenStringLength = strlen(l_NextTokenStringStart);
						}

						// Copy the token string.
						unsigned int l_AmountToCopy = min(l_TokenStringBufferCapacity - 1, l_TokenStringLength);
						strncpy_s(l_TokenStringBuffer, l_NextTokenStringStart, l_AmountToCopy);
						l_TokenStringBuffer[l_AmountToCopy] = '\0';

						// Make sure the token string is lowercase.
						_strlwr_s(l_TokenStringBuffer);

						// Match the token string to a token if possible.
						for (unsigned int l_TokenType = 0; l_TokenType < NUM_COMMAND_TOKEN_TYPES; l_TokenType++)
						{
							// Compare the token string to its name.
							if (strcmp(l_TokenStringBuffer, s_CommandTokenNames[l_TokenType]) != 0)
							{
								continue;
							}

							// Found a match.

							// Add the token to the buffer.
							if (l_CommandTokenBufferSize < l_CommandTokenBufferCapacity)
							{
								l_CommandTokenBuffer[l_CommandTokenBufferSize] = static_cast<CommandTokenTypes>(l_TokenType);
								l_CommandTokenBufferSize++;
							} 
							else
							{
								printf("Voice command too long, tail will be ignored.\n");
							}

							break;
						}

						// Get the next token string start (skip delimiter).
						if (l_NextTokenStringEnd != NULL)
						{
							l_NextTokenStringStart = l_NextTokenStringEnd + 1;
						}
						else
						{
							l_NextTokenStringStart = NULL;
						}
					}

					// Parse command tokens.
					for (unsigned int l_TokenIndex = 0; l_TokenIndex < l_CommandTokenBufferSize; l_TokenIndex++)
					{
						// Look for the prefix.
						if (l_CommandTokenBuffer[l_TokenIndex] != COMMAND_TOKEN_SAND)
						{
							continue;
						}

						// Next token.
						l_TokenIndex++;
						if (l_TokenIndex >= l_CommandTokenBufferSize)
						{
							break;
						}

						// Look for the rest of the prefix.
						if (l_CommandTokenBuffer[l_TokenIndex] != COMMAND_TOKEN_MAN)
						{
							continue;
						}

						// Next token.
						l_TokenIndex++;
						if (l_TokenIndex >= l_CommandTokenBufferSize)
						{
							break;
						}

						// Parse commands.
						if (l_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_HEAD)
						{
							// Next token.
							l_TokenIndex++;
							if (l_TokenIndex >= l_CommandTokenBufferSize)
							{
								break;
							}

							if (l_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_UP)
							{
								s_Controls[CONTROL_HEAD_UP].SetMovingDesired(true);
							}
							else if (l_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_DOWN)
							{
								s_Controls[CONTROL_HEAD_DOWN].SetMovingDesired(true);
							}
						}
						else if (l_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_KNEE)
						{
							// Next token.
							l_TokenIndex++;
							if (l_TokenIndex >= l_CommandTokenBufferSize)
							{
								break;
							}

							if (l_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_UP)
							{
								s_Controls[CONTROL_KNEE_UP].SetMovingDesired(true);
							}
							else if (l_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_DOWN)
							{
								s_Controls[CONTROL_KNEE_DOWN].SetMovingDesired(true);
							}
						}
						else if (l_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_ELEVATION)
						{
							// Next token.
							l_TokenIndex++;
							if (l_TokenIndex >= l_CommandTokenBufferSize)
							{
								break;
							}

							if (l_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_UP)
							{
								s_Controls[CONTROL_ELEVATION_UP].SetMovingDesired(true);
							}
							else if (l_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_DOWN)
							{
								s_Controls[CONTROL_ELEVATION_DOWN].SetMovingDesired(true);
							}
						}
						else if (l_CommandTokenBuffer[l_TokenIndex] == COMMAND_TOKEN_STOP)
						{
							// Stop controls.
							for (unsigned int l_ControlIndex = 0; l_ControlIndex < NUM_CONTROL_TYPES; l_ControlIndex++)
							{
								s_Controls[l_ControlIndex].SetMovingDesired(false);
							}
						}
					}

					// All tokens parsed.
					l_CommandTokenBufferSize = 0;
				}
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
			l_Done = true;
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

	// Cleanup.
	Uninitialize(l_AudioRecorder, l_VoiceActivityDetector, l_SpeechDecoder, &l_Serial);
}