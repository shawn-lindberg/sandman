#include "notification.h"

#include <map>

#include "logger.h"
#include "mqtt.h"

#define DATADIR		AM_DATADIR

// Constants
//

// Types
//

// Locals
//

// A map from identifiers to notification speech text.
static const std::map<std::string, std::string>	s_NotificationIDToSpeechTextMap = 
{
	{ "initialized", 				"Sandman initialized" },
	{ "running",					"Sandman is running" },
	{ "schedule_running",		"Schedule is running" },
	{ "schedule_start",			"Schedule started" },
	{ "schedule_stop",			"Schedule stopped" },
	{ "control_connected",		"Controller connected" },
	{ "control_disconnected",	"Controller disconnected" },
	{ "back_moving_up",			"Raising the back" },
	{ "back_moving_down",		"Lowering the back" },
	{ "back_stop",					"Back stopped" },
	{ "elev_moving_up",			"Raising the elevation" },
	{ "elev_moving_down",		"Lowering the elevation" },
	{ "elev_stop",					"Elevation stopped" },
	{ "legs_moving_up",			"Raising the legs" },
	{ "legs_moving_down",		"Lowering the legs" },
	{ "legs_stop",					"Legs stopped" },
  	{ "canceled",					"Canceled" },
	{ "restarting",				"Restarting" },
};

// Functions
//

// Play a notification.
// 
// p_ID:	The ID of the notification to play.
// 
void NotificationPlay(std::string const& p_ID)
{
	// Try to find it in the map.
	auto const l_ResultIterator = s_NotificationIDToSpeechTextMap.find(p_ID);

	if (l_ResultIterator == s_NotificationIDToSpeechTextMap.end()) 
	{
		LoggerAddMessage("Tried to play an invalid notification \"%s\".", p_ID.c_str());
		return;
	}

	auto const& l_SpeechText = l_ResultIterator->second;

	// Generate the notification.
	MQTTNotification(l_SpeechText);
}

// Get the time that the last notification finished.
//
// p_Time:	(Output) The last time.
//
void NotificationGetLastPlayFinishedTime(Time& p_Time)
{
	MQTTGetLastTextToSpeechFinishedTime(p_Time);
}