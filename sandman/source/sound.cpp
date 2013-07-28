#include <SDL.h>
#include <SDL_mixer.h>

#include "logger.h"

// Constants
//

// The most sounds that can be queued at once.
static unsigned int const s_MaxQueuedSounds = 8;

// Locals
//

// Whether the system has been initialized.
static bool s_SoundInitialized = false;

// A queue of sounds to play.
static Mix_Chunk* s_PlayQueue[s_MaxQueuedSounds];
static unsigned int s_NumQueuedSounds = 0;

// Right now there should be only one channel, but this should be a bit more future proof.
static int s_PlayingChannel = -1;

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
		s_PlayQueue[l_SoundIndex] = NULL;
	}
	
	s_NumQueuedSounds = 0;
	s_PlayingChannel = -1;
	
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
		if (s_PlayQueue[l_SoundIndex] != NULL)
		{
			Mix_FreeChunk(s_PlayQueue[l_SoundIndex]);
		}
		
		s_PlayQueue[l_SoundIndex] = NULL;
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
	Mix_Chunk* l_Sample = Mix_LoadWAV(p_FileName);
	
	if (l_Sample == NULL)
	{
		LoggerAddMessage("Failed to load sound from file \"%s\"", p_FileName);
		return false;
	}
	
	// Add it to the queue.
	s_PlayQueue[s_NumQueuedSounds] = l_Sample;
	s_NumQueuedSounds++;
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
