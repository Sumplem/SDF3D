#include "sdf3d/systems/SelectionSystem.h"

#include <utility>

namespace sdf3d {

bool SelectionSystem::setSelectedNode(SdfGraph& graph, SdfGraphNodeId id)
{
    if (id != 0 && graph.m_nodes.find(id) == graph.m_nodes.end()) {
        return false;
    }

    graph.m_selectedNode = id;
    return true;
}

SdfGraphNodeId SelectionSystem::selectedNode(const SdfGraph& graph)
{
    return graph.m_selectedNode;
}

void SelectionSystem::setSelectedNode(SceneGraph& sceneGraph, SdfNodePtr node)
{
    sceneGraph.m_selectedNode = std::move(node);
}

const SdfNodePtr& SelectionSystem::selectedNode(const SceneGraph& sceneGraph)
{
    return sceneGraph.m_selectedNode;
}

void SelectionSystem::markDirty()
{
    m_dirty = true;
}

bool SelectionSystem::consumeDirty()
{
    const bool dirty = m_dirty;
    m_dirty = false;
    return dirty;
}

} // namespace sdf3d
