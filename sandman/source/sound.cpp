#include "sound.h"

#include <algorithm>
#include <limits.h>

#include <SDL.h>
#include <SDL_mixer.h>

#include "logger.h"

#define DATADIR	AM_DATADIR

// Constants
//

// The most sounds that can be queued at once.
static unsigned int const s_MaxQueuedSounds = 8;

// How much the volume change by each time.
static int const s_VolumeChange = 0.2f * MIX_MAX_VOLUME;

// Locals
//

// Whether the system has been initialized.
static bool s_SoundInitialized = false;

// A queue of sounds to play.
static Mix_Chunk* s_PlayQueue[s_MaxQueuedSounds];
static unsigned int s_NumQueuedSounds = 0;

// Right now there should be only one channel, but this should be a bit more future proof.
static int s_PlayingChannel = -1;

// Whether we're muted.
static bool s_Muted = false;

// The desired volume (when unmuted).
static int s_DesiredVolume = MIX_MAX_VOLUME;

// The number of sounds to play before muting.
static unsigned int s_NumSoundsToPlayBeforeMute = UINT_MAX;

// Functions
//

// Remove a sound from the front of the play queue.
//
static void SoundRemoveFromQueue()
{
	if (s_NumQueuedSounds == 0)
	{
		return;
	}
	
	// Free the sound.
	Mix_FreeChunk(s_PlayQueue[0]);
	s_NumQueuedSounds--;
		
	// Shift the rest of queue forward.
	for (unsigned int l_SoundIndex = 0; l_SoundIndex < s_NumQueuedSounds; l_SoundIndex++)
	{
		s_PlayQueue[l_SoundIndex] = s_PlayQueue[l_SoundIndex + 1];
	}
}

// Initialize sound.
//
// returns:		True if successful, false otherwise.
//
bool SoundInitialize()
{
	if (s_SoundInitialized == true)
	{
		return false;
	}
	
	LoggerAddMessage("Initializing SDL audio...");

	// Initialize SDL audio.
	if (SDL_Init(SDL_INIT_AUDIO) < 0)
	{
		LoggerAddMessage("\tfailed");
		return false;
	}
	
	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");

	//LoggerAddMessage("Initializing MP3 support...");
	//
	// Initialize MP3 support.
	//int l_Result = Mix_Init(MIX_INIT_MP3);
	//
	//if ((l_Result & MIX_INIT_MP3) == 0)
	//{
	//	SDL_Quit();
	//	LoggerAddMessage("\tfailed with error \"%s\"", Mix_GetError());
	//	return false;
	//}
	//
	//LoggerAddMessage("\tsucceeded");
	//LoggerAddMessage("");
	
	// Open audio output.
	unsigned int l_SampleRate = 16000;
	
	LoggerAddMessage("Initializing audio output at %i Hz...", l_SampleRate);

	if (Mix_OpenAudio(l_SampleRate, AUDIO_S16LSB, 1, 16384) < 0)
	{
		//Mix_Quit();
		SDL_Quit();
		LoggerAddMessage("\tfailed with error \"%s\"", Mix_GetError());
		return false;
	}

	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");

	// Initialize the play queue.
	for (unsigned int l_SoundIndex = 0; l_SoundIndex < s_MaxQueuedSounds; l_SoundIndex++)
	{
		s_PlayQueue[l_SoundIndex] = nullptr;
	}
	
	s_NumQueuedSounds = 0;
	s_PlayingChannel = -1;
	s_Muted = false;
	s_DesiredVolume = MIX_MAX_VOLUME;
	s_NumSoundsToPlayBeforeMute = UINT_MAX;

	s_SoundInitialized = true;
	
	return true;
}

// Uninitialize sound.
//
void SoundUninitialize()
{
	if (s_SoundInitialized == false)
	{
		return;
	}
	
	// Clean up leftover sounds.
	for (unsigned int l_SoundIndex = 0; l_SoundIndex < s_NumQueuedSounds; l_SoundIndex++)
	{
		if (s_PlayQueue[l_SoundIndex] != nullptr)
		{
			Mix_FreeChunk(s_PlayQueue[l_SoundIndex]);
		}
		
		s_PlayQueue[l_SoundIndex] = nullptr;
	}
		
	s_NumQueuedSounds = 0;
	
	// Close audio output.
	Mix_CloseAudio();
	
	// Uninitialize MP3 support.
	//Mix_Quit();
	
	// Uninitialize SDL audio.
	SDL_Quit();
	
	s_SoundInitialized = false;
}

// Decrease the volume.  Note: Cannot reduce volume to 0.
//
void SoundDecreaseVolume()
{
	if (s_SoundInitialized == false)
	{
		return;
	}

	if (s_Muted == true)
	{
		return;
	}

	// Calculate the new volume.
	int l_NewVolume = std::max(s_DesiredVolume - s_VolumeChange, s_VolumeChange);
	
	if (l_NewVolume == s_DesiredVolume)
	{
		// Queue the sound.
		SoundAddToQueue(DATADIR "audio/vol_min.wav");
		return;
	}
	
	// Set the volume.
	s_DesiredVolume = l_NewVolume;
	Mix_Volume(-1, s_DesiredVolume);
	
	// Queue the sound.
	if (s_DesiredVolume > s_VolumeChange)
	{
		SoundAddToQueue(DATADIR "audio/vol_down.wav");
	}
	else
	{	
		SoundAddToQueue(DATADIR "audio/vol_min.wav");
	}
}

// Increase the volume.
//
void SoundIncreaseVolume()
{
	if (s_SoundInitialized == false)
	{
		return;
	}

	if (s_Muted == true)
	{
		return;
	}

	// Calculate the new volume.
	int l_NewVolume = std::min(s_DesiredVolume + s_VolumeChange, MIX_MAX_VOLUME);
	
	if (l_NewVolume == s_DesiredVolume)
	{
		// Queue the sound.
		SoundAddToQueue(DATADIR "audio/vol_max.wav");
		return;
	}
	
	// Set the volume.
	s_DesiredVolume = l_NewVolume;
	Mix_Volume(-1, s_DesiredVolume);
	
	// Queue the sound.
	if (s_DesiredVolume < MIX_MAX_VOLUME)
	{
		SoundAddToQueue(DATADIR "audio/vol_up.wav");
	}
	else
	{	
		SoundAddToQueue(DATADIR "audio/vol_max.wav");
	}
}

// Mute/unmute sound.
//
// p_Mute:	True for mute, false for unmute.
//
void SoundMute(bool p_Mute)
{
	if (s_SoundInitialized == false)
	{
		return;
	}
	
	if (s_Muted == p_Mute)
	{
		return;
	}
	
	// Update the flag.
	s_Muted = p_Mute;
	
	if (p_Mute == true)
	{
		LoggerAddMessage("Muting sound.");

		// Queue the sound.
		SoundAddToQueue(DATADIR "audio/vol_mute.wav");

		// Wait to mute.
		s_NumSoundsToPlayBeforeMute = s_NumQueuedSounds;
		return;
	}
	
	LoggerAddMessage("Unmuting sound.");

	// Queue the sound.
	SoundAddToQueue(DATADIR "audio/vol_unmute.wav");

	// Don't wait to mute anymore.
	s_NumSoundsToPlayBeforeMute = UINT_MAX;
	
	// Set the volume.
	Mix_Volume(-1, s_DesiredVolume);
}

// Add a sound to the back of the play queue.
//
// p_FileName:	The name of the file to queue.
//
// returns:		True if successful, false otherwise.
//
bool SoundAddToQueue(char const* const p_FileName)
{
	if (s_SoundInitialized == false)
	{
		return false;
	}
	
	// Is the queue full?
	if (s_NumQueuedSounds == s_MaxQueuedSounds)
	{
		LoggerAddMessage("Sound play queue already full when trying to add \"%s\"", p_FileName);
		return false;
	}
	
	// Load the file.
	auto l_Sample = Mix_LoadWAV(p_FileName);
	
	if (l_Sample == nullptr)
	{
		LoggerAddMessage("Failed to load sound from file \"%s\"", p_FileName);
		return false;
	}
	
	// Add it to the queue.
	s_PlayQueue[s_NumQueuedSounds] = l_Sample;
	s_NumQueuedSounds++;
	return true;
}

// Process sound.
//
void SoundProcess()
{
	if (s_SoundInitialized == false)
	{
		return;
	}
	
	// Was something playing?
	if (s_PlayingChannel != -1)
	{
		// Check if the sound is still playing.
		if (Mix_Playing(s_PlayingChannel) == true)
		{
			return;
		}
		
		s_PlayingChannel = -1;

		// Remove the sound.
		SoundRemoveFromQueue();
		
		// Waiting to mute?
		if (s_NumSoundsToPlayBeforeMute != UINT_MAX)
		{
			// We just played one, adjust the count.
			s_NumSoundsToPlayBeforeMute--;
			
			if (s_NumSoundsToPlayBeforeMute == 0)
			{
				// Set the volume.
				Mix_Volume(-1, 0);
				
				// Reset the counter.
				s_NumSoundsToPlayBeforeMute = UINT_MAX;
			}
		}
	}

	// No queued sounds?
	if (s_NumQueuedSounds == 0)
	{
		return;
	}
	
	// Start playing.
	s_PlayingChannel = Mix_PlayChannel(-1, s_PlayQueue[0], 0);
	
	if (s_PlayingChannel == -1)
	{
		LoggerAddMessage("Failed to play a sound with error \"%s\"", Mix_GetError());
		
		// Remove the sound.
		SoundRemoveFromQueue();
	}
}
