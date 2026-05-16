#pragma once

#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/scene/SdfGraph.h"

#include <vector>

namespace sdf3d {

/// Owns editor selection helpers and scene dirty state.
class SelectionSystem {
public:
    /// Sets the currently selected graph node.
    static bool setSelectedNode(SdfGraph& graph, SdfGraphNodeId id);

    /// Returns the currently selected graph node, or 0 when nothing is selected.
    static SdfGraphNodeId selectedNode(const SdfGraph& graph);

    /// Toggles a graph node in the multi-selection set.
    static bool toggleSelectedNode(SdfGraph& graph, SdfGraphNodeId id);

    /// Replaces selected graph nodes and primary selected node.
    static bool setSelectedNodes(SdfGraph& graph, std::vector<SdfGraphNodeId> ids, SdfGraphNodeId primary);

    /// Clears graph selection.
    static void clearSelection(SdfGraph& graph);

    /// Returns true when a graph node is selected.
    static bool isNodeSelected(const SdfGraph& graph, SdfGraphNodeId id);

    /// Sets the selected legacy tree node.
    static void setSelectedNode(SceneGraph& sceneGraph, SdfNodePtr node);

    /// Returns the selected legacy tree node.
    static const SdfNodePtr& selectedNode(const SceneGraph& sceneGraph);

    /// Marks scene data dirty.
    void markDirty();

    /// Marks material uniform data dirty.
    void markMaterialDirty();

    /// Returns true once when scene data was dirty.
    bool consumeDirty();

    /// Returns true once when material uniform data was dirty.
    bool consumeMaterialDirty();

private:
    bool m_dirty = false;
    bool m_materialDirty = false;
};

} // namespace sdf3d
