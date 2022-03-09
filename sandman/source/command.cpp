#include "command.h"

#include "control.h"
#include "input.h"
#include "logger.h"
#include "schedule.h"
#include "sound.h"

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
	"volume",		// TYPE_VOLUME
	// "mute",			// TYPE_MUTE
	// "unmute",		// TYPE_UNMUTE
	"schedule",		// TYPE_SCHEDULE
	"start",			// TYPE_START
	"status",		// TYPE_STATUS
	
	"integer", 		// TYPE_INTEGER
};

// Keep handles to the controls.
static ControlHandle s_BackControlHandle;
static ControlHandle s_LegsControlHandle;
static ControlHandle s_ElevationControlHandle;

// Keep a handle to the input.
static Input const* s_Input = nullptr;

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

// Parse the command tokens into commands.
//
// p_CommandTokens:	All of the potential tokens for the command.
//
void CommandParseTokens(std::vector<CommandToken> const& p_CommandTokens)
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
			}
			break;
			
			case CommandToken::TYPE_STOP:
			{
				// Stop controls.
				ControlsStopAll();			
			}
			break;
			
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
				}
				else if (l_Token.m_Type == CommandToken::TYPE_STOP)
				{
					ScheduleStop();
				}			
			}
			break;
			
			case CommandToken::TYPE_VOLUME:
			{
				// Next token.
				l_TokenIndex++;
				if (l_TokenIndex >= l_TokenCount)
				{
					break;
				}

				l_Token = p_CommandTokens[l_TokenIndex];
				
				if (l_Token.m_Type == CommandToken::TYPE_RAISE)
				{
					SoundIncreaseVolume();
				}
				else if (l_Token.m_Type == CommandToken::TYPE_LOWER)
				{
					SoundDecreaseVolume();
				}
			}
			break;
			
			// case CommandToken::TYPE_MUTE:
			// {
			// 	SoundMute(true);
			// }
			// break;
			
			// case CommandToken::TYPE_UNMUTE:
			// {
			// 	SoundMute(false);
			// }
			// break;
			
			case CommandToken::TYPE_STATUS:
			{
				// Play status speech.
				SoundAddToQueue(DATADIR "audio/running.wav");	
				
				if (ScheduleIsRunning() == true)
				{
					SoundAddToQueue(DATADIR "audio/sched_running.wav");	
				}
				
				if ((s_Input != nullptr) && (s_Input->IsConnected() == true))
				{
					SoundAddToQueue(DATADIR "audio/control_connected.wav");
				}
			}
			break;
			
			default:	
			{
			}
			break;
		}
	}
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

		for (unsigned int l_TokenType = 0; l_TokenType < CommandToken::TYPE_NOT_PARAMETER_COUNT; 
			l_TokenType++)
		{
			// Compare the token string to its name.
			if (l_TokenString.compare(s_CommandTokenNames[l_TokenType]) != 0)
			{
				continue;
			}

			// Found a match.
			l_Token.m_Type = static_cast<CommandToken::Types>(l_TokenType);
			break;
		}

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

