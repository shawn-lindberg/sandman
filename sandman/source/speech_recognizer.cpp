#include "speech_recognizer.h"

// Locals
//

// How long to wait with no new voice to end an utterance in seconds.
static float const s_UtteranceTrailingSilenceThresholdSec = 1.0f;

// Functions
//

// SpeechRecognizer members

// Initialize the recognizer.
//
// p_HMMFileName:			File name of the HMM the recognizer will use.
// p_LanguageModelFileName:	File name of the language model the recognizer will use.
// p_DictionaryFileName:	File name of the dictionary the recognizer will use.
// p_LogFileName:			File name of the log for recognizer output.
//
// returns:		True for success, false otherwise.
//
bool SpeechRecognizer::Initialize(char const* p_HMMFileName, char const* p_LanguageModelFileName, char const* p_DictionaryFileName,
	char const* p_LogFileName)
{
	m_AudioRecorder = NULL;
	m_VoiceActivityDetector = NULL;
	m_SpeechDecoder = NULL;

	m_InUtterance = false;
	m_LastVoiceSampleCount = 0;

	printf("\nInitializing default audio input device...\n");

	// Open the default audio device for recording.
	m_AudioRecorder = ad_open();

	if (m_AudioRecorder == NULL)
	{
		printf("\tfailed\n");
		return false;
	}

	printf("\tsucceeded\n");

	printf("\nInitializing voice activity detection...\n");

	// Initialize the voice activity detector.
	m_VoiceActivityDetector = cont_ad_init(m_AudioRecorder, ad_read);

	if (m_VoiceActivityDetector == NULL)
	{
		Uninitialize();

		printf("\tfailed\n");
		return false;
	}

	printf("\tsucceeded\n");

	printf("\nCalibrating the voice activity detection...\n");

	// Calibrate the voice activity detector.
	if (ad_start_rec(m_AudioRecorder) < 0) 
	{
		Uninitialize();

		printf("\tfailed\n");
		return 0;
	}

	if (cont_ad_calib(m_VoiceActivityDetector) < 0) 
	{
		Uninitialize();

		printf("\tfailed\n");
		return 0;
	}

	printf("\tsucceeded\n");

	printf("\nInitializing the speech decoder...\n");

	// Initialize the speech decoder.
	cmd_ln_t* l_SpeechDecoderConfig = cmd_ln_init(NULL, ps_args(), TRUE, "-hmm", p_HMMFileName, "-lm", p_LanguageModelFileName,
		"-dict", p_DictionaryFileName, "-logfn", p_LogFileName, NULL);

	if (l_SpeechDecoderConfig == NULL)
	{
		Uninitialize();

		printf("\tfailed\n");
		return false;
	}

	m_SpeechDecoder = ps_init(l_SpeechDecoderConfig);
	
	if (m_SpeechDecoder == NULL)
	{
		Uninitialize();

		printf("\tfailed\n");
		return false;
	}

	printf("\tsucceeded\n");
	return true;
}

// Uninitialize the recognizer.
//
void SpeechRecognizer::Uninitialize()
{
	// Uninitialize the speech decoder.
	if (m_SpeechDecoder != NULL)
	{
		ps_free(m_SpeechDecoder);
	}

	// Uninitialize the voice activity detector.
	if (m_VoiceActivityDetector != NULL)
	{
		cont_ad_close(m_VoiceActivityDetector);
	}

	// Uninitialize the audio input device.
	if (m_AudioRecorder != NULL)
	{
		ad_close(m_AudioRecorder);
	}
}

// Process audio input in an attempt to recognize speech..
//
// p_RecognizedSpeech:	(Output) The speech recognized if any, or NULL if not.
//
// returns:				True for success, false if an error occurred.
//
bool SpeechRecognizer::Process(char const*& p_RecognizedSpeech)
{
	// Read some more audio looking for voice.
	unsigned int const l_VoiceDataBufferCapacity = 4096;
	short int l_VoiceDataBuffer[l_VoiceDataBufferCapacity];

	int l_NumSamplesRead = cont_ad_read(m_VoiceActivityDetector, l_VoiceDataBuffer, l_VoiceDataBufferCapacity);

	if (l_NumSamplesRead < 0)
	{
		printf("Error reading audio for voice activity detection.\n");
		return false;
	}

	if (l_NumSamplesRead > 0)
	{
		// An utterance has begun.
		if (m_InUtterance == false)
		{
			printf("Beginning of utterance detected.\n");

			// NULL here means automatically choose an utterance ID.
			if (ps_start_utt(m_SpeechDecoder, NULL) < 0)
			{
				printf("Error starting utterance.\n");
				return false;
			}

			m_InUtterance = true;
		}

		// Process the voice ddata.
		if (ps_process_raw(m_SpeechDecoder, l_VoiceDataBuffer, l_NumSamplesRead, 0, 0) < 0)
		{
			printf("Error decoding speech data.\n");
			return false;
		}

		// New voice samples, update count.
		m_LastVoiceSampleCount = m_VoiceActivityDetector->read_ts;
	}

	if ((l_NumSamplesRead == 0) && (m_InUtterance == true))
	{
		// Get the time since last voice sample.
		float l_TimeSinceVoiceSec = 
			(m_VoiceActivityDetector->read_ts - m_LastVoiceSampleCount) / static_cast<float>(m_AudioRecorder->sps);

		if (l_TimeSinceVoiceSec >= s_UtteranceTrailingSilenceThresholdSec)
		{
			// An utterance has ended.
			printf("End of utterance detected.\n");

			if (ps_end_utt(m_SpeechDecoder) < 0)
			{
				printf("Error ending utterance.\n");
				return false;
			}

			// Reset accumulated data for the voice activity detector.
			cont_ad_reset(m_VoiceActivityDetector);

			m_InUtterance = false;

			// Get the speech for the utterance.
			int l_Score = 0;
			char const* l_UtteranceID = NULL;
			p_RecognizedSpeech = ps_get_hyp(m_SpeechDecoder, &l_Score, &l_UtteranceID);

			printf("Recognized: \"%s\", score %d, utterance ID \"%s\"\n", p_RecognizedSpeech, l_Score, l_UtteranceID);
		}
	}

	return true;
}

