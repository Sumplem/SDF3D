#pragma once

#include "sdf3d/scene/SceneGraph.h"

#include <glm/glm.hpp>

#include <optional>
#include <string>

namespace sdf3d {

/// Draws the Add menu and returns true when it mutates the scene graph.
class AddMenu {
public:
    bool draw(SceneGraph& sceneGraph);

    /// Draws the node graph popup Add menu at an optional editor-space spawn position.
    bool drawPopup(SceneGraph& sceneGraph, float editorX, float editorY);

    /// Draws Add popup for a drag starting from an output socket.
    bool drawPopupFromOutput(SceneGraph& sceneGraph, float editorX, float editorY, SdfGraphNodeId fromNode, std::string fromSocket);

    /// Draws Add popup for a drag detached from an input socket.
    bool drawPopupToInput(SceneGraph& sceneGraph, float editorX, float editorY, SdfGraphNodeId toNode, std::string toSocket);

    /// Draws Add popup for inserting a new node between an existing output and input.
    bool drawPopupBetween(SceneGraph& sceneGraph, float editorX, float editorY, SdfGraphNodeId fromNode, std::string fromSocket, SdfGraphNodeId toNode, std::string toSocket);

    /// Draws viewport Add popup and wraps new primitives in Translate at world position.
    bool drawViewportPopup(SceneGraph& sceneGraph, glm::vec3 worldPosition);

private:
    bool drawItems(SceneGraph& sceneGraph);
    void addPrimitive(SceneGraph& sceneGraph, SdfNodePtr node, bool linkToSelection = true);
    void linkCreatedNode(SceneGraph& sceneGraph, SdfGraphNodeId createdNode);

    std::optional<float> m_spawnEditorX;
    std::optional<float> m_spawnEditorY;
    std::optional<glm::vec3> m_spawnWorldPosition;
    SdfGraphNodeId m_linkFromNode = 0;
    std::string m_linkFromSocket;
    SdfGraphNodeId m_linkToNode = 0;
    std::string m_linkToSocket;
};

} // namespace sdf3d
