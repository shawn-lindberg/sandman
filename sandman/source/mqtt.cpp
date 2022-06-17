#include "mqtt.h"

#include <mutex>
#include <unistd.h>

#include <mosquitto.h> 
#include "rapidjson/document.h"

#include "command.h"
#include "logger.h"
#include "timer.h"

#define DATADIR		AM_DATADIR

// Constants
//


// Locals
//

// The client instance.
static mosquitto* s_MosquittoClient = nullptr;

// We need to protect access to the list of commands.
std::mutex s_CommandsMutex;

// A list of commands to be processed.
std::vector<std::vector<CommandToken>> s_CommandsList;

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
// p_UserData:				The user data associated with the client instance.
// p_Message:				The message data that was received.
//
void OnMessageCallback(mosquitto* p_MosquittoClient, void* p_UserData, 
	const mosquitto_message* p_Message)
{
	const auto* l_PayloadString = reinterpret_cast<char*>(p_Message->payload);
	LoggerAddMessage("Received MQTT message for topic \"%s\".", p_Message->topic);
	//LoggerAddMessage("Received MQTT message for topic \"%s\": %s", p_Message->topic, 
	//	l_PayloadString);

	// Parse the payload as JSON.
	rapidjson::Document l_PayloadDocument;
	l_PayloadDocument.Parse(l_PayloadString);

	if (l_PayloadDocument.HasParseError() == true)
	{
		return;
	}

	std::vector<CommandToken> l_CommandTokens;
	CommandTokenizeJSONDocument(l_CommandTokens, l_PayloadDocument);

	if (l_CommandTokens.empty() == false)
	{
		// Acquire a lock to protect the command list.
		std::lock_guard<std::mutex> l_CommandsGuard(s_CommandsMutex);

		s_CommandsList.push_back(l_CommandTokens);
	}
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

	int l_MajorVersion = 0;
	int l_MinorVersion = 0;
	int l_Revision = 0;
	mosquitto_lib_version(&l_MajorVersion, &l_MinorVersion, &l_Revision);

	LoggerAddMessage("MQTT version %i.%i.%i", l_MajorVersion, l_MinorVersion, l_Revision);

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

	// We are going to repeatedly attempt to connect roughly every second for a 
	// certain period of time.
	auto l_Connected = false;
	
	Time l_ConnectStartTime;
	TimerGetCurrent(l_ConnectStartTime);
		
	while (l_Connected == false)
	{
		static constexpr int l_Port = 12183;
		static constexpr int l_KeepAliveSeconds = 60;

		const auto l_ReturnCode = mosquitto_connect(s_MosquittoClient, "localhost", 
			l_Port, l_KeepAliveSeconds);
		
		if (l_ReturnCode == MOSQ_ERR_SUCCESS)
		{		
			l_Connected = true;
			break;
		}
	
		// Figure out how long we have been attempting to connect.
		Time l_ConnectCurrentTime;
		TimerGetCurrent(l_ConnectCurrentTime);
		
		float const l_DurationMS = TimerGetElapsedMilliseconds(l_ConnectStartTime, 
			l_ConnectCurrentTime);
		auto const l_DurationSeconds = static_cast<unsigned long>(l_DurationMS) / 
			1000;
		
		// Attempt for five minutes at most.
		static constexpr unsigned long l_TimeoutDurationSeconds = 5 * 60;
		
		if (l_DurationSeconds >= l_TimeoutDurationSeconds)
		{
			break;
		}
		
		// Sleep for one second.
		sleep(1);
	}

	if (l_Connected == false) {
		
		LoggerAddMessage("\tfailed");
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
	// Acquire a lock to protect the command list.
	// NOTE: It is expected that this will be executed from the main thread.
	std::lock_guard<std::mutex> l_CommandsGuard(s_CommandsMutex);

	for (auto const& l_CommandTokens : s_CommandsList)
	{
		CommandParseTokens(l_CommandTokens);
	}

	// Get rid of the commands.
	s_CommandsList.clear();
}

// Publishes a message to a given topic.
//
// p_Topic:		The topic to publish to.
// p_Message:	The message to be published.
//
void MQTTPublishMessage(char const* p_Topic, char const* p_Message)
{
	if (p_Topic == nullptr)
	{
		return;
	}

	if (p_Message == nullptr)
	{
		return;
	}

	// I thought that we needed to count the terminator here, but it actually doesn't work if we do. 
	// Go figure.
	auto const l_MessageLength = strlen(p_Message);

	int const l_QoS = 0;
	bool const l_Retain = false;
	auto l_ReturnCode = mosquitto_publish(s_MosquittoClient, nullptr, p_Topic, l_MessageLength,
		p_Message, l_QoS, l_Retain);

	if (l_ReturnCode != MOSQ_ERR_SUCCESS)
	{
		LoggerAddMessage("Publish to MQTT topic \"%s\" failed with return code %d", p_Topic,
			l_ReturnCode);		
	} 
	else 
	{
		LoggerAddMessage("Published message to MQTT topic \"%s\": %s", p_Topic, p_Message);			
	}
}

// Generates and publishes a message to cause the provided text to be spoken.
//
// p_Text:	The text that should be spoken.
//
void MQTTTextToSpeech(std::string const& p_Text)
{
	// Create a properly formatted message that will trigger the text to be spoken.
	static constexpr unsigned int l_MessageBufferCapacity = 500;
	char l_MessageBuffer[l_MessageBufferCapacity];

	snprintf(l_MessageBuffer, l_MessageBufferCapacity, 
		"{\"text\": \"%s\", \"siteId\": \"default\", \"lang\": null, \"id\": \"\", "
		"\"sessionId\": \"\", \"volume\": 1.0}", p_Text.c_str());

	// Actually publish to the topic.
	char const* l_Topic = "hermes/tts/say";
	MQTTPublishMessage(l_Topic, l_MessageBuffer);
}

// Generates and publishes a message that causes a spoken notification.
//
// p_Text:	The notification text.
//
void MQTTNotification(std::string const& p_Text)
{
	// Create a properly formatted message that will trigger the notification.
	static constexpr unsigned int l_MessageBufferCapacity = 500;
	char l_MessageBuffer[l_MessageBufferCapacity];

	snprintf(l_MessageBuffer, l_MessageBufferCapacity, 
		"{\"init\": {\"type\": \"notification\", \"text\": \"%s\"}, \"siteId\": \"default\"}",
		p_Text.c_str());

	// Actually publish to the topic.
	char const* l_Topic = "hermes/dialogueManager/startSession";
	MQTTPublishMessage(l_Topic, l_MessageBuffer);
}
