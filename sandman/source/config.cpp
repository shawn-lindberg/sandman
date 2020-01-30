#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

// Constants
//


// Functions
//

template<class T>
T const& Min(T const& p_A, T const& p_B)
{
	return (p_A < p_B) ? p_A : p_B;
}

// Determines whether the name of a node matches a string. I don't like this, but for some reason 
// the developers of the XML library decided to make this other type for the strings, but the "official" 
// examples do blind casts anyway. So I'm hiding this detail in this function.
//
// p_Node:	The node whose name we are going to check.
// p_Name:	The name we are checking for.
//
// Returns:	True if the names match, false otherwise.
//
static bool XMLIsNodeNamed(const xmlNodePtr p_Node, char const* p_Name)
{
	auto const* l_XMLName = reinterpret_cast<xmlChar const*>(p_Name);
	if (xmlStrcmp(p_Node->name, l_XMLName) == 0)
	{
		return true;
	}
	
	return false;
}

// Gets an integer from the text of an XML node.
// 
// p_Document:	The XML document that the node belongs to.
// p_Node:		The XML node to get the the integer from.
//
static int XMLGetNodeTextAsInteger(xmlDocPtr p_Document, xmlNodePtr p_Node)
{
	// Get the string representation of the note text.
	auto* l_NodeText = xmlNodeListGetString(p_Document, p_Node->xmlChildrenNode, 1);
	
	if (l_NodeText == nullptr) {
		return -1;
	}
	
	// Convert the string to an integer.
	auto const l_Value = atoi(reinterpret_cast<char const*>(l_NodeText));
	
	// Free the memory that was allocated and return the value.
	xmlFree(l_NodeText);
	
	return l_Value;
}

// Copy the text of an XML node into a buffer.
// 
// p_Buffer:			(output) A buffer that the text can be copied into.
// p_BufferCapacity:	The number of characters that can be stored in the buffer.
// p_Document:			The XML document that the node belongs to.
// p_Node:				The XML node to get the the integer from.
//
// Returns:		The number of characters copied into the buffer.
//
static int XMLCopyNodeText(char* p_Buffer, unsigned int p_BufferCapacity, xmlDocPtr p_Document, 
	xmlNodePtr p_Node)
{
	// Get the string representation of the note text.
	auto* l_NodeText = xmlNodeListGetString(p_Document, p_Node->xmlChildrenNode, 1);
	
	if (l_NodeText == nullptr) {
		return -1;
	}
	
	// Copy no more than the amount of text the buffer can hold.
	auto const* l_NodeTextString = reinterpret_cast<char const*>(l_NodeText);
	unsigned int const l_AmountToCopy = 
		Min(static_cast<unsigned int>(p_BufferCapacity) - 1, strlen(l_NodeTextString));
	strncpy(p_Buffer, l_NodeTextString, l_AmountToCopy);
	p_Buffer[l_AmountToCopy] = '\0';
	
	// Free the memory that was allocated and return the amount copied.
	xmlFree(l_NodeText);
	
	return l_AmountToCopy;
}

// Searches a list of XML nodes starting from a given node and returns the next node with a matching 
// name.
//
// p_StartNode:	Does node to start with. This can any node among the same list. Pass in the 
// 					previously found node to get the next one.
// p_SearchName:	The name of the node to search for.
//
// Returns: 		The next node, or null if none was found.
//
static xmlNodePtr XMLFindNextNodeByName(const xmlNodePtr p_StartNode, char const* p_SearchName)
{
	// Search through all of the nodes at this level.
	for (auto l_Node = p_StartNode; l_Node != nullptr; l_Node = l_Node->next)
	{
		// If the name doesn't match, go to the next node.
		if (XMLIsNodeNamed(l_Node, p_SearchName) == false)
		{
			continue;
		}
		
		// Found it, return it.
		return l_Node;
	}
	
	return nullptr;
}

// Config members

Config::Config()
{
	m_SpeechInputDeviceName[0] = '\0';
	m_InputSampleRate = 0;
	m_PostSpeechDelaySec = 1.0f;
	m_InputDeviceName[0] = '\0';
	m_ControlMaxMovingDurationMS = 100000;
	m_ControlCoolDownDurationMS = 50000;
}

// Read the configuration from a file.
// 
// p_ConfigFileName:	The name of the config file.
//
// Returns:				True if successful, false otherwise.
//
bool Config::ReadFromFile(char const* p_ConfigFileName)
{
	if (p_ConfigFileName == nullptr)
	{
		return false;
	}
	
	// Open the config file.
	auto* l_ConfigDocument = xmlParseFile(p_ConfigFileName);
	
	if (l_ConfigDocument == nullptr) {		
		
		printf("Failed to open the config file.\n");
		return false;
	}
	
	// Get the root element of the tree.
	auto const* l_RootNode = xmlDocGetRootElement(l_ConfigDocument);
	
	if (l_RootNode == nullptr) {
		
		xmlFreeDoc(l_ConfigDocument);
		printf("Failed to get the root node of the config file. Maybe it is corrupt?\n");
		return false;
	}
	
	// Try to find the input settings node.
	static auto const* s_InputSettingsNodeName = "InputSettings";
	auto const* l_InputSettingsNode = XMLFindNextNodeByName(l_RootNode->xmlChildrenNode, 
		s_InputSettingsNodeName);
		
	if (l_InputSettingsNode != nullptr)
	{
		// Let's go through the input settings and look for ones we recognize.
		auto l_SettingNode = l_InputSettingsNode->xmlChildrenNode;
		for (; l_SettingNode != nullptr; l_SettingNode = l_SettingNode->next)
		{
			// See if this is the input device name.
			static auto const* s_InputDeviceNodeName = "InputDevice";
			if (XMLIsNodeNamed(l_SettingNode, s_InputDeviceNodeName) == true)
			{
				// Load the text from the node.
				XMLCopyNodeText(m_InputDeviceName, INPUT_DEVICE_NAME_CAPACITY, l_ConfigDocument, 
					l_SettingNode);
				
				continue;
			}
		}
	}
	
	// Try to find the control settings node.
	static auto const* s_ControlSettingsNodeName = "ControlSettings";
	auto const* l_ControlSettingsNode = XMLFindNextNodeByName(l_RootNode->xmlChildrenNode, 
		s_ControlSettingsNodeName);
	
	if (l_ControlSettingsNode != nullptr) {
		
		// Let's go through the control settings and look for ones we recognize.
		auto l_SettingNode = l_ControlSettingsNode->xmlChildrenNode;
		for (; l_SettingNode != nullptr; l_SettingNode = l_SettingNode->next)
		{
			// See if this is the maximum moving duration.
			static auto const* s_MaxMovingDurationNodeName = "MaxMovingDurationMS";
			if (XMLIsNodeNamed(l_SettingNode, s_MaxMovingDurationNodeName) == true)
			{
				// Load the value from the node.
				auto const l_MovingDuration = XMLGetNodeTextAsInteger(l_ConfigDocument, l_SettingNode);
				m_ControlMaxMovingDurationMS = l_MovingDuration;
				
				continue;
			}
			
			// See if this is the cooldown duration.
			static auto const* s_CoolDownDurationNodeName = "CoolDownDurationMS";
			if (XMLIsNodeNamed(l_SettingNode, s_CoolDownDurationNodeName) == true)
			{
				// Load the value from the node.
				auto const l_CoolDownDuration = XMLGetNodeTextAsInteger(l_ConfigDocument, l_SettingNode);
				m_ControlCoolDownDurationMS = l_CoolDownDuration;
				
				continue;
			}
		}
	}
	
	// "Close" the config file.
	xmlFreeDoc(l_ConfigDocument);
	
	return true;
}

