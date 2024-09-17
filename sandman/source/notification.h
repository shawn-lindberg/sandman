
#pragma once

#include <string>

#include "timer.h"

// Types
//

// Functions
//

// Play a notification.
// 
// notificationID:	The ID of the notification to play.
// 
void NotificationPlay(std::string const& notificationID);

// Get the time that the last notification finished.
//
// time:	(Output) The last time.
//
void NotificationGetLastPlayFinishedTime(Time& time);