#pragma once

#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/scene/SdfNode.h"

namespace sdf3d {

/// Owns the root SDF expression tree and current node selection.
class SceneGraph {
public:
    SceneGraph();

    /// Returns the root node of the SDF expression tree.
    const SdfNodePtr& root() const;

    /// Replaces the root node and clears stale selection.
    void setRoot(SdfNodePtr root);

    /// Returns the currently selected node, if any.
    const SdfNodePtr& selectedNode() const;

    /// Sets the current selected node.
    void setSelectedNode(SdfNodePtr node);

    /// Returns the editable SDF graph model.
    SdfGraph& graph();

    /// Returns the editable SDF graph model.
    const SdfGraph& graph() const;

private:
    // AGENT: SceneGraph owns both models during migration so current tree UI
    // stays working while graph compiler/UI land in small reviewable steps.
    SdfGraph m_graph;
    SdfNodePtr m_root;
    SdfNodePtr m_selectedNode;
};

} // namespace sdf3d
