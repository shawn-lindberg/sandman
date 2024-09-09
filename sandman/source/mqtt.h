#pragma once

#include <string>

#include "timer.h"

// Functions
//

// Initialize MQTT.
//
bool MQTTInitialize();

// Uninitialize MQTT.
//
void MQTTUninitialize();

// Process MQTT.
//
void MQTTProcess();

// Generates and publishes a message to cause the provided text to be spoken.
//
// text:	The text that should be spoken.
//
void MQTTTextToSpeech(std::string const& text);

// Causes a spoken notification.
//
// text:	The notification text.
//
void MQTTNotification(std::string const& text);

// Get the time that the last text-to-speech finished.
//
// time:	(Output) The last time.
//
void MQTTGetLastTextToSpeechFinishedTime(Time& time);