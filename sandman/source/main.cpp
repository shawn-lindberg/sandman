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

	// Track utterances.
	bool l_InUtterance = false;
	int l_LastVoiceSampleCount = 0;

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