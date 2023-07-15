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
// p_Text:	The text that should be spoken.
//
void MQTTTextToSpeech(std::string const& p_Text);

// Causes a spoken notification.
//
// p_Text:	The notification text.
//
void MQTTNotification(std::string const& p_Text);

// Get the time that the last text-to-speech finished.
//
// p_Time:	(Output) The last time.
//
void MQTTGetLastTextToSpeechFinishedTime(Time& p_Time);