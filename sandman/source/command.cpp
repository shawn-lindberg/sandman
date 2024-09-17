#include "command.h"

#include <unistd.h>
#include <sys/reboot.h>
#include <charconv>

#include "control.h"
#include "input.h"
#include "logger.h"
#include "notification.h"
#include "reports.h"
#include "schedule.h"

// Constants
//


// Locals
//

// Names for each command token.
static constexpr char const* const kCommandTokenNames[] = 
{
	"back",			// kTypeBack
	"legs",			// kTypeLegs
	"elevation",	// kTypeElevation
	"raise",			// kTypeRaise
	"lower",			// kTypeLower
	"stop",			// kTypeStop
	"schedule",		// kTypeSchedule
	"start",			// kTypeStart
	"status",		// kTypeStatus
	"reboot", 		// kTypeReboot
	"yes", 			// kTypeYes
	"no", 			// kTypeNo
	
	"integer", 		// kTypeInteger
};

// A mapping between token names and token type.
static std::map<std::string, CommandToken::Types> const s_commandTokenNameToTypeMap = 
{
	{ "back", 		CommandToken::kTypeBack }, 
	{ "legs",		CommandToken::kTypeLegs },
	{ "elevation",	CommandToken::kTypeElevation },
	{ "raise",		CommandToken::kTypeRaise },
	{ "up",			CommandToken::kTypeRaise },	// Alternative.
	{ "lower",		CommandToken::kTypeLower },
	{ "down",		CommandToken::kTypeLower },	// Alternative.
	{ "stop",		CommandToken::kTypeStop },
	{ "schedule",	CommandToken::kTypeSchedule },
	{ "start",		CommandToken::kTypeStart },
	{ "status",		CommandToken::kTypeStatus },
	{ "reboot", 	CommandToken::kTypeReboot }, 
	{ "yes", 		CommandToken::kTypeYes }, 
	{ "no", 			CommandToken::kTypeNo },

	// "integer", 	kTypeInteger
};

// Keep handles to the controls.
static ControlHandle s_backControlHandle;
static ControlHandle s_legsControlHandle;
static ControlHandle s_elevationControlHandle;

// Keep a handle to the input.
static Input const* s_input = nullptr;

// Signals whether we are in the process of rebooting.
static bool s_rebooting = false;

// Keep track of when we started the reboot process so we can time the delay.
static Time s_rebootDelayStartTime;

// Functions
//

// Initialize the system.
//
// input:	The input device.
//
void CommandInitialize(Input const& input)
{
	s_input = &input;
}

// Uninitialize the system.
//
void CommandUninitialize()
{
	s_input = nullptr;
}

// Process the system.
//
void CommandProcess()
{
	// At the moment we only have per frame processing for rebooting.
	if (s_rebooting == false)
	{
		return;
	}

	// I don't really want to make a whole function for this, but will call it from multiple code 
	// paths.
	static constexpr auto DoReboot = []() -> void
	{
		s_rebooting = false;

		Logger::WriteLine("Rebooting!");

		sync();
		reboot(RB_AUTOBOOT);
	};

	// If the notification is done, we can stop waiting.
	Time notificationFinishedTime;
	NotificationGetLastPlayFinishedTime(notificationFinishedTime);

	if (notificationFinishedTime > s_rebootDelayStartTime) 
	{
		DoReboot();
		return;
	}

	// Wait for a maximum amount of time regardless.
	Time rebootDelayCurrentTime;
	TimerGetCurrent(rebootDelayCurrentTime);
	
	float const durationMS = TimerGetElapsedMilliseconds(s_rebootDelayStartTime, 
																		  rebootDelayCurrentTime);

	auto const durationSeconds{static_cast<unsigned long>(durationMS) / 1'000};

	static constexpr unsigned long kDelayDurationSeconds{60u};

	if (durationSeconds >= kDelayDurationSeconds)
	{
		DoReboot();
	}
}

// Parse the command tokens into commands.
//
// commandTokens:	All of the potential tokens for the command.
//
// Returns:	A value signifying the result of the parsing.
//
CommandParseTokensReturnTypes CommandParseTokens(std::vector<CommandToken> const& commandTokens)
{
	char const* confirmationText = nullptr;
	return CommandParseTokens(confirmationText, commandTokens);
}

// Parse the command tokens into commands.
//
// confirmationText:	(Output) In cases with missing confirmation, this is the confirmation 
// 	 						prompt.
// commandTokens:		All of the potential tokens for the command.
//
// Returns:	A value signifying the result of the parsing.
//
CommandParseTokensReturnTypes CommandParseTokens(char const*& confirmationText, 
	std::vector<CommandToken> const& commandTokens)
{
	// Parse command tokens.
	auto const tokenCount = static_cast<unsigned int>(commandTokens.size());
	for (unsigned int tokenIndex = 0; tokenIndex < tokenCount; tokenIndex++)
	{
		// Parse commands.
		auto token = commandTokens[tokenIndex];
		switch (token.m_type)
		{
			case CommandToken::kTypeBack:			// Fall through...
			case CommandToken::kTypeLegs:			// Fall through...
			case CommandToken::kTypeElevation:	
			{
				// Try to access the control corresponding to the command token.
				Control* control = nullptr;
				
				switch (token.m_type)
				{
					case CommandToken::kTypeBack:
					{
						// Try to find the control.
						if (s_backControlHandle.IsValid() == false)
						{
							s_backControlHandle = Control::GetHandle("back");
						}
						
						control = Control::GetFromHandle(s_backControlHandle);
					}
					break;
					
					case CommandToken::kTypeLegs:
					{
						// Try to find the control.
						if (s_legsControlHandle.IsValid() == false)
						{
							s_legsControlHandle = Control::GetHandle("legs");
						}
						
						control = Control::GetFromHandle(s_legsControlHandle);
					}
					break;
					
					case CommandToken::kTypeElevation:
					{
						// Try to find the control.
						if (s_elevationControlHandle.IsValid() == false)
						{
							s_elevationControlHandle = Control::GetHandle("elev");
						}
			
						control = Control::GetFromHandle(s_elevationControlHandle);
					}
					break;
					
					default:
					{
						Logger::WriteFormattedLine("Unrecognized token \"%s\" trying to process a control movement "
							"command.", kCommandTokenNames[token.m_type]);
					}
					break;
				}
			
				if (control == nullptr)
				{
					break;
				}			
			
				// Next token.
				tokenIndex++;
				if (tokenIndex >= tokenCount)
				{
					break;
				}
				
				token = commandTokens[tokenIndex];
				
				// Try to get the action which should be performed on the control.
				auto action = Control::kActionStopped;
				
				if (token.m_type == CommandToken::kTypeRaise)
				{
					action = Control::kActionMovingUp;
				}
				else if (token.m_type == CommandToken::kTypeLower)
				{
					action = Control::kActionMovingDown;
				}
				
				if (action == Control::kActionStopped)
				{
					break;
				}
				
				// Determine the duration percent.
				unsigned int durationPercent = 100;
				
				// Peak at the next token.
				auto const nextTokenIndex = tokenIndex + 1;
				if (nextTokenIndex < tokenCount)
				{
					auto const nextToken = commandTokens[nextTokenIndex];
					
					if (nextToken.m_type == CommandToken::kTypeInteger)
					{
						durationPercent = nextToken.m_parameter;
						
						// Actually consume this token.
						tokenIndex++;
					}
				}
				
				
				control->SetDesiredAction(action, Control::kModeTimed, durationPercent);

				ReportsAddControlItem(control->GetName(), action, "command");
				return CommandParseTokensReturnTypes::kSuccess;
			}
			
			case CommandToken::kTypeStop:
			{
				// Stop controls.
				ControlsStopAll();			

				ReportsAddControlItem("all", Control::kActionStopped, "command");
				return CommandParseTokensReturnTypes::kSuccess;
			}
			
			case CommandToken::kTypeSchedule:
			{
				// Next token.
				tokenIndex++;
				if (tokenIndex >= tokenCount)
				{
					break;
				}
				
				token = commandTokens[tokenIndex];
			
				if (token.m_type == CommandToken::kTypeStart)
				{
					ScheduleStart();
					return CommandParseTokensReturnTypes::kSuccess;
				}
				else if (token.m_type == CommandToken::kTypeStop)
				{
					ScheduleStop();
					return CommandParseTokensReturnTypes::kSuccess;
				}			
			}
			break;
			
			case CommandToken::kTypeStatus:
			{
				// Play status notification.
				NotificationPlay("running");
				
				if (ScheduleIsRunning() == true)
				{
					NotificationPlay("schedule_running");
				}
				
				if ((s_input != nullptr) && (s_input->IsConnected() == true))
				{
					NotificationPlay("control_connected");
				}

				ReportsAddStatusItem();
				return CommandParseTokensReturnTypes::kSuccess;
			}

			case CommandToken::kTypeReboot:
			{
				// Next token.
				tokenIndex++;
				if (tokenIndex >= tokenCount)
				{
					confirmationText = "Are you sure you want to reboot?";
					return CommandParseTokensReturnTypes::kMissingConfirmation;
				}

				token = commandTokens[tokenIndex];

				// Only a positive confirmation is accepted.
				if (token.m_type != CommandToken::kTypeYes)
				{
					Logger::WriteFormattedLine("Ignoring reboot command because it was not followed by "
														"a positive confirmation.");
					NotificationPlay("canceled");
					break;
				}

				// Kick off the reboot.
				s_rebooting = true;
				TimerGetCurrent(s_rebootDelayStartTime);

				Logger::WriteFormattedLine("Reboot starting!");
				NotificationPlay("restarting");

				return CommandParseTokensReturnTypes::kSuccess;
			}
			
			default:	
			{
			}
			break;
		}
	}

	return CommandParseTokensReturnTypes::kInvalid;
}

// Take a token string and convert it into a token type, if possible.
//
// tokenString:	The string to attempt to convert.
// 
// Returns:	The corresponding token type or invalid if one couldn't be found.
// 
static CommandToken::Types CommandConvertStringToTokenType(std::string const& tokenString)
{
	// Try to find it in the map.
	auto const resultIterator = s_commandTokenNameToTypeMap.find(tokenString);

	if (resultIterator == s_commandTokenNameToTypeMap.end()) 
	{		
		// No match.
		return CommandToken::kTypeInvalid;
	}

	// Found it!
	return resultIterator->second;
}

// Take a command string and turn it into a list of tokens.
//
// commandTokens:	(Output) The resulting command tokens, in order.
// commandString:	The command string to tokenize.
//
void CommandTokenizeString(std::vector<CommandToken>& commandTokens, 
	std::string const& commandString)
{
	// Get the first token string start.
	auto nextTokenStringStart = std::string::size_type{0};

	while (nextTokenStringStart != std::string::npos)
	{
		// Get the next token string end.
		auto const nextTokenStringEnd = commandString.find(' ', nextTokenStringStart);

		// Get the token string.
		auto const tokenStringLength = (nextTokenStringEnd != std::string::npos) ? 
			(nextTokenStringEnd - nextTokenStringStart) : commandString.size();
		auto tokenString = commandString.substr(nextTokenStringStart, tokenStringLength);

		// Make sure the token string is lowercase.
		for (auto& character : tokenString)
		{
			character = std::tolower(character, std::locale::classic());
		}

		// Match the token string to a token (with no parameter) if possible.
		CommandToken token;
		token.m_type = CommandConvertStringToTokenType(tokenString);

		// If we couldn't turn it into a plain old token, see if it is a parameter token.
		if (token.m_type == CommandToken::kTypeInvalid)
		{
			std::string_view const tokenStringView(tokenString);

			// Attempt to parse the string into a number; save result into `token.m_parameter`.
			auto const [endPointer, errorCode] = std::from_chars(tokenStringView.begin(),
																				  tokenStringView.end(),
																				  token.m_parameter);

			// Check if successfully parsed to number and matched whole string.
			if (errorCode == std::errc() and endPointer == tokenStringView.end())
			{
				token.m_type = CommandToken::kTypeInteger;
			}

		}

		// Add the token to the list.
		commandTokens.push_back(token);

		// Get the next token string start (skip delimiter).
		nextTokenStringStart = nextTokenStringEnd;

		if (nextTokenStringEnd != std::string::npos)
		{
			nextTokenStringStart++;
		}
	}
}

// Just a simple structure to store a slot name/value pair.
//
struct SlotNameValue
{
	std::string	m_name;
	std::string	m_value;
};

// Extract the slots from a JSON document.
//
// extractedSlots:		(Output) The slot name/value pairs that we found.
// commandDocument:	The command document to get the slots for.
//
static void CommandExtractSlotsFromJSONDocument(std::vector<SlotNameValue>& extractedSlots, 
	rapidjson::Document const& commandDocument)
{
	auto const slotsIterator = commandDocument.FindMember("slots");

	if (slotsIterator == commandDocument.MemberEnd())
	{
		return;
	}

	auto const& slots = slotsIterator->value;

	if (slots.IsArray() == false)
	{
		return;
	}

	// Now extract each slot individually.
	for (auto const& slot : slots.GetArray())
	{
		if (slot.IsObject() == false) 
		{
			continue;
		}

		// Try to find the slot name.
		auto const slotNameIterator = slot.FindMember("slotName");

		if (slotNameIterator == slot.MemberEnd()) 
		{
			continue;
		}

		auto const& slotName = slotNameIterator->value;

		if (slotName.IsString() == false) 
		{
			continue;
		}

		// Now try to find the value.
		auto const slotValueIterator = slot.FindMember("rawValue");

		if (slotValueIterator == slot.MemberEnd()) 
		{
			continue;
		}

		auto const& slotValue = slotValueIterator->value;

		if (slotValue.IsString() == false) 
		{
			continue;
		}
		
		// Now that we found the name and the value, we can add it to the output.
		SlotNameValue extractedSlot;
		extractedSlot.m_name = slotName.GetString();
		extractedSlot.m_value = slotValue.GetString();

		extractedSlots.push_back(extractedSlot);
	}
}

// Take a command JSON document and turn it into a list of tokens.
//
// commandTokens:		(Input/Output) The resulting command tokens, in order. If there was a 
// 							command pending confirmation, the corresponding tokens will be passed in.
// commandDocument:	The command document to tokenize.
//
void CommandTokenizeJSONDocument(std::vector<CommandToken>& commandTokens, 
	rapidjson::Document const& commandDocument)
{
	// First we need the intent, then the name of the intent.
	auto const intentIterator = commandDocument.FindMember("intent");

	if (intentIterator == commandDocument.MemberEnd())
	{
		return;
	}

	auto const intentNameIterator = intentIterator->value.FindMember("intentName");

	if (intentNameIterator == commandDocument.MemberEnd())
	{
		return;
	}

	// Now, try to recognize the intent.
	auto const intentName = intentNameIterator->value.GetString();

	// We handle confirmations first so we can short-circuit more easily.
	if (strcmp(intentName, "ConfirmationResponse") == 0)
	{
		// We can ignore this if we are not waiting for confirmation.
		if (commandTokens.empty() == true)
		{
			Logger::WriteFormattedLine("Received a confirmation response, but wasn't waiting for confirmation. "
				"Ignoring.");
			return;
		}

		// We need to get the slots so that we can get the necessary parameters.
		std::vector<SlotNameValue> slots;
		CommandExtractSlotsFromJSONDocument(slots, commandDocument);

		// We are looking to fill out one token, the response. 
		CommandToken responseToken;

		for (auto const& slot : slots)
		{			
			// This is the response slot.
			if (slot.m_name.compare("response") == 0)
			{
				responseToken.m_type = CommandConvertStringToTokenType(slot.m_value);
			}		
		}	

		if (responseToken.m_type == CommandToken::kTypeInvalid)
		{
			// It's important in this case that we clear the command tokens so that we don't attempt 
			// to process the pending command.
			commandTokens.clear();

			Logger::WriteFormattedLine("Couldn't recognize a %s intent because of invalid parameters.", 
				intentName);
			return;
		}

		Logger::WriteFormattedLine("Recognized a %s intent.", intentName);

		// Now that we theoretically have a set of valid tokens, add them to the output.
		commandTokens.push_back(responseToken);
		return;
	}
	else if (commandTokens.empty() == false)
	{
		// If we were waiting on confirmation but got something else instead, ignore it.
		commandTokens.clear();

		Logger::WriteFormattedLine("Ignoring intent %s because there was a command pending confirmation.", 
			intentName);
		return;
	}

	if (strcmp(intentName, "GetStatus") == 0)
	{
		Logger::WriteFormattedLine("Recognized a %s intent.", intentName);

		// For status, we only have to output the status token.
		CommandToken token;
		token.m_type = CommandToken::kTypeStatus;

		commandTokens.push_back(token);
		return;
	}

	if (strcmp(intentName, "MovePart") == 0)
	{
		// We need to get the slots so that we can get the necessary parameters.
		std::vector<SlotNameValue> slots;
		CommandExtractSlotsFromJSONDocument(slots, commandDocument);

		// We are looking to fill out two tokens, the part and the direction.
		CommandToken partToken;
		CommandToken directionToken;

		for (auto const& slot : slots)
		{
			// This is the part slot.
			if (slot.m_name.compare("name") == 0)
			{
				partToken.m_type = CommandConvertStringToTokenType(slot.m_value);
				continue;
			}

			// This is the direction slot.
			if (slot.m_name.compare("direction") == 0)
			{
				directionToken.m_type = CommandConvertStringToTokenType(slot.m_value);
				continue;
			}			
		}	

		if ((partToken.m_type == CommandToken::kTypeInvalid) || 
			(directionToken.m_type == CommandToken::kTypeInvalid))
		{
			Logger::WriteFormattedLine("Couldn't recognize a %s intent because of invalid parameters.", 
				intentName);
			return;
		}

		Logger::WriteFormattedLine("Recognized a %s intent.", intentName);

		// Now that we theoretically have a set of valid tokens, add them to the output.
		commandTokens.push_back(partToken);
		commandTokens.push_back(directionToken);
		return;
	}

	if (strcmp(intentName, "SetSchedule") == 0)
	{
		// We need to get the slots so that we can get the necessary parameters.
		std::vector<SlotNameValue> slots;
		CommandExtractSlotsFromJSONDocument(slots, commandDocument);

		// We are looking to fill out one token, what to do to the schedule.
		CommandToken scheduleToken;
		scheduleToken.m_type = CommandToken::kTypeSchedule;

		CommandToken actionToken;

		for (auto const& slot : slots)
		{
			// This is the action slot.
			if (slot.m_name.compare("action") == 0)
			{
				actionToken.m_type = CommandConvertStringToTokenType(slot.m_value);
				continue;
			}		
		}	

		if (actionToken.m_type == CommandToken::kTypeInvalid)
		{
			Logger::WriteFormattedLine("Couldn't recognize a %s intent because of invalid parameters.", 
				intentName);
			return;
		}

		Logger::WriteFormattedLine("Recognized a %s intent.", intentName);

		// Now that we theoretically have a set of valid tokens, add them to the output.
		commandTokens.push_back(scheduleToken);
		commandTokens.push_back(actionToken);
		return;
	}

	if (strcmp(intentName, "Reboot") == 0)
	{
		Logger::WriteFormattedLine("Recognized a %s intent.", intentName);

		// For reboot, we only have to output the reboot token.
		CommandToken token;
		token.m_type = CommandToken::kTypeReboot;

		commandTokens.push_back(token);
		return;
	}

	Logger::WriteFormattedLine("Unrecognized intent named %s.", intentName);
}

