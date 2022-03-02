#pragma once

#include <string>
#include <vector>

// Types
//

// Describes a command token.
struct CommandToken
{
	// Types of command tokens.
	enum Types
	{
		TYPE_INVALID = -1,

		TYPE_BACK,
		TYPE_LEGS,
		TYPE_ELEVATION,
		TYPE_RAISE,
		TYPE_LOWER,
		TYPE_STOP,
		TYPE_VOLUME,
		// TYPE_MUTE,
		// TYPE_UNMUTE,
		TYPE_SCHEDULE,
		TYPE_START,
		TYPE_STATUS,
		
		TYPE_NOT_PARAMETER_COUNT, 
		
		// The following command tokens are parameters.
		TYPE_INTEGER = TYPE_NOT_PARAMETER_COUNT, 
		
		TYPE_COUNT,
	};

	// The type of the token.
	Types	m_Type = TYPE_INVALID;
	
	// The value of the integer parameter, if relevant.
	int   m_Parameter = 0;
};


// Functions
//

// Initialize the system.
//
// p_Input:	The input device.
//
void CommandInitialize(class Input const& p_Input);

// Uninitialize the system.
//
void CommandUninitialize();

// Parse the command tokens into commands.
//
// p_CommandTokens:	All of the potential tokens for the command.
//
void CommandParseTokens(std::vector<CommandToken> const& p_CommandTokens);

// Take a command string and turn it into a list of tokens.
//
// p_CommandTokens:	(Output) The resulting command tokens, in order.
// p_CommandString:	The command string to tokenize.
//
void CommandTokenizeString(std::vector<CommandToken>& p_CommandTokens, 
	std::string const& p_CommandString);