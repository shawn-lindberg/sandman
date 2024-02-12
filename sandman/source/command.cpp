#include "command.h"

#include <unistd.h>
#include <sys/reboot.h>

#include "control.h"
#include "input.h"
#include "logger.h"
#include "notification.h"
#include "reports.h"
#include "schedule.h"

#define DATADIR		AM_DATADIR

// Constants
//


// Locals
//

// Names for each command token.
static char const* const s_CommandTokenNames[] = 
{
	"back",			// TYPE_BACK
	"legs",			// TYPE_LEGS
	"elevation",	// TYPE_ELEVATION
	"raise",			// TYPE_RAISE
	"lower",			// TYPE_LOWER
	"stop",			// TYPE_STOP
	"schedule",		// TYPE_SCHEDULE
	"start",			// TYPE_START
	"status",		// TYPE_STATUS
	"reboot", 		// TYPE_REBOOT
	"yes", 			// TYPE_YES
	"no", 			// TYPE_NO
	
	"integer", 		// TYPE_INTEGER
};

// A mapping between token names and token type.
static const std::map<std::string, CommandToken::Types>	s_CommandTokenNameToTypeMap = 
{
	{ "back", 		CommandToken::TYPE_BACK }, 
	{ "legs",		CommandToken::TYPE_LEGS },
	{ "elevation",	CommandToken::TYPE_ELEVATION },
	{ "raise",		CommandToken::TYPE_RAISE },
	{ "up",			CommandToken::TYPE_RAISE },	// Alternative.
	{ "lower",		CommandToken::TYPE_LOWER },
	{ "down",		CommandToken::TYPE_LOWER },	// Alternative.
	{ "stop",		CommandToken::TYPE_STOP },
	{ "schedule",	CommandToken::TYPE_SCHEDULE },
	{ "start",		CommandToken::TYPE_START },
	{ "status",		CommandToken::TYPE_STATUS },
	{ "reboot", 	CommandToken::TYPE_REBOOT }, 
	{ "yes", 		CommandToken::TYPE_YES }, 
	{ "no", 			CommandToken::TYPE_NO },

	// "integer", 	TYPE_INTEGER
};

// Keep handles to the controls.
static ControlHandle s_BackControlHandle;
static ControlHandle s_LegsControlHandle;
static ControlHandle s_ElevationControlHandle;

// Keep a handle to the input.
static Input const* s_Input = nullptr;

// Signals whether we are in the process of rebooting.
static bool s_Rebooting = false;

// Keep track of when we started the reboot process so we can time the delay.
static Time s_RebootDelayStartTime;

// Functions
//

// Initialize the system.
//
// p_Input:	The input device.
//
void CommandInitialize(Input const& p_Input)
{
	s_Input = &p_Input;
}

// Uninitialize the system.
//
void CommandUninitialize()
{
	s_Input = nullptr;
}

// Process the system.
//
void CommandProcess()
{
	// At the moment we only have per frame processing for rebooting.
	if (s_Rebooting == false)
	{
		return;
	}

	// I don't really want to make a whole function for this, but will call it from multiple code 
	// paths.
	auto l_DoReboot = []()
	{
		s_Rebooting = false;

		LoggerAddMessage("Rebooting!");

		sync();
		reboot(RB_AUTOBOOT);
	};

	// If the notification is done, we can stop waiting.
	Time l_NotificationFinishedTime;
	NotificationGetLastPlayFinishedTime(l_NotificationFinishedTime);

	if (l_NotificationFinishedTime > s_RebootDelayStartTime) 
	{
		l_DoReboot();
		return;
	}

	// Wait for a maximum amount of time regardless.
	Time l_RebootDelayCurrentTime;
	TimerGetCurrent(l_RebootDelayCurrentTime);
	
	float const l_DurationMS = TimerGetElapsedMilliseconds(s_RebootDelayStartTime, 
		l_RebootDelayCurrentTime);
	auto const l_DurationSeconds = static_cast<unsigned long>(l_DurationMS) / 
		1000;
	
	static constexpr unsigned long l_DelayDurationSeconds = 60;
	
	if (l_DurationSeconds >= l_DelayDurationSeconds)
	{
		l_DoReboot();
	}
}

// Parse the command tokens into commands.
//
// p_CommandTokens:	All of the potential tokens for the command.
//
// Returns:	A value signifying the result of the parsing.
//
CommandParseTokensReturnTypes CommandParseTokens(std::vector<CommandToken> const& p_CommandTokens)
{
	char const* l_ConfirmationText = nullptr;
	return CommandParseTokens(l_ConfirmationText, p_CommandTokens);
}

// Parse the command tokens into commands.
//
// p_ConfirmationText:	(Output) In cases with missing confirmation, this is the confirmation 
// 	 						prompt.
// p_CommandTokens:		All of the potential tokens for the command.
//
// Returns:	A value signifying the result of the parsing.
//
CommandParseTokensReturnTypes CommandParseTokens(char const*& p_ConfirmationText, 
	std::vector<CommandToken> const& p_CommandTokens)
{
	// Parse command tokens.
	auto const l_TokenCount = static_cast<unsigned int>(p_CommandTokens.size());
	for (unsigned int l_TokenIndex = 0; l_TokenIndex < l_TokenCount; l_TokenIndex++)
	{
		// Parse commands.
		auto l_Token = p_CommandTokens[l_TokenIndex];
		switch (l_Token.m_Type)
		{
			case CommandToken::TYPE_BACK:			// Fall through...
			case CommandToken::TYPE_LEGS:			// Fall through...
			case CommandToken::TYPE_ELEVATION:	
			{
				// Try to access the control corresponding to the command token.
				Control* l_Control = nullptr;
				
				switch (l_Token.m_Type)
				{
					case CommandToken::TYPE_BACK:
					{
						// Try to find the control.
						if (s_BackControlHandle.IsValid() == false)
						{
							s_BackControlHandle = Control::GetHandle("back");
						}
						
						l_Control = Control::GetFromHandle(s_BackControlHandle);
					}
					break;
					
					case CommandToken::TYPE_LEGS:
					{
						// Try to find the control.
						if (s_LegsControlHandle.IsValid() == false)
						{
							s_LegsControlHandle = Control::GetHandle("legs");
						}
						
						l_Control = Control::GetFromHandle(s_LegsControlHandle);
					}
					break;
					
					case CommandToken::TYPE_ELEVATION:
					{
						// Try to find the control.
						if (s_ElevationControlHandle.IsValid() == false)
						{
							s_ElevationControlHandle = Control::GetHandle("elev");
						}
			
						l_Control = Control::GetFromHandle(s_ElevationControlHandle);
					}
					break;
					
					default:
					{
						LoggerAddMessage("Unrecognized token \"%s\" trying to process a control movement "
							"command.", s_CommandTokenNames[l_Token.m_Type]);
					}
					break;
				}
			
				if (l_Control == nullptr)
				{
					break;
				}			
			
				// Next token.
				l_TokenIndex++;
				if (l_TokenIndex >= l_TokenCount)
				{
					break;
				}
				
				l_Token = p_CommandTokens[l_TokenIndex];
				
				// Try to get the action which should be performed on the control.
				auto l_Action = Control::ACTION_STOPPED;
				
				if (l_Token.m_Type == CommandToken::TYPE_RAISE)
				{
					l_Action = Control::ACTION_MOVING_UP;
				}
				else if (l_Token.m_Type == CommandToken::TYPE_LOWER)
				{
					l_Action = Control::ACTION_MOVING_DOWN;
				}
				
				if (l_Action == Control::ACTION_STOPPED)
				{
					break;
				}
				
				// Determine the duration percent.
				unsigned int l_DurationPercent = 100;
				
				// Peak at the next token.
				auto const l_NextTokenIndex = l_TokenIndex + 1;
				if (l_NextTokenIndex < l_TokenCount)
				{
					auto const l_NextToken = p_CommandTokens[l_NextTokenIndex];
					
					if (l_NextToken.m_Type == CommandToken::TYPE_INTEGER)
					{
						l_DurationPercent = l_NextToken.m_Parameter;
						
						// Actually consume this token.
						l_TokenIndex++;
					}
				}
				
				
				l_Control->SetDesiredAction(l_Action, Control::MODE_TIMED, l_DurationPercent);

				ReportsAddControlItem(l_Control->GetName(), l_Action, "command");
				return CommandParseTokensReturnTypes::SUCCESS;
			}
			
			case CommandToken::TYPE_STOP:
			{
				// Stop controls.
				ControlsStopAll();			

				ReportsAddControlItem("all", Control::ACTION_STOPPED, "command");
				return CommandParseTokensReturnTypes::SUCCESS;
			}
			
			case CommandToken::TYPE_SCHEDULE:
			{
				// Next token.
				l_TokenIndex++;
				if (l_TokenIndex >= l_TokenCount)
				{
					break;
				}
				
				l_Token = p_CommandTokens[l_TokenIndex];
			
				if (l_Token.m_Type == CommandToken::TYPE_START)
				{
					ScheduleStart();
					return CommandParseTokensReturnTypes::SUCCESS;
				}
				else if (l_Token.m_Type == CommandToken::TYPE_STOP)
				{
					ScheduleStop();
					return CommandParseTokensReturnTypes::SUCCESS;
				}			
			}
			break;
			
			case CommandToken::TYPE_STATUS:
			{
				// Play status notification.
				NotificationPlay("running");
				
				if (ScheduleIsRunning() == true)
				{
					NotificationPlay("schedule_running");
				}
				
				if ((s_Input != nullptr) && (s_Input->IsConnected() == true))
				{
					NotificationPlay("control_connected");
				}

				ReportsAddStatusItem();
				return CommandParseTokensReturnTypes::SUCCESS;
			}

			case CommandToken::TYPE_REBOOT:
			{
				// Next token.
				l_TokenIndex++;
				if (l_TokenIndex >= l_TokenCount)
				{
					p_ConfirmationText = "Are you sure you want to reboot?";
					return CommandParseTokensReturnTypes::MISSING_CONFIRMATION;
				}

				l_Token = p_CommandTokens[l_TokenIndex];	

				// Only a positive confirmation is accepted.
				if (l_Token.m_Type != CommandToken::TYPE_YES)
				{
					LoggerAddMessage("Ignoring reboot command because it was not followed by a positive "
						"confirmation.");
					NotificationPlay("canceled");
					break;
				}

				// Kick off the reboot.
				s_Rebooting = true;
				TimerGetCurrent(s_RebootDelayStartTime);

				LoggerAddMessage("Reboot starting!");
				NotificationPlay("restarting");

				return CommandParseTokensReturnTypes::SUCCESS;
			}
			
			default:	
			{
			}
			break;
		}
	}

	return CommandParseTokensReturnTypes::INVALID;
}

// Take a token string and convert it into a token type, if possible.
//
// p_TokenString:	The string to attempt to convert.
// 
// Returns:	The corresponding token type or invalid if one couldn't be found.
// 
static CommandToken::Types CommandConvertStringToTokenType(std::string const& p_TokenString)
{
	// Try to find it in the map.
	auto const l_ResultIterator = s_CommandTokenNameToTypeMap.find(p_TokenString);

	if (l_ResultIterator == s_CommandTokenNameToTypeMap.end()) 
	{		
		// No match.
		return CommandToken::TYPE_INVALID;
	}

	// Found it!
	return l_ResultIterator->second;
}

// Take a command string and turn it into a list of tokens.
//
// p_CommandTokens:	(Output) The resulting command tokens, in order.
// p_CommandString:	The command string to tokenize.
//
void CommandTokenizeString(std::vector<CommandToken>& p_CommandTokens, 
	std::string const& p_CommandString)
{
	// Get the first token string start.
	auto l_NextTokenStringStart = std::string::size_type{0};

	while (l_NextTokenStringStart != std::string::npos)
	{
		// Get the next token string end.
		auto const l_NextTokenStringEnd = p_CommandString.find(' ', l_NextTokenStringStart);

		// Get the token string.
		auto const l_TokenStringLength = (l_NextTokenStringEnd != std::string::npos) ? 
			(l_NextTokenStringEnd - l_NextTokenStringStart) : p_CommandString.size();
		auto l_TokenString = p_CommandString.substr(l_NextTokenStringStart, l_TokenStringLength);

		// Make sure the token string is lowercase.
		for (auto& l_Character : l_TokenString)
		{
			l_Character = std::tolower(l_Character);
		}

		// Match the token string to a token (with no parameter) if possible.
		CommandToken l_Token;
		l_Token.m_Type = CommandConvertStringToTokenType(l_TokenString);

		// If we couldn't turn it into a plain old token, see if it is a parameter token.
		if (l_Token.m_Type == CommandToken::TYPE_INVALID)
		{
			// First, determine whether the string is numeric.
			auto l_IsNumeric = true;

			for (const auto& l_Character : l_TokenString)
			{
				l_IsNumeric = l_IsNumeric && std::isdigit(l_Character);
			}
			
			if (l_IsNumeric == true)
			{
				l_Token.m_Parameter = std::stoul(l_TokenString);
				l_Token.m_Type = CommandToken::TYPE_INTEGER;
			}
		}
		
		// Add the token to the list.
		p_CommandTokens.push_back(l_Token);

		// Get the next token string start (skip delimiter).
		l_NextTokenStringStart = l_NextTokenStringEnd;

		if (l_NextTokenStringEnd != std::string::npos)
		{
			l_NextTokenStringStart++;
		}
	}
}

// Just a simple structure to store a slot name/value pair.
//
struct SlotNameValue
{
	std::string	m_Name;
	std::string	m_Value;
};

// Extract the slots from a JSON document.
//
// p_ExtractedSlots:		(Output) The slot name/value pairs that we found.
// p_CommandDocument:	The command document to get the slots for.
//
static void CommandExtractSlotsFromJSONDocument(std::vector<SlotNameValue>& p_ExtractedSlots, 
	rapidjson::Document const& p_CommandDocument)
{
	auto const l_SlotsIterator = p_CommandDocument.FindMember("slots");

	if (l_SlotsIterator == p_CommandDocument.MemberEnd())
	{
		return;
	}

	auto const& l_Slots = l_SlotsIterator->value;

	if (l_Slots.IsArray() == false)
	{
		return;
	}

	// Now extract each slot individually.
	for (auto const& l_Slot : l_Slots.GetArray())
	{
		if (l_Slot.IsObject() == false) 
		{
			continue;
		}

		// Try to find the slot name.
		auto const l_SlotNameIterator = l_Slot.FindMember("slotName");

		if (l_SlotNameIterator == l_Slot.MemberEnd()) 
		{
			continue;
		}

		auto const& l_SlotName = l_SlotNameIterator->value;

		if (l_SlotName.IsString() == false) 
		{
			continue;
		}

		// Now try to find the value.
		auto const l_SlotValueIterator = l_Slot.FindMember("rawValue");

		if (l_SlotValueIterator == l_Slot.MemberEnd()) 
		{
			continue;
		}

		auto const& l_SlotValue = l_SlotValueIterator->value;

		if (l_SlotValue.IsString() == false) 
		{
			continue;
		}
		
		// Now that we found the name and the value, we can add it to the output.
		SlotNameValue l_ExtractedSlot;
		l_ExtractedSlot.m_Name = l_SlotName.GetString();
		l_ExtractedSlot.m_Value = l_SlotValue.GetString();

		p_ExtractedSlots.push_back(l_ExtractedSlot);
	}
}

// Take a command JSON document and turn it into a list of tokens.
//
// p_CommandTokens:		(Input/Output) The resulting command tokens, in order. If there was a 
// 							command pending confirmation, the corresponding tokens will be passed in.
// p_CommandDocument:	The command document to tokenize.
//
void CommandTokenizeJSONDocument(std::vector<CommandToken>& p_CommandTokens, 
	rapidjson::Document const& p_CommandDocument)
{
	// First we need the intent, then the name of the intent.
	auto const l_IntentIterator = p_CommandDocument.FindMember("intent");

	if (l_IntentIterator == p_CommandDocument.MemberEnd())
	{
		return;
	}

	auto const l_IntentNameIterator = l_IntentIterator->value.FindMember("intentName");

	if (l_IntentNameIterator == p_CommandDocument.MemberEnd())
	{
		return;
	}

	// Now, try to recognize the intent.
	auto const l_IntentName = l_IntentNameIterator->value.GetString();

	// We handle confirmations first so we can short-circuit more easily.
	if (strcmp(l_IntentName, "ConfirmationResponse") == 0)
	{
		// We can ignore this if we are not waiting for confirmation.
		if (p_CommandTokens.empty() == true)
		{
			LoggerAddMessage("Received a confirmation response, but wasn't waiting for confirmation. "
				"Ignoring.");
			return;
		}

		// We need to get the slots so that we can get the necessary parameters.
		std::vector<SlotNameValue> l_Slots;
		CommandExtractSlotsFromJSONDocument(l_Slots, p_CommandDocument);

		// We are looking to fill out one token, the response. 
		CommandToken l_ResponseToken;

		for (auto const& l_Slot : l_Slots)
		{			
			// This is the response slot.
			if (l_Slot.m_Name.compare("response") == 0)
			{
				l_ResponseToken.m_Type = CommandConvertStringToTokenType(l_Slot.m_Value);
			}		
		}	

		if (l_ResponseToken.m_Type == CommandToken::TYPE_INVALID)
		{
			// It's important in this case that we clear the command tokens so that we don't attempt 
			// to process the pending command.
			p_CommandTokens.clear();

			LoggerAddMessage("Couldn't recognize a %s intent because of invalid parameters.", 
				l_IntentName);
			return;
		}

		LoggerAddMessage("Recognized a %s intent.", l_IntentName);

		// Now that we theoretically have a set of valid tokens, add them to the output.
		p_CommandTokens.push_back(l_ResponseToken);
		return;
	}
	else if (p_CommandTokens.empty() == false)
	{
		// If we were waiting on confirmation but got something else instead, ignore it.
		p_CommandTokens.clear();

		LoggerAddMessage("Ignoring intent %s because there was a command pending confirmation.", 
			l_IntentName);
		return;
	}

	if (strcmp(l_IntentName, "GetStatus") == 0)
	{
		LoggerAddMessage("Recognized a %s intent.", l_IntentName);

		// For status, we only have to output the status token.
		CommandToken l_Token;
		l_Token.m_Type = CommandToken::TYPE_STATUS;

		p_CommandTokens.push_back(l_Token);
		return;
	}

	if (strcmp(l_IntentName, "MovePart") == 0)
	{
		// We need to get the slots so that we can get the necessary parameters.
		std::vector<SlotNameValue> l_Slots;
		CommandExtractSlotsFromJSONDocument(l_Slots, p_CommandDocument);

		// We are looking to fill out two tokens, the part and the direction.
		CommandToken l_PartToken;
		CommandToken l_DirectionToken;

		for (auto const& l_Slot : l_Slots)
		{
			// This is the part slot.
			if (l_Slot.m_Name.compare("name") == 0)
			{
				l_PartToken.m_Type = CommandConvertStringToTokenType(l_Slot.m_Value);
				continue;
			}

			// This is the direction slot.
			if (l_Slot.m_Name.compare("direction") == 0)
			{
				l_DirectionToken.m_Type = CommandConvertStringToTokenType(l_Slot.m_Value);
				continue;
			}			
		}	

		if ((l_PartToken.m_Type == CommandToken::TYPE_INVALID) || 
			(l_DirectionToken.m_Type == CommandToken::TYPE_INVALID))
		{
			LoggerAddMessage("Couldn't recognize a %s intent because of invalid parameters.", 
				l_IntentName);
			return;
		}

		LoggerAddMessage("Recognized a %s intent.", l_IntentName);

		// Now that we theoretically have a set of valid tokens, add them to the output.
		p_CommandTokens.push_back(l_PartToken);
		p_CommandTokens.push_back(l_DirectionToken);
		return;
	}

	if (strcmp(l_IntentName, "SetSchedule") == 0)
	{
		// We need to get the slots so that we can get the necessary parameters.
		std::vector<SlotNameValue> l_Slots;
		CommandExtractSlotsFromJSONDocument(l_Slots, p_CommandDocument);

		// We are looking to fill out one token, what to do to the schedule.
		CommandToken l_ScheduleToken;
		l_ScheduleToken.m_Type = CommandToken::TYPE_SCHEDULE;

		CommandToken l_ActionToken;

		for (auto const& l_Slot : l_Slots)
		{
			// This is the action slot.
			if (l_Slot.m_Name.compare("action") == 0)
			{
				l_ActionToken.m_Type = CommandConvertStringToTokenType(l_Slot.m_Value);
				continue;
			}		
		}	

		if (l_ActionToken.m_Type == CommandToken::TYPE_INVALID)
		{
			LoggerAddMessage("Couldn't recognize a %s intent because of invalid parameters.", 
				l_IntentName);
			return;
		}

		LoggerAddMessage("Recognized a %s intent.", l_IntentName);

		// Now that we theoretically have a set of valid tokens, add them to the output.
		p_CommandTokens.push_back(l_ScheduleToken);
		p_CommandTokens.push_back(l_ActionToken);
		return;
	}

	if (strcmp(l_IntentName, "Reboot") == 0)
	{
		LoggerAddMessage("Recognized a %s intent.", l_IntentName);

		// For reboot, we only have to output the reboot token.
		CommandToken l_Token;
		l_Token.m_Type = CommandToken::TYPE_REBOOT;

		p_CommandTokens.push_back(l_Token);
		return;
	}

	LoggerAddMessage("Unrecognized intent named %s.", l_IntentName);
}

