#pragma once

#include <string>
#include <vector>
#include <cstddef>

#include "rapidjson/document.h"

// Types
//

// Describes a command token.
struct CommandToken
{
	// Types of command tokens.
	enum Types
	{
		kTypeInvalid = -1,

		kTypeBack,
		kTypeLegs,
		kTypeElevation,
		kTypeRaise,
		kTypeLower,
		kTypeStop,
		kTypeSchedule,
		kTypeStart,
		kTypeStatus,
		kTypeReboot, 
		kTypeYes, 
		kTypeNo, 
		
		kTypeNotParameterCount, 
		
		// The following command tokens are parameters.
		kTypeInteger = kTypeNotParameterCount, 
		
		kTypeCount,
	};

	// The type of the token.
	Types m_Type = kTypeInvalid;

	// The value of the integer parameter, if relevant.
	unsigned int m_Parameter = 0u;
};

// Potential return values from parsing tokens.
enum class CommandParseTokensReturnTypes
{
	Invalid = 0, 
	Success, 
	MissingConfirmation, 
};

// Functions
//

// Initialize the system.
//
// input:	The input device.
//
void CommandInitialize(class Input const& input);

// Uninitialize the system.
//
void CommandUninitialize();

// Process the system.
//
void CommandProcess();

// Parse the command tokens into commands.
//
// commandTokens:	All of the potential tokens for the command.
//
// Returns:	A value signifying the result of the parsing.
//
CommandParseTokensReturnTypes CommandParseTokens(std::vector<CommandToken> const& commandTokens);

// Parse the command tokens into commands.
//
// confirmationText:	(Output) In cases with missing confirmation, this is the confirmation 
// 	 						prompt.
// commandTokens:		All of the potential tokens for the command.
//
// Returns:	A value signifying the result of the parsing.
//
CommandParseTokensReturnTypes CommandParseTokens(char const*& confirmationText, 
	std::vector<CommandToken> const& commandTokens);

// Take a command string and turn it into a list of tokens.
//
// commandTokens:	(Output) The resulting command tokens, in order.
// commandString:	The command string to tokenize.
//
void CommandTokenizeString(std::vector<CommandToken>& commandTokens, 
	std::string const& commandString);

// Take a command JSON document and turn it into a list of tokens.
//
// commandTokens:		(Input/Output) The resulting command tokens, in order. If there was a 
// 							command pending confirmation, the corresponding tokens will be passed in.
// commandDocument:	The command document to tokenize.
//
void CommandTokenizeJSONDocument(std::vector<CommandToken>& commandTokens, 
	rapidjson::Document const& commandDocument);