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
static const std::map<std::string, std::string>	s_notificationIDToSpeechTextMap = 
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
// iD:	The ID of the notification to play.
// 
void NotificationPlay(std::string const& iD)
{
	// Try to find it in the map.
	auto const resultIterator = s_notificationIDToSpeechTextMap.find(iD);

	if (resultIterator == s_notificationIDToSpeechTextMap.end()) 
	{
		Logger::WriteFormattedLine("Tried to play an invalid notification \"%s\".", iD.c_str());
		return;
	}

	auto const& speechText = resultIterator->second;

	// Generate the notification.
	MQTTNotification(speechText);
}

// Get the time that the last notification finished.
//
// time:	(Output) The last time.
//
void NotificationGetLastPlayFinishedTime(Time& time)
{
	MQTTGetLastTextToSpeechFinishedTime(time);
}