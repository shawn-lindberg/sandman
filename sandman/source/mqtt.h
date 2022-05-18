#pragma once

#include <string>

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

// Generates and publishes a message that causes a spoken notification.
//
// p_Text:	The notification text.
//
void MQTTNotification(std::string const& p_Text);