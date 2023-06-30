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

// Types
//

// A message that we received or need to send later.
struct MessageInfo
{
	// The topic the message was or will be published to.
	std::string	m_Topic;

	// The message payload.
	std::string	m_Payload;
};

// Locals
//

// The client instance.
static mosquitto* s_MosquittoClient = nullptr;

// Track whether we are connected to the host.
static bool s_ConnectedToHost = false;

// Keep track of whether we have ever seen text-to-speech finish.
static bool s_FirstTextToSpeechFinished = false;

// A list of messages to publish once we are able.
static std::vector<MessageInfo> s_PendingMessageList;

// A list of notifications to post once we are able.
static std::vector<std::string> s_PendingNotificationList;

// We need to protect access to the list of received messages.
static std::mutex s_ReceivedMessagesMutex;

// A list of messages we have received to process when we are able.
static std::vector<MessageInfo> s_ReceivedMessageList;

// Keep track of the current dialogue manager session ID.
static std::string s_DialogueManagerSessionID;

// If we have command tokens awaiting confirmation, store them here.
static std::vector<CommandToken> s_CommandTokensPendingConfirmation;

// Functions
//

// Subscribes to a topic.
//
// p_MosquittoClient:	The client instance.
// p_Topic:					The topic that we want to subscribe to.
//
// Returns:	True on success, false on failure.
//
static bool MQTTSubscribeTopic(mosquitto* p_MosquittoClient, const char* p_Topic)
{
	const int l_QoS = 0;
	auto l_ReturnCode = mosquitto_subscribe(p_MosquittoClient, nullptr, p_Topic, l_QoS);

	if (l_ReturnCode != MOSQ_ERR_SUCCESS)
	{
		LoggerAddMessage("Subscription to MQTT topic \"%s\" failed with return code %d", p_Topic,
			l_ReturnCode);		
		return false;
	}
	
	LoggerAddMessage("Subscribed to MQTT topic \"%s\".", p_Topic);
	return true;
}

// Handles acknowledgment of a connection.
//
// p_MosquittoClient:	The client instance that connected.
// p_UserData:				The user data associated with the client instance.
// p_ReturnCode:			The return code of the connection response.
//
void OnConnectCallback(mosquitto* p_MosquittoClient, void* p_UserData, int p_ReturnCode)
{
	if (p_ReturnCode != MOSQ_ERR_SUCCESS)
	{		
		LoggerAddMessage("Connection to MQTT host failed with return code %d", p_ReturnCode);
		return;
	}

	s_ConnectedToHost = true;
	LoggerAddMessage("Connected to MQTT host.");

	// Subscribe to the relevant topics.
	MQTTSubscribeTopic(p_MosquittoClient, "hermes/intent/#");
	MQTTSubscribeTopic(p_MosquittoClient, "hermes/tts/#");
	MQTTSubscribeTopic(p_MosquittoClient, "hermes/dialogueManager/#");
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
	//LoggerAddMessage("Received MQTT message for topic \"%s\": %s", p_Message->topic, 
	//	l_PayloadString);

	std::string const l_Topic(p_Message->topic);

	// Keep track of whether the first text-to-speech finished.
	if (l_Topic.find("hermes/tts/sayFinished") != std::string::npos)
	{
		s_FirstTextToSpeechFinished = true;
	}

	// Helper lambda to save a message to process later.
	auto l_SaveMessage = [&]()
	{
		// Acquire a lock to protect the received message list.
		std::lock_guard<std::mutex> l_MessageGuard(s_ReceivedMessagesMutex);

		MessageInfo l_Message;
		l_Message.m_Topic = l_Topic;
		l_Message.m_Payload = l_PayloadString;

		s_ReceivedMessageList.push_back(l_Message);
	};

	// Only save certain messages to process later.
	if (l_Topic.find("hermes/dialogueManager/") != std::string::npos)
	{
		l_SaveMessage();
		return;
	}

	if (l_Topic.find("hermes/intent/") != std::string::npos) 
	{
		l_SaveMessage();
		return;
	}
}

// Initialize MQTT.
//
bool MQTTInitialize()
{
	LoggerAddMessage("Initializing MQTT support...");

	s_ConnectedToHost = false;
	s_FirstTextToSpeechFinished = false;
	s_DialogueManagerSessionID = "";
	
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

// Publishes a message to a given topic.
//
// p_Topic:		The topic to publish to.
// p_Message:	The message to be published.
//
static void MQTTPublishMessage(char const* p_Topic, char const* p_Message)
{
	if (p_Topic == nullptr)
	{
		return;
	}

	if (p_Message == nullptr)
	{
		return;
	}

	// If we are not yet connected, put the message and the topic on a list to publish once we are.
	if (s_ConnectedToHost == false) {

		MessageInfo l_PendingMessage;
		l_PendingMessage.m_Topic = p_Topic;
		l_PendingMessage.m_Payload = p_Message;

		s_PendingMessageList.push_back(l_PendingMessage);
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
		//LoggerAddMessage("Published message to MQTT topic \"%s\": %s", p_Topic, p_Message);			
		LoggerAddMessage("Published message to MQTT topic \"%s\"", p_Topic);			
	}
}

// End the current dialogue manager session.
//
static void DialogueManagerEndSession()
{
	// Create a properly formatted message that will end the session.
	static constexpr unsigned int l_MessageBufferCapacity = 500;
	char l_MessageBuffer[l_MessageBufferCapacity];

	snprintf(l_MessageBuffer, l_MessageBufferCapacity, "{\"sessionId\": \"%s\", \"text\": \"\"}", 
		s_DialogueManagerSessionID.c_str());

	// Actually publish to the topic.
	char const* l_Topic = "hermes/dialogueManager/endSession";
	MQTTPublishMessage(l_Topic, l_MessageBuffer);
}

// Handles processing a dialogue manager message.
//
// p_Topic:					The topic of the message.
// p_MessageDocument:	The JSON document for the message payload.
// 
static void ProcessDialogueManagerMessage(std::string const& p_Topic, 
	rapidjson::Document const& p_MessageDocument)
{
	// Technically we probably don't need to be able to access the session ID for all cases here, 
	// but it's reasonable to expect and the code is cleanest this way.
	auto const l_SessionIDIterator = p_MessageDocument.FindMember("sessionId");

	if (l_SessionIDIterator == p_MessageDocument.MemberEnd())
	{
		return;
	}
		
	auto const l_SessionID = l_SessionIDIterator->value.GetString();
	
	if (p_Topic.find("sessionStarted") != std::string::npos)
	{
		LoggerAddMessage("Dialogue session started with ID: %s", l_SessionID);
		s_DialogueManagerSessionID = l_SessionID;
		return;
	}

	if (p_Topic.find("sessionEnded") != std::string::npos)
	{
		auto l_GetReason = [&]() -> char const*
		{
			auto const l_TerminationIterator = p_MessageDocument.FindMember("termination");

			if (l_TerminationIterator == p_MessageDocument.MemberEnd())
			{
				return nullptr;
			}

			auto const l_ReasonIterator = l_TerminationIterator->value.FindMember("reason");

			if (l_ReasonIterator == p_MessageDocument.MemberEnd())
			{
				return nullptr;
			}

			return l_ReasonIterator->value.GetString();
		};

		auto const l_Reason = l_GetReason();

		if (l_Reason != nullptr)
		{
			LoggerAddMessage("Dialogue session ended with ID: %s and reason: %s", l_SessionID, 
				l_Reason);
		}
		else
		{
			LoggerAddMessage("Dialogue session ended with ID: %s", l_SessionID);
		}	
	
		s_DialogueManagerSessionID = "";
		return;
	}
}

// Handles processing an intent message.
//
// p_IntentDocument:	The JSON document for the intent payload.
//
static void ProcessIntentMessage(rapidjson::Document const& p_IntentDocument)
{
	// Take into account tokens pending confirmation, but only once.
	auto l_CommandTokens = s_CommandTokensPendingConfirmation;
	s_CommandTokensPendingConfirmation.clear();

	CommandTokenizeJSONDocument(l_CommandTokens, p_IntentDocument);

	if (l_CommandTokens.empty() == true)
	{
		DialogueManagerEndSession();
		return;
	}
		
	char const* l_ConfirmationText = nullptr;
	auto const l_ReturnValue = CommandParseTokens(l_ConfirmationText, l_CommandTokens);

	if (l_ReturnValue == CommandParseTokensReturnTypes::INVALID)
	{
		DialogueManagerEndSession();
		return;
	}

	// Handle missing confirmations, if necessary.
	if (l_ReturnValue != CommandParseTokensReturnTypes::MISSING_CONFIRMATION)
	{
		return;
	}

 	// Save these tokens for next time.
	s_CommandTokensPendingConfirmation = l_CommandTokens;
	
	// Create a properly formatted message that will trigger the confirmation.
	static constexpr unsigned int l_MessageBufferCapacity = 500;
	char l_MessageBuffer[l_MessageBufferCapacity];

	snprintf(l_MessageBuffer, l_MessageBufferCapacity, "{\"sessionId\": \"%s\", \"text\": \"%s\"}", 
		s_DialogueManagerSessionID.c_str(), l_ConfirmationText);

	// Actually publish to the topic.
	char const* l_Topic = "hermes/dialogueManager/continueSession";
	MQTTPublishMessage(l_Topic, l_MessageBuffer);
}

// Process is a message that we have received.
//
// p_Message:	The message we have received.
//
static void MQTTProcessReceivedMessage(MessageInfo const& p_Message)
{
	// Parse the payload as JSON.
	rapidjson::Document l_PayloadDocument;
	l_PayloadDocument.Parse(p_Message.m_Payload.c_str());

	if (l_PayloadDocument.HasParseError() == true)
	{
		return;
	}

	auto const& l_Topic = p_Message.m_Topic;
	
	if (l_Topic.find("hermes/dialogueManager/") != std::string::npos)
	{
		ProcessDialogueManagerMessage(l_Topic, l_PayloadDocument);
		return;
	}

	if (l_Topic.find("hermes/intent/") != std::string::npos) 
	{
		LoggerAddMessage("Received MQTT message for topic \"%s\"", p_Message.m_Topic.c_str());

		ProcessIntentMessage(l_PayloadDocument);
		return;
	}
}

// Generates and publishes a message that causes a spoken notification.
//
// p_Text:	The notification text.
//
static void MQTTPublishNotification(std::string const& p_Text)
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

// Process MQTT.
//
void MQTTProcess()
{
	{
		// Acquire a lock to protect the received message list.
		// NOTE: It is expected that this will be executed from the main thread.
		std::lock_guard<std::mutex> l_MessageGuard(s_ReceivedMessagesMutex);

		for (auto const& l_Message : s_ReceivedMessageList)		
		{
			MQTTProcessReceivedMessage(l_Message);
		}

		// Get rid of the messages.
		s_ReceivedMessageList.clear();
	}

	// If we are connected, send any pending messages.
	if (s_ConnectedToHost == true) {

		for (auto const& l_PendingMessage : s_PendingMessageList)
		{
			MQTTPublishMessage(l_PendingMessage.m_Topic.c_str(), l_PendingMessage.m_Payload.c_str());
		}

		// Get rid of the pending messages.
		s_PendingMessageList.clear();

		if (s_FirstTextToSpeechFinished == true)
		{
			// If we have successfully started playing notifications, go ahead and post the rest.
			for (auto const& l_PendingNotification : s_PendingNotificationList)
			{
				MQTTPublishNotification(l_PendingNotification);
			}

			// Get rid of the pending notifications. 
			s_PendingNotificationList.clear();
		}
		else
		{
			// We use this to tell not only when we are attempting the first notification for the very 
			// first time, but to prevent us from double posting the first notification after we 
			//succeed.
			static std::string s_FirstNotification = "";

			static Time s_LastAttemptTime;

			if ((s_FirstNotification.compare("") == 0) && (s_PendingNotificationList.size() > 0))
			{
				// Pull the first notification off and store it separately.
				s_FirstNotification = s_PendingNotificationList[0];
				s_PendingNotificationList.erase(s_PendingNotificationList.begin());

				// Make our first attempt.
				MQTTPublishNotification(s_FirstNotification);
				TimerGetCurrent(s_LastAttemptTime);

				LoggerAddMessage("Attempted first notification.");
			}

			// See if enough time has passed since our last attempt.
			Time l_CurrentTime;
			TimerGetCurrent(l_CurrentTime);

			auto const l_DurationMS = TimerGetElapsedMilliseconds(s_LastAttemptTime, l_CurrentTime);
			auto const l_DurationSeconds = static_cast<unsigned long>(l_DurationMS) / 1000;
			
			static constexpr unsigned long l_ReattemptTimeSeconds = 5;

			if ((s_FirstNotification.compare("") != 0) && 
				(l_DurationSeconds >= l_ReattemptTimeSeconds))
			{
				// If so, reattempt the notification.
				MQTTPublishNotification(s_FirstNotification);
				TimerGetCurrent(s_LastAttemptTime);

				LoggerAddMessage("Reattempted first notification.");
			}
		}
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

// Causes a spoken notification.
//
// p_Text:	The notification text.
//
void MQTTNotification(std::string const& p_Text)
{
	s_PendingNotificationList.push_back(p_Text);
}
