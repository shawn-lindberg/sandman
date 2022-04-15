#include "mqtt.h"

#include <mosquitto.h> 
#include "rapidjson/document.h"

#include "command.h"
#include "logger.h"

#define DATADIR		AM_DATADIR

// Constants
//


// Locals
//

// The client instance.
static mosquitto* s_MosquittoClient = nullptr;


// Functions
//

// Handles acknowledgment of a connection.
//
// p_MosquittoClient:	The client instance that connected.
// p_UserData:			The user data associated with the client instance.
// p_ReturnCode:		The return code of the connection response.
//
void OnConnectCallback(mosquitto* p_MosquittoClient, void* p_UserData, int p_ReturnCode)
{
	if (p_ReturnCode != MOSQ_ERR_SUCCESS)
	{		
		LoggerAddMessage("Connection to MQTT host failed with return code %d", p_ReturnCode);
		return;
	}

	LoggerAddMessage("Connected to MQTT host.");

	// Subscribe to the relevant topics.
	const char* l_Topic = "hermes/intent/#";
	const int l_QoS = 0;
	auto l_ReturnCode = mosquitto_subscribe(p_MosquittoClient, nullptr, l_Topic, l_QoS);

	if (l_ReturnCode != MOSQ_ERR_SUCCESS)
	{
		LoggerAddMessage("Subscription to MQTT topic \"%s\" failed with return code %d", l_Topic,
			l_ReturnCode);		
	} 
	else 
	{
		LoggerAddMessage("Subscribed to MQTT topic \"%s\".", l_Topic);
	}
}

// Handles message for a subscribed topic.
//
// p_MosquittoClient:	The client instance that subscribed.
// p_UserData:			The user data associated with the client instance.
// p_Message:			The message data that was received.
//
void OnMessageCallback(mosquitto* p_MosquittoClient, void* p_UserData, 
	const mosquitto_message* p_Message)
{
	const auto* l_PayloadString = reinterpret_cast<char*>(p_Message->payload);
	LoggerAddMessage("Received MQTT message for topic \"%s\".", p_Message->topic);
	// LoggerAddMessage("Received MQTT message for topic \"%s\": %s", p_Message->topic, 
	// 	l_PayloadString);

	// Parse the payload as JSON.
	rapidjson::Document l_PayloadDocument;
	l_PayloadDocument.Parse(l_PayloadString);

	if (l_PayloadDocument.HasParseError() == true)
	{
		return;
	}

	std::vector<CommandToken> l_CommandTokens;
	CommandTokenizeJSONDocument(l_CommandTokens, l_PayloadDocument);

	CommandParseTokens(l_CommandTokens);
}

// Initialize MQTT.
//
bool MQTTInitialize()
{
	LoggerAddMessage("Initializing MQTT support...");
	
	if (mosquitto_lib_init() != MOSQ_ERR_SUCCESS)
	{
		LoggerAddMessage("\tfailed");
		return false;
	}
		
	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");

	LoggerAddMessage("Creating MQTT client...");

	const bool l_CleanSession = true;
    s_MosquittoClient = mosquitto_new("sandman", l_CleanSession, nullptr);

	if (s_MosquittoClient == nullptr) 
	{
		LoggerAddMessage("\tfailed");
		return false;
	}
	
	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");

	// Set some necessary callbacks.
	mosquitto_connect_callback_set(s_MosquittoClient, OnConnectCallback);
	mosquitto_message_callback_set(s_MosquittoClient, OnMessageCallback);

	LoggerAddMessage("Connecting to MQTT host...");

	static constexpr int l_Port = 12183;
	static constexpr int l_KeepAliveSeconds = 60;
	const auto l_ReturnCode = mosquitto_connect(s_MosquittoClient, "localhost", l_Port, 
		l_KeepAliveSeconds);
		
	if (l_ReturnCode != MOSQ_ERR_SUCCESS)
	{		
		LoggerAddMessage("\tfailed with return code %d", l_ReturnCode);
		return false;
	}
		
	LoggerAddMessage("\tsucceeded");
	LoggerAddMessage("");

	// Start processing in another thread.
	mosquitto_loop_start(s_MosquittoClient);

    return true;
}

// Uninitialize MQTT.
//
void MQTTUninitialize()
{
	if (s_MosquittoClient != nullptr)
	{
		// Stop any processing that may have been occurring in another thread.
		const auto l_Force = true;
		mosquitto_loop_stop(s_MosquittoClient, l_Force);

		mosquitto_disconnect(s_MosquittoClient);
		mosquitto_destroy(s_MosquittoClient);
	}
	
	mosquitto_lib_cleanup();
}

// Process MQTT.
//
void MQTTProcess()
{
}
