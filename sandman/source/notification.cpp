#include "notification.h"

#include <map>

#include "logger.h"
#include "mqtt.h"
#include "sound.h"

#define DATADIR		AM_DATADIR

// Constants
//

// Types
//

// A description of a notification.
struct NotificationDescription
{
	// The name of a sound file to play.
	std::string	m_SoundFileName;

	// Text to be spoken.
	std::string	m_SpeechText;
};

// Locals
//

// A map from identifiers to notification descriptions.
static const std::map<std::string, NotificationDescription>	s_NotificationIDToDescriptionMap = 
{
	{ "initialized", 				{ DATADIR "audio/initialized.wav",
										  "Sandman initialized" } },
	{ "running",					{ DATADIR "audio/running.wav",
										  "Sandman is running" } },
	{ "schedule_running",		{ DATADIR "audio/sched_running.wav",
										  "Schedule is running" } },
	{ "schedule_start",			{ DATADIR "audio/sched_start.wav",
										  "Schedule started" } },
	{ "schedule_stop",			{ DATADIR "audio/sched_stop.wav",
										  "Schedule stopped" } },
	{ "control_connected",		{ DATADIR "audio/control_connected.wav",
										  "Controller connected" } },
	{ "control_disconnected",	{ DATADIR "audio/control_disconnect.wav",
										  "Controller disconnected" } },
	{ "back_moving_up",			{ DATADIR "audio/back_moving_up.wav",
										  "Raising the back" } },
	{ "back_moving_down",		{ DATADIR "audio/back_moving_down.wav",
										  "Lowering the back" } },
	{ "back_stop",					{ DATADIR "audio/back_stop.wav",
										  "Back stopped" } },
	{ "elev_moving_up",			{ DATADIR "audio/elev_moving_up.wav",
										  "Raising the elevation" } },
	{ "elev_moving_down",		{ DATADIR "audio/elev_moving_down.wav",
										  "Lowering the elevation" } },
	{ "elev_stop",					{ DATADIR "audio/elev_stop.wav",
										  "Elevation stopped" } },
	{ "legs_moving_up",			{ DATADIR "audio/legs_moving_up.wav",
										  "Raising the legs" } },
	{ "legs_moving_down",		{ DATADIR "audio/legs_moving_down.wav",
										  "Lowering the legs" } },
	{ "legs_stop",					{ DATADIR "audio/legs_stop.wav",
										  "Legs stopped" } },
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
	auto const l_ResultIterator = s_NotificationIDToDescriptionMap.find(p_ID);

	if (l_ResultIterator == s_NotificationIDToDescriptionMap.end()) 
	{
		LoggerAddMessage("Tried to play an invalid notification \"%s\".", p_ID.c_str());
		return;
	}

	auto const& l_Description = l_ResultIterator->second;

	// Play the sound.
	SoundAddToQueue(l_Description.m_SoundFileName.c_str());

	// Generate the notification.
	MQTTNotification(l_Description.m_SpeechText);
}
