#include "speech_recognizer.h"

#include "logger.h"
#include "timer.h"

// Locals
//

// How long to wait with no new voice to end an utterance in seconds.
static float s_UtteranceTrailingSilenceThresholdSec = 1.0f;

// Maximum length of an utterance in seconds.
static float const s_UtteranceMaxLengthSec = 30.0f;

// Functions
//

// SpeechRecognizer members

// Initialize the recognizer.
//
// p_CaptureDeviceName:								The name of the audio capture device.
// p_SampleRate:										The audio capture sample rate.
// p_HMMFileName:										File name of the HMM the recognizer will use.
// p_LanguageModelFileName:						File name of the language model the recognizer will use.
// p_DictionaryFileName:							File name of the dictionary the recognizer will use.
// p_LogFileName:										File name of the log for recognizer output.
// p_UtteranceTrailingSilenceThresholdSec:	How long to wait with no new voice to end an utterance 
// 														in seconds.
//
// returns:		True for success, false otherwise.
//
bool SpeechRecognizer::Initialize(char const* p_CaptureDeviceName, unsigned int p_SampleRate, 
	char const* p_HMMFileName,	char const* p_LanguageModelFileName, char const* p_DictionaryFileName, 
	char const* p_LogFileName, float p_UtteranceTrailingSilenceThresholdSec)
{
	m_AudioRecorder = nullptr;
	m_VoiceActivityDetector = nullptr;
	m_SpeechDecoder = nullptr;

	m_InUtterance = false;
	m_LastVoiceSampleCount = 0;

	// Delete the current recognizer log.
	auto* l_LogFile = fopen(p_LogFileName, "w");

	if (l_LogFile != nullptr)
	{
		fclose(l_LogFile);
	}

	LoggerAddMessage("Initializing audio input device \"%s\" at %i Hz...", p_CaptureDeviceName, 
		p_SampleRate);

	// Open the default audio device for recording.
	m_AudioRecorder = ad_open_dev(p_CaptureDeviceName, p_SampleRate);

	if (m_AudioRecorder == nullptr)
	{
		LoggerAddMessage("\tfailed");
		return false;
	}

	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");

	LoggerAddMessage("Initializing voice activity detection...");

	// Initialize the voice activity detector.
	m_VoiceActivityDetector = cont_ad_init(m_AudioRecorder, ad_read);

	if (m_VoiceActivityDetector == nullptr)
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
	auto* l_SpeechDecoderConfig = cmd_ln_init(nullptr, ps_args(), TRUE, "-hmm", p_HMMFileName, 
		"-lm", p_LanguageModelFileName, "-dict", p_DictionaryFileName, "-logfn", p_LogFileName, 
		nullptr);

	if (l_SpeechDecoderConfig == nullptr)
	{
		Uninitialize();

		LoggerAddMessage("\tfailed");
		return false;
	}

	m_SpeechDecoder = ps_init(l_SpeechDecoderConfig);
	
	if (m_SpeechDecoder == nullptr)
	{
		Uninitialize();

		LoggerAddMessage("\tfailed");
		return false;
	}

	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");

	// Post speech delay.
	if (p_UtteranceTrailingSilenceThresholdSec > 0.0f)
	{
		s_UtteranceTrailingSilenceThresholdSec = p_UtteranceTrailingSilenceThresholdSec;

		LoggerAddMessage("Speech recognizer post speech delay set to - %f sec.", 
			p_UtteranceTrailingSilenceThresholdSec);
	}
		
	return true;
}

// Uninitialize the recognizer.
//
void SpeechRecognizer::Uninitialize()
{
	// Uninitialize the speech decoder.
	if (m_SpeechDecoder != nullptr)
	{
		ps_free(m_SpeechDecoder);
		m_SpeechDecoder = nullptr;
	}

	// Uninitialize the voice activity detector.
	if (m_VoiceActivityDetector != nullptr)
	{
		cont_ad_close(m_VoiceActivityDetector);
		m_VoiceActivityDetector = nullptr;
	}

	// Uninitialize the audio input device.
	if (m_AudioRecorder != nullptr)
	{
		ad_close(m_AudioRecorder);
		m_AudioRecorder = nullptr;
	}
}

// Process audio input in an attempt to recognize speech.
//
// p_RecognizedSpeech:	(Output) The speech recognized if any, or null if not.
//
// returns:				True for success, false if an error occurred.
//
bool SpeechRecognizer::Process(char const*& p_RecognizedSpeech)
{
	// Read some more audio looking for voice.
	static constexpr unsigned int const l_VoiceDataBufferCapacity = 4096;
	short int l_VoiceDataBuffer[l_VoiceDataBufferCapacity];

	auto l_NumSamplesRead = cont_ad_read(m_VoiceActivityDetector, l_VoiceDataBuffer, 
		l_VoiceDataBufferCapacity);

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

			// Null here means automatically choose an utterance ID.
			if (ps_start_utt(m_SpeechDecoder, nullptr) < 0)
			{
				LoggerAddMessage("Error starting utterance.");
				return false;
			}

			// Record the utterance start.
			m_InUtterance = true;
			TimerGetCurrent(m_UtteranceStartTime);
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
		auto const l_TimeSinceVoiceSec = (m_VoiceActivityDetector->read_ts - m_LastVoiceSampleCount) / 
			static_cast<float>(m_AudioRecorder->sps);

		// Get elapsed time since utterance start.
		Time l_CurrentTime;
		TimerGetCurrent(l_CurrentTime);

		auto const l_ElapsedTimeSec = TimerGetElapsedMilliseconds(m_UtteranceStartTime, l_CurrentTime) / 
			1000.0f;

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
			char const* l_UtteranceID = nullptr;
			p_RecognizedSpeech = ps_get_hyp(m_SpeechDecoder, &l_Score, &l_UtteranceID);

			LoggerAddMessage("Recognized: \"%s\", score %d, utterance ID \"%s\"", p_RecognizedSpeech, 
				l_Score, l_UtteranceID);
		}
	}

	return true;
}

