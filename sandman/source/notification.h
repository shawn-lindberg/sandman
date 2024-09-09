
#pragma once

#include <string>

#include "timer.h"

// Types
//

// Functions
//

// Play a notification.
// 
// iD:	The ID of the notification to play.
// 
void NotificationPlay(std::string const& iD);

// Get the time that the last notification finished.
//
// time:	(Output) The last time.
//
void NotificationGetLastPlayFinishedTime(Time& time);