
// Allows easy iteration of all of the nodes with a given name at a certain level of the tree.
//
// p_StartNode:	The node to start with. This can any node among the same list, but it probably makes 
//						sense for this to be the first node.
// p_SearchName:	The name of the nodes to visit.
// p_Visitor:		The visitor function that will be called on matching nodes.
//
template <typename VisitorType>
void XMLForEachNodeNamed(const xmlNodePtr p_StartNode, char const* p_SearchName, VisitorType p_Visitor)
{
	// Search through all of the nodes at this level.
	for (auto l_Node = p_StartNode; l_Node != nullptr; l_Node = l_Node->next)
	{
		// If the name doesn't match, go to the next node.
		if (XMLIsNodeNamed(l_Node, p_SearchName) == false)
		{
			continue;
		}
		
		// Call the visitor function.
		p_Visitor(l_Node);
	}
}
