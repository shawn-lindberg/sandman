#include "mqtt.h"

#include <mutex>
#include <unistd.h>

#include <mosquitto.h> 
#include "rapidjson/document.h"

#include "command.h"
#include "logger.h"

#define DATADIR		AM_DATADIR

// Constants
//

// Types
//

// A message that we received or need to send later.
struct MessageInfo
{
	// The topic the message was or will be published to.
	std::string	m_topic;

	// The message payload.
	std::string	m_payload;
};

// Locals
//

// The client instance.
static mosquitto* s_mosquittoClient = nullptr;

// Track whether we are connected to the host.
static bool s_connectedToHost = false;

// Keep track of whether we have ever seen text-to-speech finish.
static bool s_firstTextToSpeechFinished = false;

// Keep track of the last time text-to-speech finished.
static Time s_lastTextToSpeechFinishedTime;

// A list of messages to publish once we are able.
static std::vector<MessageInfo> s_pendingMessageList;

// A list of notifications to post once we are able.
static std::vector<std::string> s_pendingNotificationList;

// We need to protect access to the list of received messages.
static std::mutex s_receivedMessagesMutex;

// A list of messages we have received to process when we are able.
static std::vector<MessageInfo> s_receivedMessageList;

// Keep track of the current dialogue manager session ID.
static std::string s_dialogueManagerSessionID;

// If we have command tokens awaiting confirmation, store them here.
static std::vector<CommandToken> s_commandTokensPendingConfirmation;

// Functions
//

// Subscribes to a topic.
//
// mosquittoClient:	The client instance.
// topic:					The topic that we want to subscribe to.
//
// Returns:	True on success, false on failure.
//
static bool MQTTSubscribeTopic(mosquitto* mosquittoClient, const char* topic)
{
	const int qualityOfService = 0;
	auto returnCode = mosquitto_subscribe(mosquittoClient, nullptr, topic, qualityOfService);

	if (returnCode != MOSQ_ERR_SUCCESS)
	{
		Logger::WriteFormattedLine(Shell::Red,
											"Subscription to MQTT topic \"%s\" failed with return code %d",
											topic, returnCode);
		return false;
	}

	Logger::WriteFormattedLine("Subscribed to MQTT topic \"%s\".", topic);
	return true;
}

// Handles acknowledgment of a connection.
//
// mosquittoClient:	The client instance that connected.
// userData:				The user data associated with the client instance.
// returnCode:			The return code of the connection response.
//
void OnConnectCallback(mosquitto* mosquittoClient, void* /* userData */, int returnCode)
{
	if (returnCode != MOSQ_ERR_SUCCESS)
	{
		Logger::WriteFormattedLine(Shell::Red, "Connection to MQTT host failed with return code %d",
											returnCode);
		return;
	}

	s_connectedToHost = true;
	Logger::WriteFormattedLine("Connected to MQTT host.");

	// Subscribe to the relevant topics.
	MQTTSubscribeTopic(mosquittoClient, "hermes/intent/#");
	MQTTSubscribeTopic(mosquittoClient, "hermes/tts/#");
	MQTTSubscribeTopic(mosquittoClient, "hermes/dialogueManager/#");
}

// Handles message for a subscribed topic.
//
// mosquittoClient:	The client instance that subscribed.
// userData:				The user data associated with the client instance.
// message:				The message data that was received.
//
void OnMessageCallback(mosquitto* /* mosquittoClient */, void* /* userData */,
							  mosquitto_message const* message)
{
	const auto* payloadString = reinterpret_cast<char*>(message->payload);
	//Logger::WriteFormattedLine("Received MQTT message for topic \"%s\": %s", message->topic, 
	//	payloadString);

	std::string const topic(message->topic);

	// Keep track of whether the first text-to-speech finished.
	if (topic.find("hermes/tts/sayFinished") != std::string::npos)
	{
		s_firstTextToSpeechFinished = true;

		// Record this time.
		TimerGetCurrent(s_lastTextToSpeechFinishedTime);
	}

	// Helper lambda to save a message to process later.
	auto SaveMessage = [&]()
	{
		// Acquire a lock to protect the received message list.
		std::lock_guard<std::mutex> messageGuard(s_receivedMessagesMutex);

		MessageInfo messageObject;
		messageObject.m_topic = topic;
		messageObject.m_payload = payloadString;

		s_receivedMessageList.push_back(messageObject);
	};

	// Only save certain messages to process later.
	if (topic.find("hermes/dialogueManager/") != std::string::npos)
	{
		SaveMessage();
		return;
	}

	if (topic.find("hermes/intent/") != std::string::npos) 
	{
		SaveMessage();
		return;
	}
}

// Initialize MQTT.
//
bool MQTTInitialize()
{
	Logger::WriteLine("Initializing MQTT support...");

	s_connectedToHost = false;
	s_firstTextToSpeechFinished = false;
	s_dialogueManagerSessionID = "";
	
	if (mosquitto_lib_init() != MOSQ_ERR_SUCCESS)
	{
		Logger::WriteLine('\t', Shell::Red("failed"));
		return false;
	}
		
	Logger::WriteLine('\t', Shell::Green("succeeded"));
	Logger::WriteLine();

	int majorVersion = 0;
	int minorVersion = 0;
	int revision = 0;
	mosquitto_lib_version(&majorVersion, &minorVersion, &revision);

	Logger::WriteFormattedLine("MQTT version %i.%i.%i", majorVersion, minorVersion, revision);

	Logger::WriteFormattedLine("Creating MQTT client...");

	const bool cleanSession = true;
    s_mosquittoClient = mosquitto_new("sandman", cleanSession, nullptr);

	if (s_mosquittoClient == nullptr) 
	{
		Logger::WriteLine('\t', Shell::Red("failed"));
		return false;
	}
	
	Logger::WriteLine('\t', Shell::Green("succeeded"));
	Logger::WriteLine();

	// Set some necessary callbacks.
	mosquitto_connect_callback_set(s_mosquittoClient, OnConnectCallback);
	mosquitto_message_callback_set(s_mosquittoClient, OnMessageCallback);

	Logger::WriteFormattedLine("Connecting to MQTT host...");

	// We are going to repeatedly attempt to connect roughly every second for a 
	// certain period of time.
	auto connected = false;
	
	Time connectStartTime;
	TimerGetCurrent(connectStartTime);
		
	while (connected == false)
	{
		static constexpr int kPort{ 12183 };
		static constexpr int kKeepAliveSeconds{ 60 };

		const auto returnCode = mosquitto_connect(s_mosquittoClient, "localhost", 
			kPort, kKeepAliveSeconds);
		
		if (returnCode == MOSQ_ERR_SUCCESS)
		{		
			connected = true;
			break;
		}
	
		// Figure out how long we have been attempting to connect.
		Time connectCurrentTime;
		TimerGetCurrent(connectCurrentTime);
		
		float const durationMS = TimerGetElapsedMilliseconds(connectStartTime, connectCurrentTime);
		auto const durationSeconds = static_cast<unsigned long>(durationMS) / 1'000;
		
		// Attempt for five minutes at most.
		static constexpr unsigned long kTimeoutDurationSeconds = 5 * 60;
		
		if (durationSeconds >= kTimeoutDurationSeconds)
		{
			break;
		}
		
		// Sleep for one second.
		sleep(1);
	}

	if (connected == false) {
		
		Logger::WriteLine('\t', Shell::Red("failed"));
		return false;
	}
	
	Logger::WriteLine('\t', Shell::Green("succeeded"));
	Logger::WriteLine();

	// Start processing in another thread.
	mosquitto_loop_start(s_mosquittoClient);

   return true;
}

// Uninitialize MQTT.
//
void MQTTUninitialize()
{
	if (s_mosquittoClient != nullptr)
	{
		// Stop any processing that may have been occurring in another thread.
		const auto force = true;
		mosquitto_loop_stop(s_mosquittoClient, force);

		mosquitto_disconnect(s_mosquittoClient);
		mosquitto_destroy(s_mosquittoClient);
	}
	
	mosquitto_lib_cleanup();
}

// Publishes a message to a given topic.
//
// topic:		The topic to publish to.
// message:	The message to be published.
//
static void MQTTPublishMessage(char const* topic, char const* message)
{
	if (topic == nullptr)
	{
		return;
	}

	if (message == nullptr)
	{
		return;
	}

	// If we are not yet connected, put the message and the topic on a list to publish once we are.
	if (s_connectedToHost == false) {

		MessageInfo pendingMessage;
		pendingMessage.m_topic = topic;
		pendingMessage.m_payload = message;

		s_pendingMessageList.push_back(pendingMessage);
		return;
	}

	// I thought that we needed to count the terminator here, but it actually doesn't work if we do. 
	// Go figure.
	auto const messageLength = std::strlen(message);

	int const qualityOfService = 0;
	bool const retain = false;
	auto returnCode = mosquitto_publish(s_mosquittoClient, nullptr, topic, messageLength,
		message, qualityOfService, retain);

	if (returnCode != MOSQ_ERR_SUCCESS)
	{
		Logger::WriteFormattedLine(
			Shell::Red, "Publish to MQTT topic \"%s\" failed with return code %d", topic, returnCode);
	}
	else
	{
		//LoggerAddMessage("Published message to MQTT topic \"%s\": %s", p_topic, p_message);
		Logger::WriteFormattedLine("Published message to MQTT topic \"%s\"", topic);
	}
}

// End the current dialogue manager session.
//
static void DialogueManagerEndSession()
{
	// Create a properly formatted message that will end the session.
	static constexpr std::size_t kMessageBufferCapacity{ 500u };
	char messageBuffer[kMessageBufferCapacity];

	std::snprintf(messageBuffer, kMessageBufferCapacity, "{\"sessionId\": \"%s\", \"text\": \"\"}", 
		s_dialogueManagerSessionID.c_str());

	// Actually publish to the topic.
	char const* topic = "hermes/dialogueManager/endSession";
	MQTTPublishMessage(topic, messageBuffer);
}

// Handles processing a dialogue manager message.
//
// topic:					The topic of the message.
// messageDocument:	The JSON document for the message payload.
// 
static void ProcessDialogueManagerMessage(std::string const& topic, 
	rapidjson::Document const& messageDocument)
{
	// Technically we probably don't need to be able to access the session ID for all cases here, 
	// but it's reasonable to expect and the code is cleanest this way.
	auto const sessionIDIterator = messageDocument.FindMember("sessionId");

	if (sessionIDIterator == messageDocument.MemberEnd())
	{
		return;
	}
		
	auto const sessionID = sessionIDIterator->value.GetString();
	
	if (topic.find("sessionStarted") != std::string::npos)
	{
		Logger::WriteFormattedLine("Dialogue session started with ID: %s", sessionID);
		s_dialogueManagerSessionID = sessionID;
		return;
	}

	if (topic.find("sessionEnded") != std::string::npos)
	{
		auto GetReason = [&]() -> char const*
		{
			auto const terminationIterator = messageDocument.FindMember("termination");

			if (terminationIterator == messageDocument.MemberEnd())
			{
				return nullptr;
			}

			auto const reasonIterator = terminationIterator->value.FindMember("reason");

			if (reasonIterator == messageDocument.MemberEnd())
			{
				return nullptr;
			}

			return reasonIterator->value.GetString();
		};

		auto const reason = GetReason();

		if (reason != nullptr)
		{
			Logger::WriteFormattedLine("Dialogue session ended with ID: %s and reason: %s", sessionID, 
				reason);
		}
		else
		{
			Logger::WriteFormattedLine("Dialogue session ended with ID: %s", sessionID);
		}	
	
		s_dialogueManagerSessionID = "";
		return;
	}
}

// Handles processing an intent message.
//
// intentDocument:	The JSON document for the intent payload.
//
static void ProcessIntentMessage(rapidjson::Document const& intentDocument)
{
	// Take into account tokens pending confirmation, but only once.
	auto commandTokens = s_commandTokensPendingConfirmation;
	s_commandTokensPendingConfirmation.clear();

	CommandTokenizeJSONDocument(commandTokens, intentDocument);

	if (commandTokens.empty() == true)
	{
		DialogueManagerEndSession();
		return;
	}
		
	char const* confirmationText = nullptr;
	auto const returnValue = CommandParseTokens(confirmationText, commandTokens);

	if (returnValue == CommandParseTokensReturnTypes::kInvalid)
	{
		DialogueManagerEndSession();
		return;
	}

	// Handle missing confirmations, if necessary.
	if (returnValue != CommandParseTokensReturnTypes::kMissingConfirmation)
	{
		return;
	}

 	// Save these tokens for next time.
	s_commandTokensPendingConfirmation = commandTokens;
	
	// Create a properly formatted message that will trigger the confirmation.
	static constexpr std::size_t kMessageBufferCapacity{ 500u };
	char messageBuffer[kMessageBufferCapacity];

	snprintf(messageBuffer, kMessageBufferCapacity, "{\"sessionId\": \"%s\", \"text\": \"%s\"}", 
		s_dialogueManagerSessionID.c_str(), confirmationText);

	// Actually publish to the topic.
	char const* topic = "hermes/dialogueManager/continueSession";
	MQTTPublishMessage(topic, messageBuffer);
}

// Process is a message that we have received.
//
// message:	The message we have received.
//
static void MQTTProcessReceivedMessage(MessageInfo const& message)
{
	// Parse the payload as JSON.
	rapidjson::Document payloadDocument;
	payloadDocument.Parse(message.m_payload.c_str());

	if (payloadDocument.HasParseError() == true)
	{
		return;
	}

	auto const& topic = message.m_topic;
	
	if (topic.find("hermes/dialogueManager/") != std::string::npos)
	{
		ProcessDialogueManagerMessage(topic, payloadDocument);
		return;
	}

	if (topic.find("hermes/intent/") != std::string::npos) 
	{
		Logger::WriteFormattedLine("Received MQTT message for topic \"%s\"", message.m_topic.c_str());

		ProcessIntentMessage(payloadDocument);
		return;
	}
}

// Generates and publishes a message that causes a spoken notification.
//
// text:	The notification text.
//
static void MQTTPublishNotification(std::string const& text)
{
	// Create a properly formatted message that will trigger the notification.
	static constexpr std::size_t kMessageBufferCapacity{ 500u };
	char messageBuffer[kMessageBufferCapacity];

	snprintf(messageBuffer, kMessageBufferCapacity, 
		"{\"init\": {\"type\": \"notification\", \"text\": \"%s\"}, \"siteId\": \"default\"}",
		text.c_str());

	// Actually publish to the topic.
	char const* topic = "hermes/dialogueManager/startSession";
	MQTTPublishMessage(topic, messageBuffer);
}

// Process MQTT.
//
void MQTTProcess()
{
	{
		// Acquire a lock to protect the received message list.
		// NOTE: It is expected that this will be executed from the main thread.
		std::lock_guard<std::mutex> messageGuard(s_receivedMessagesMutex);

		for (auto const& message : s_receivedMessageList)		
		{
			MQTTProcessReceivedMessage(message);
		}

		// Get rid of the messages.
		s_receivedMessageList.clear();
	}

	// If we are connected, send any pending messages.
	if (s_connectedToHost == true) {

		for (auto const& pendingMessage : s_pendingMessageList)
		{
			MQTTPublishMessage(pendingMessage.m_topic.c_str(), pendingMessage.m_payload.c_str());
		}

		// Get rid of the pending messages.
		s_pendingMessageList.clear();

		if (s_firstTextToSpeechFinished == true)
		{
			// If we have successfully started playing notifications, go ahead and post the rest.
			for (auto const& pendingNotification : s_pendingNotificationList)
			{
				MQTTPublishNotification(pendingNotification);
			}

			// Get rid of the pending notifications. 
			s_pendingNotificationList.clear();
		}
		else
		{
			// We use this to tell not only when we are attempting the first notification for the very 
			// first time, but to prevent us from double posting the first notification after we 
			//succeed.
			static std::string s_firstNotification = "";

			static Time s_lastAttemptTime;

			if ((s_firstNotification.compare("") == 0) && (s_pendingNotificationList.size() > 0))
			{
				// Pull the first notification off and store it separately.
				s_firstNotification = s_pendingNotificationList[0];
				s_pendingNotificationList.erase(s_pendingNotificationList.begin());

				// Make our first attempt.
				MQTTPublishNotification(s_firstNotification);
				TimerGetCurrent(s_lastAttemptTime);

				Logger::WriteFormattedLine("Attempted first notification.");
			}

			// See if enough time has passed since our last attempt.
			Time currentTime;
			TimerGetCurrent(currentTime);

			auto const durationMS = TimerGetElapsedMilliseconds(s_lastAttemptTime, currentTime);
			auto const durationSeconds = static_cast<unsigned long>(durationMS) / 1'000;

			static constexpr unsigned long kReattemptTimeSeconds = 5;

			if ((s_firstNotification.compare("") != 0) && 
				(durationSeconds >= kReattemptTimeSeconds))
			{
				// If so, reattempt the notification.
				MQTTPublishNotification(s_firstNotification);
				TimerGetCurrent(s_lastAttemptTime);

				Logger::WriteFormattedLine("Reattempted first notification.");
			}
		}
	}
}

// Generates and publishes a message to cause the provided text to be spoken.
//
// text:	The text that should be spoken.
//
void MQTTTextToSpeech(std::string const& text)
{
	// Create a properly formatted message that will trigger the text to be spoken.
	static constexpr std::size_t kMessageBufferCapacity{ 500u };
	char messageBuffer[kMessageBufferCapacity];

	snprintf(messageBuffer, kMessageBufferCapacity, 
		"{\"text\": \"%s\", \"siteId\": \"default\", \"lang\": null, \"id\": \"\", "
		"\"sessionId\": \"\", \"volume\": 1.0}", text.c_str());

	// Actually publish to the topic.
	char const* topic = "hermes/tts/say";
	MQTTPublishMessage(topic, messageBuffer);
}

// Causes a spoken notification.
//
// text:	The notification text.
//
void MQTTNotification(std::string const& text)
{
	s_pendingNotificationList.push_back(text);
}

// Get the time that the last text-to-speech finished.
//
// time:	(Output) The last time.
//
void MQTTGetLastTextToSpeechFinishedTime(Time& time)
{
	time = s_lastTextToSpeechFinishedTime;
}
