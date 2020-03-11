#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "logger.h"
#include "xml.h"

// Constants
//


// Functions
//

// Config members

Config::Config()
{
	m_SpeechInputDeviceName[0] = '\0';
	m_InputDeviceName[0] = '\0';
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
			// See if this is an input device.
			static auto const* s_InputDeviceNodeName = "InputDevice";
			if (XMLIsNodeNamed(l_SettingNode, s_InputDeviceNodeName) == false)
			{				
				continue;
			}
			
			// Let's go through the input device settings and try to load those in.
			auto l_InputDeviceNode = l_SettingNode->xmlChildrenNode;
			for (; l_InputDeviceNode != nullptr; l_InputDeviceNode = l_InputDeviceNode->next)
			{
				// See if this is the device name.
				static auto const* s_DeviceNameNodeName = "DeviceName";
				if (XMLIsNodeNamed(l_InputDeviceNode, s_DeviceNameNodeName) == true)
				{			
					// Load the text from the node.
					XMLCopyNodeText(m_InputDeviceName, ms_InputDeviceNameCapacity, l_ConfigDocument, 
						l_InputDeviceNode);
					continue;
				}
				
				// See if this is a set of input bindings.
				static auto const* s_BindingsNodeName = "Bindings";
				if (XMLIsNodeNamed(l_InputDeviceNode, s_BindingsNodeName) == true)
				{
					// Clear the list so that if there are multiple sets of bindings, we always take the 
					// last ones.
					m_InputBindings.clear();
		
					// Let's go through the bindings and try to load those in.
					static auto const* s_BindingNodeName = "Binding";
					XMLForEachNodeNamed(l_InputDeviceNode->xmlChildrenNode, s_BindingNodeName, 
						[&](xmlNodePtr p_Node)
					{						
						// Try to read the binding.
						InputBinding l_Binding;
						if (l_Binding.ReadFromXML(l_ConfigDocument, p_Node) == false)
						{
							return;
						}
						
						// If we successfully read a binding, add it to the list.
						m_InputBindings.push_back(l_Binding);
					});				
	
					continue;
				}
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
			
			// See if this is a set of control configs.
			static auto const* s_ControlConfigsNodeName = "ControlConfigs";
			if (XMLIsNodeNamed(l_SettingNode, s_ControlConfigsNodeName) == true)
			{
				// Clear the list so that if there are multiple sets, we always take the last ones.
				m_ControlConfigs.clear();
		
				static auto const* s_ControlConfigNodeName = "ControlConfig";
				XMLForEachNodeNamed(l_SettingNode->xmlChildrenNode, s_ControlConfigNodeName, 
					[&](xmlNodePtr p_Node)
				{								
					// Try to read the control config.
					ControlConfig l_ControlConfig;
					if (l_ControlConfig.ReadFromXML(l_ConfigDocument, p_Node) == false)
					{
						return;
					}
					
					// If we successfully read a control config, add it to the list.
					m_ControlConfigs.push_back(l_ControlConfig);
				});		

				continue;
			}
		}
	}
	
	// "Close" the config file.
	xmlFreeDoc(l_ConfigDocument);
	
	return true;
}

