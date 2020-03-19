#pragma once

#include <libxml/parser.h>

// Types
//

// Functions
//

// Determines whether the name of a node matches a string. I don't like this, but for some reason 
// the developers of the XML library decided to make this other type for the strings, but the "official" 
// examples do blind casts anyway. So I'm hiding this detail in this function.
//
// p_Node:	The node whose name we are going to check.
// p_Name:	The name we are checking for.
//
// Returns:	True if the names match, false otherwise.
//
bool XMLIsNodeNamed(const xmlNodePtr p_Node, char const* p_Name);

// Gets an integer from the text of an XML node.
// 
// p_Document:	The XML document that the node belongs to.
// p_Node:		The XML node to get the integer from.
//
int XMLGetNodeTextAsInteger(xmlDocPtr p_Document, xmlNodePtr p_Node);

// Copy the text of an XML node into a buffer.
// 
// p_Buffer:			(output) A buffer that the text can be copied into.
// p_BufferCapacity:	The number of characters that can be stored in the buffer.
// p_Document:			The XML document that the node belongs to.
// p_Node:				The XML node to get the text from.
//
// Returns:		True if the text was copied, false otherwise.
//
bool XMLCopyNodeText(char* p_Buffer, unsigned int p_BufferCapacity, xmlDocPtr p_Document, 
	xmlNodePtr p_Node);

// Searches a list of XML nodes starting from a given node and returns the next node with a matching 
// name.
//
// p_StartNode:	The node to start with. This can any node among the same list. Pass in the 
// 					previously found node to get the next one.
// p_SearchName:	The name of the node to search for.
//
// Returns: 		The next node, or null if none was found.
//
xmlNodePtr XMLFindNextNodeByName(const xmlNodePtr p_StartNode, char const* p_SearchName);

// Allows easy iteration of all of the nodes with a given name at a certain level of the tree.
//
// p_StartNode:	The node to start with. This can any node among the same list, but it probably makes 
//						sense for this to be the first node.
// p_SearchName:	The name of the nodes to visit.
// p_Visitor:		The visitor function that will be called on matching nodes.
//
template <typename VisitorType>
void XMLForEachNodeNamed(const xmlNodePtr p_StartNode, char const* p_SearchName, VisitorType p_Visitor);


#include "xml.inl"
