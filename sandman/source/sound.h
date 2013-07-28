#pragma once

// Types
//

// Functions
//

// Initialize sound.
//
// returns:		True if successful, false otherwise.
//
bool SoundInitialize();

// Uninitialize sound.
//
void SoundUninitialize();

// Add a sound to the back of the play queue.
//
// p_FileName:	The name of the file to queue.
//
// returns:		True if successful, false otherwise.
//
bool SoundAddToQueue(char const* const p_FileName);

// Process sound.
//
void SoundProcess();
