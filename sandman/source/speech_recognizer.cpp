#include "speech_recognizer.h"

#include "logger.h"
#include "timer.h"

// Locals
//

// How long to wait with no new voice to end an utterance in seconds.
static float const s_UtteranceTrailingSilenceThresholdSec = 1.0f;

// Maximum length of an utterance in seconds.
static float const s_UtteranceMaxLengthSec = 30.0f;

// Functions
//

// SpeechRecognizer members

// Initialize the recognizer.
//
// p_CaptureDeviceName:		The name of the audio capture device.
// p_SampleRate:			The audio capture sample rate.
// p_HMMFileName:			File name of the HMM the recognizer will use.
// p_LanguageModelFileName:	File name of the language model the recognizer will use.
// p_DictionaryFileName:	File name of the dictionary the recognizer will use.
// p_LogFileName:			File name of the log for recognizer output.
//
// returns:		True for success, false otherwise.
//
bool SpeechRecognizer::Initialize(char const* p_CaptureDeviceName, unsigned int p_SampleRate, char const* p_HMMFileName,
	char const* p_LanguageModelFileName, char const* p_DictionaryFileName, char const* p_LogFileName)
{
	m_AudioRecorder = NULL;
	m_VoiceActivityDetector = NULL;
	m_SpeechDecoder = NULL;

	m_InUtterance = false;
	m_LastVoiceSampleCount = 0;

	// Delete the current recognizer log.
	FILE* l_LogFile = fopen(p_LogFileName, "w");

	if (l_LogFile != NULL)
	{
		fclose(l_LogFile);
	}

	LoggerAddMessage("Initializing default audio input device...");

	// Open the default audio device for recording.
	m_AudioRecorder = ad_open_dev(p_CaptureDeviceName, p_SampleRate);

	if (m_AudioRecorder == NULL)
	{
		LoggerAddMessage("\tfailed");
		return false;
	}

	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");

	LoggerAddMessage("Initializing voice activity detection...");

	// Initialize the voice activity detector.
	m_VoiceActivityDetector = cont_ad_init(m_AudioRecorder, ad_read);

	if (m_VoiceActivityDetector == NULL)
	{
		Uninitialize();

		LoggerAddMessage("\tfailed");
		return false;
	}

	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");

	LoggerAddMessage("Calibrating the voice activity detection...");

	// Calibrate the voice activity detector.
	if (ad_start_rec(m_AudioRecorder) < 0) 
	{
		Uninitialize();

		LoggerAddMessage("\tfailed");
		return 0;
	}

	if (cont_ad_calib(m_VoiceActivityDetector) < 0) 
	{
		Uninitialize();

		LoggerAddMessage("\tfailed");
		return 0;
	}

	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");

	LoggerAddMessage("Initializing the speech decoder...");

	// Initialize the speech decoder.
	cmd_ln_t* l_SpeechDecoderConfig = cmd_ln_init(NULL, ps_args(), TRUE, "-hmm", p_HMMFileName, "-lm", p_LanguageModelFileName,
		"-dict", p_DictionaryFileName, "-logfn", p_LogFileName, NULL);

	if (l_SpeechDecoderConfig == NULL)
	{
		Uninitialize();

		LoggerAddMessage("\tfailed");
		return false;
	}

	m_SpeechDecoder = ps_init(l_SpeechDecoderConfig);
	
	if (m_SpeechDecoder == NULL)
	{
		Uninitialize();

		LoggerAddMessage("\tfailed");
		return false;
	}

	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");

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

// Process audio input in an attempt to recognize speech.
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
		LoggerAddMessage("Error reading audio for voice activity detection.");
		return false;
	}

	if (l_NumSamplesRead > 0)
	{
		// An utterance has begun.
		if (m_InUtterance == false)
		{
			LoggerAddMessage("Beginning of utterance detected.");

			// NULL here means automatically choose an utterance ID.
			if (ps_start_utt(m_SpeechDecoder, NULL) < 0)
			{
				LoggerAddMessage("Error starting utterance.");
				return false;
			}

			// Record the utterance start.
			m_InUtterance = true;
			m_UtteranceStartTimeTicks = TimerGetTicks();
		}

		// Process the voice ddata.
		if (ps_process_raw(m_SpeechDecoder, l_VoiceDataBuffer, l_NumSamplesRead, 0, 0) < 0)
		{
			LoggerAddMessage("Error decoding speech data.");
			return false;
		}

		// New voice samples, update count.
		m_LastVoiceSampleCount = m_VoiceActivityDetector->read_ts;
	}

	if (m_InUtterance == true)
	{
		// Get the time since last voice sample.
		float l_TimeSinceVoiceSec = 
			(m_VoiceActivityDetector->read_ts - m_LastVoiceSampleCount) / static_cast<float>(m_AudioRecorder->sps);

		// Get elapsed time since utterance start.
		uint64_t l_CurrentTimeTicks = TimerGetTicks();

		float l_ElapsedTimeSec = TimerGetElapsedMilliseconds(m_UtteranceStartTimeTicks, l_CurrentTimeTicks) / 1000.0f;

		// The utterance either ends if it experiences a long enough silence or gets too long.
		if (((l_NumSamplesRead == 0) && (l_TimeSinceVoiceSec >= s_UtteranceTrailingSilenceThresholdSec)) ||
			(l_ElapsedTimeSec > s_UtteranceMaxLengthSec))
		{
			// An utterance has ended.
			LoggerAddMessage("End of utterance detected after %f seconds.", l_ElapsedTimeSec);

			if (ps_end_utt(m_SpeechDecoder) < 0)
			{
				LoggerAddMessage("Error ending utterance.");
				return false;
			}

			// Reset accumulated data for the voice activity detector.
			cont_ad_reset(m_VoiceActivityDetector);

			m_InUtterance = false;

			// Get the speech for the utterance.
			int l_Score = 0;
			char const* l_UtteranceID = NULL;
			p_RecognizedSpeech = ps_get_hyp(m_SpeechDecoder, &l_Score, &l_UtteranceID);

			LoggerAddMessage("Recognized: \"%s\", score %d, utterance ID \"%s\"", p_RecognizedSpeech, l_Score, l_UtteranceID);
		}
	}

	return true;
}

