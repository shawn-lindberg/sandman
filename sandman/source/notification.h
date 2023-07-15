
#pragma once

#include <string>

#include "timer.h"

// Types
//

// Functions
//

// Play a notification.
// 
// p_ID:	The ID of the notification to play.
// 
void NotificationPlay(std::string const& p_ID);

// Get the time that the last notification finished.
//
// p_Time:	(Output) The last time.
//
void NotificationGetLastPlayFinishedTime(Time& p_Time);