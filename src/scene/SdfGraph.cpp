#include "sdf3d/scene/SdfGraph.h"

#include "sdf3d/systems/GraphSystem.h"
#include "sdf3d/systems/SelectionSystem.h"

namespace sdf3d {

SdfGraph::SdfGraph()
{
    GraphSystem::initialize(*this);
    SelectionSystem::setSelectedNode(*this, m_outputNode);
}

SdfGraphNodeId SdfGraph::createNode(SdfNodeType type, std::string name)
{
    const SdfGraphNodeId id = GraphSystem::createNode(*this, type, std::move(name));
    SelectionSystem::setSelectedNode(*this, id);
    return id;
}

SdfGraphNodeId SdfGraph::duplicateNode(SdfGraphNodeId id)
{
    const SdfGraphNodeId duplicateId = GraphSystem::duplicateNode(*this, id);
    if (duplicateId != 0) {
        SelectionSystem::setSelectedNode(*this, duplicateId);
    }
    return duplicateId;
}

bool SdfGraph::deleteNode(SdfGraphNodeId id)
{
    return GraphSystem::deleteNode(*this, id);
}

bool SdfGraph::link(SdfGraphNodeId fromNode, SdfGraphNodeId toNode, std::string toSocket)
{
    return GraphSystem::link(*this, fromNode, toNode, std::move(toSocket));
}

bool SdfGraph::link(SdfGraphNodeId fromNode, std::string fromSocket, SdfGraphNodeId toNode, std::string toSocket)
{
    return GraphSystem::link(*this, fromNode, std::move(fromSocket), toNode, std::move(toSocket));
}

bool SdfGraph::unlinkInput(SdfGraphNodeId toNode, const std::string& toSocket)
{
    return GraphSystem::unlinkInput(*this, toNode, toSocket);
}

bool SdfGraph::unlink(SdfGraphNodeId fromNode, const std::string& fromSocket, SdfGraphNodeId toNode, const std::string& toSocket)
{
    return GraphSystem::unlink(*this, fromNode, fromSocket, toNode, toSocket);
}

bool SdfGraph::setOutputNode(SdfGraphNodeId id)
{
    return GraphSystem::setOutputNode(*this, id);
}

SdfGraphNodeId SdfGraph::outputNode() const
{
    return m_outputNode;
}

bool SdfGraph::setSelectedNode(SdfGraphNodeId id)
{
    return SelectionSystem::setSelectedNode(*this, id);
}

SdfGraphNodeId SdfGraph::selectedNode() const
{
    return SelectionSystem::selectedNode(*this);
}

bool SdfGraph::toggleSelectedNode(SdfGraphNodeId id)
{
    return SelectionSystem::toggleSelectedNode(*this, id);
}

bool SdfGraph::setSelectedNodes(std::vector<SdfGraphNodeId> ids, SdfGraphNodeId primary)
{
    return SelectionSystem::setSelectedNodes(*this, std::move(ids), primary);
}

void SdfGraph::clearSelection()
{
    SelectionSystem::clearSelection(*this);
}

const std::vector<SdfGraphNodeId>& SdfGraph::selectedNodes() const
{
    return m_selectedNodes;
}

bool SdfGraph::isNodeSelected(SdfGraphNodeId id) const
{
    return SelectionSystem::isNodeSelected(*this, id);
}

bool SdfGraph::isOutputNode(SdfGraphNodeId id) const
{
    return GraphSystem::isOutputNode(*this, id);
}

bool SdfGraph::hasLinks(SdfGraphNodeId id) const
{
    return GraphSystem::hasLinks(*this, id);
}

SdfGraphNode* SdfGraph::node(SdfGraphNodeId id)
{
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) {
        return nullptr;
    }

    return &it->second;
}

const SdfGraphNode* SdfGraph::node(SdfGraphNodeId id) const
{
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) {
        return nullptr;
    }

    return &it->second;
}

const std::unordered_map<SdfGraphNodeId, SdfGraphNode>& SdfGraph::nodes() const
{
    return m_nodes;
}

const std::vector<SdfGraphLink>& SdfGraph::links() const
{
    return m_links;
}

} // namespace sdf3d
