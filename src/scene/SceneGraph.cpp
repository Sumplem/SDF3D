#include "sdf3d/scene/SceneGraph.h"

#include "sdf3d/systems/SelectionSystem.h"

#include <utility>

namespace sdf3d {

SceneGraph::SceneGraph()
{
}

const SdfNodePtr& SceneGraph::root() const
{
    return m_root;
}

void SceneGraph::setRoot(SdfNodePtr root)
{
    // AGENT: Selection is reset with the root because M3 has no stable node IDs
    // yet to prove an old selected pointer still belongs to the new tree.
    m_root = std::move(root);
    SelectionSystem::setSelectedNode(*this, m_root);
}

const SdfNodePtr& SceneGraph::selectedNode() const
{
    return SelectionSystem::selectedNode(*this);
}

void SceneGraph::setSelectedNode(SdfNodePtr node)
{
    SelectionSystem::setSelectedNode(*this, std::move(node));
}

SdfGraph& SceneGraph::graph()
{
    return m_graph;
}

const SdfGraph& SceneGraph::graph() const
{
    return m_graph;
}

} // namespace sdf3d
