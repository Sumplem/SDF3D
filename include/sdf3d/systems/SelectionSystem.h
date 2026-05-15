#pragma once

#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/scene/SdfGraph.h"

namespace sdf3d {

/// Owns editor selection helpers and scene dirty state.
class SelectionSystem {
public:
    /// Sets the currently selected graph node.
    static bool setSelectedNode(SdfGraph& graph, SdfGraphNodeId id);

    /// Returns the currently selected graph node, or 0 when nothing is selected.
    static SdfGraphNodeId selectedNode(const SdfGraph& graph);

    /// Sets the selected legacy tree node.
    static void setSelectedNode(SceneGraph& sceneGraph, SdfNodePtr node);

    /// Returns the selected legacy tree node.
    static const SdfNodePtr& selectedNode(const SceneGraph& sceneGraph);

    /// Marks scene data dirty.
    void markDirty();

    /// Returns true once when scene data was dirty.
    bool consumeDirty();

private:
    bool m_dirty = false;
};

} // namespace sdf3d
