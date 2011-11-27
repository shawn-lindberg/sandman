#pragma once

#include <sphinxbase/ad.h>
#include <sphinxbase/cont_ad.h>
#include <pocketsphinx.h>

// Types
//

// Hides the details of recognizing speech.
//
class SpeechRecognizer
{
	public:

		// Initialize the recognizer.
		//
		// p_HMMFileName:			File name of the HMM the recognizer will use.
		// p_LanguageModelFileName:	File name of the language model the recognizer will use.
		// p_DictionaryFileName:	File name of the dictionary the recognizer will use.
		//
		// returns:		True for success, false otherwise.
		//
		bool Initialize(char const* p_HMMFileName, char const* p_LanguageModelFileName, char const* p_DictionaryFileName);

		// Uninitialize the recognizer.
		//
		void Uninitialize();

		// Process audio input in an attempt to recognize speech..
		//
		// p_RecognizedSpeech:	(Output) The speech recognized if any, or NULL if not.
		//
		// returns:				True for success, false if an error occurred.
		//
		bool Process(char const*& p_RecognizedSpeech);

	private:

		// Allows recording from an audio input device.
		ad_rec_t* m_AudioRecorder;

		// Detects voice activity vs. silence in the audio input.
		cont_ad_t* m_VoiceActivityDetector;

		// Decodes the audio input.
		ps_decoder_t* m_SpeechDecoder;

		// Track where an utterance is in progress.
		bool m_InUtterance;

		// A counter tracking when we last saw voice activity.
		int m_LastVoiceSampleCount;
};
