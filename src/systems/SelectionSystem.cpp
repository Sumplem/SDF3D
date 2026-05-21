#include "sdf3d/systems/SelectionSystem.h"

#include <algorithm>
#include <utility>

namespace sdf3d {

bool SelectionSystem::setSelectedNode(SdfGraph& graph, SdfGraphNodeId id)
{
    if (id != 0 && graph.m_nodes.find(id) == graph.m_nodes.end()) {
        return false;
    }

    graph.m_selectedNode = id;
    graph.m_selectedNodes.clear();
    if (id != 0) {
        graph.m_selectedNodes.push_back(id);
    }
    return true;
}

SdfGraphNodeId SelectionSystem::selectedNode(const SdfGraph& graph)
{
    return graph.m_selectedNode;
}

bool SelectionSystem::toggleSelectedNode(SdfGraph& graph, SdfGraphNodeId id)
{
    if (id == 0 || graph.m_nodes.find(id) == graph.m_nodes.end()) {
        return false;
    }

    auto it = std::find(graph.m_selectedNodes.begin(), graph.m_selectedNodes.end(), id);
    if (it != graph.m_selectedNodes.end()) {
        graph.m_selectedNodes.erase(it);
        graph.m_selectedNode = graph.m_selectedNodes.empty() ? 0 : graph.m_selectedNodes.back();
        return true;
    }

    graph.m_selectedNodes.push_back(id);
    graph.m_selectedNode = id;
    return true;
}

bool SelectionSystem::setSelectedNodes(SdfGraph& graph, std::vector<SdfGraphNodeId> ids, SdfGraphNodeId primary)
{
    std::vector<SdfGraphNodeId> validIds;
    validIds.reserve(ids.size());
    for (SdfGraphNodeId id : ids) {
        if (id == 0 || graph.m_nodes.find(id) == graph.m_nodes.end()) {
            return false;
        }
        if (std::find(validIds.begin(), validIds.end(), id) == validIds.end()) {
            validIds.push_back(id);
        }
    }

    if (primary != 0 && std::find(validIds.begin(), validIds.end(), primary) == validIds.end()) {
        return false;
    }

    graph.m_selectedNodes = std::move(validIds);
    graph.m_selectedNode = primary != 0 ? primary : (graph.m_selectedNodes.empty() ? 0 : graph.m_selectedNodes.back());
    return true;
}

void SelectionSystem::clearSelection(SdfGraph& graph)
{
    graph.m_selectedNode = 0;
    graph.m_selectedNodes.clear();
}

bool SelectionSystem::isNodeSelected(const SdfGraph& graph, SdfGraphNodeId id)
{
    return std::find(graph.m_selectedNodes.begin(), graph.m_selectedNodes.end(), id) != graph.m_selectedNodes.end();
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

void SelectionSystem::markMaterialDirty()
{
    m_materialDirty = true;
}

void SelectionSystem::markParamDirty()
{
    m_paramDirty = true;
}

bool SelectionSystem::consumeDirty()
{
    const bool dirty = m_dirty;
    m_dirty = false;
    return dirty;
}

bool SelectionSystem::consumeMaterialDirty()
{
    const bool dirty = m_materialDirty;
    m_materialDirty = false;
    return dirty;
}

bool SelectionSystem::consumeParamDirty()
{
    const bool dirty = m_paramDirty;
    m_paramDirty = false;
    return dirty;
}

} // namespace sdf3d
