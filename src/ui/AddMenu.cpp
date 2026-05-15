#include "sdf3d/ui/AddMenu.h"

#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <string>
#include <utility>

#include <imgui.h>

namespace sdf3d {
namespace {

bool activeOutputNodeExists(const SceneGraph& sceneGraph)
{
    const SdfGraphNode* outputNode = sceneGraph.graph().node(sceneGraph.graph().outputNode());
    return outputNode != nullptr && outputNode->payload.type == SdfNodeType::Output;
}

bool outputSurfaceLinked(const SceneGraph& sceneGraph)
{
    const SdfGraphNodeId outputNode = sceneGraph.graph().outputNode();
    for (const SdfGraphLink& link : sceneGraph.graph().links()) {
        if (link.toNode == outputNode && link.toSocket == "surface") {
            return true;
        }
    }

    return false;
}

bool nodeFeedsOutputSurface(const SceneGraph& sceneGraph, SdfGraphNodeId node)
{
    const SdfGraphNodeId outputNode = sceneGraph.graph().outputNode();
    for (const SdfGraphLink& link : sceneGraph.graph().links()) {
        if (link.fromNode == node && link.toNode == outputNode && link.toSocket == "surface") {
            return true;
        }
    }

    return false;
}

bool inputHasLink(const SdfGraph& graph, SdfGraphNodeId node, const std::string& socket)
{
    for (const SdfGraphLink& link : graph.links()) {
        if (link.toNode == node && link.toSocket == socket) {
            return true;
        }
    }

    return false;
}

bool tryLinkNewNodeToSelectedInput(SdfGraph& graph, SdfGraphNodeId newNode, SdfGraphNodeId selectedNode)
{
    const SdfGraphNode* selected = graph.node(selectedNode);
    if (selected == nullptr || selectedNode == newNode || graph.isOutputNode(selectedNode)) {
        return false;
    }

    for (const SdfGraphSocket& input : selected->inputs) {
        if (input.type == SdfSocketType::Sdf && !inputHasLink(graph, selectedNode, input.name)) {
            return graph.link(newNode, "sdf", selectedNode, input.name);
        }
    }

    return false;
}

bool tryLinkOutputToNewNodeInput(SdfGraph& graph, SdfGraphNodeId fromNode, const std::string& fromSocket, SdfGraphNodeId newNode)
{
    const SdfGraphNode* created = graph.node(newNode);
    if (created == nullptr) {
        return false;
    }

    for (const SdfGraphSocket& input : created->inputs) {
        if (input.type == SdfSocketType::Sdf && graph.link(fromNode, fromSocket, newNode, input.name)) {
            return true;
        }
    }

    return false;
}

bool tryLinkNewNodeOutputToInput(SdfGraph& graph, SdfGraphNodeId newNode, SdfGraphNodeId toNode, const std::string& toSocket)
{
    const SdfGraphNode* created = graph.node(newNode);
    if (created == nullptr) {
        return false;
    }

    for (const SdfGraphSocket& output : created->outputs) {
        if (output.type == SdfSocketType::Sdf && graph.link(newNode, output.name, toNode, toSocket)) {
            return true;
        }
    }

    return false;
}

} // namespace

bool AddMenu::draw(SceneGraph& sceneGraph)
{
    if (!ImGui::BeginMenu("Add")) {
        return false;
    }

    const bool sceneDirty = drawItems(sceneGraph);
    ImGui::EndMenu();
    return sceneDirty;
}

bool AddMenu::drawPopup(SceneGraph& sceneGraph, float editorX, float editorY)
{
    bool sceneDirty = false;
    if (ImGui::BeginPopup(node_editor::NODE_ADD_POPUP_ID)) {
        m_spawnEditorX = editorX;
        m_spawnEditorY = editorY;
        m_linkFromNode = 0;
        m_linkFromSocket.clear();
        m_linkToNode = 0;
        m_linkToSocket.clear();
        sceneDirty = drawItems(sceneGraph);
        m_spawnEditorX.reset();
        m_spawnEditorY.reset();
        ImGui::EndPopup();
    }

    return sceneDirty;
}

bool AddMenu::drawPopupFromOutput(SceneGraph& sceneGraph, float editorX, float editorY, SdfGraphNodeId fromNode, std::string fromSocket)
{
    bool sceneDirty = false;
    if (ImGui::BeginPopup(node_editor::NODE_ADD_POPUP_ID)) {
        m_spawnEditorX = editorX;
        m_spawnEditorY = editorY;
        m_linkFromNode = fromNode;
        m_linkFromSocket = std::move(fromSocket);
        m_linkToNode = 0;
        m_linkToSocket.clear();
        sceneDirty = drawItems(sceneGraph);
        m_spawnEditorX.reset();
        m_spawnEditorY.reset();
        ImGui::EndPopup();
    }

    return sceneDirty;
}

bool AddMenu::drawPopupToInput(SceneGraph& sceneGraph, float editorX, float editorY, SdfGraphNodeId toNode, std::string toSocket)
{
    bool sceneDirty = false;
    if (ImGui::BeginPopup(node_editor::NODE_ADD_POPUP_ID)) {
        m_spawnEditorX = editorX;
        m_spawnEditorY = editorY;
        m_linkFromNode = 0;
        m_linkFromSocket.clear();
        m_linkToNode = toNode;
        m_linkToSocket = std::move(toSocket);
        sceneDirty = drawItems(sceneGraph);
        m_spawnEditorX.reset();
        m_spawnEditorY.reset();
        ImGui::EndPopup();
    }

    return sceneDirty;
}

bool AddMenu::drawItems(SceneGraph& sceneGraph)
{
    bool sceneDirty = false;
    auto addAndLinkSelected = [&](SdfNodePtr node, const char* inputSocket) {
        const SdfGraphNodeId previousSelection = sceneGraph.graph().selectedNode();
        const bool previousFedOutput = nodeFeedsOutputSurface(sceneGraph, previousSelection);
        addPrimitive(sceneGraph, std::move(node), false);
        sceneDirty = true;

        const SdfGraphNodeId createdNode = sceneGraph.graph().selectedNode();
        if (previousSelection != 0 && createdNode != 0 && previousSelection != createdNode) {
            // AGENT: Operation shortcuts wrap the selected graph node by linking
            // it into the new operation, matching common node-editor behavior.
            if (sceneGraph.graph().link(previousSelection, createdNode, inputSocket)) {
                if (!activeOutputNodeExists(sceneGraph)) {
                    sceneGraph.graph().setOutputNode(createdNode);
                } else if (previousFedOutput) {
                    sceneGraph.graph().link(createdNode, "sdf", sceneGraph.graph().outputNode(), "surface");
                }
                sceneDirty = true;
            }
        }
    };

    for (SdfNodeType type : sdfNodeTypesForCategory(SdfNodeCategory::Primitive)) {
        const SdfNodeDefinition* definition = sdfNodeDefinition(type);
        if (definition != nullptr && ImGui::MenuItem(definition->displayName.c_str())) {
            addPrimitive(sceneGraph, makeSdfNodeFromDefinition(type));
            sceneDirty = true;
        }
    }

    if (ImGui::BeginMenu("Transform")) {
        const bool hasSelection = sceneGraph.graph().selectedNode() != 0 || sceneGraph.selectedNode() != nullptr;
        for (SdfNodeType type : sdfNodeTypesForCategory(SdfNodeCategory::Transform)) {
            const SdfNodeDefinition* definition = sdfNodeDefinition(type);
            if (definition != nullptr && ImGui::MenuItem(definition->displayName.c_str(), nullptr, false, hasSelection)) {
                addAndLinkSelected(makeSdfNodeFromDefinition(type), "child");
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Boolean")) {
        for (SdfNodeType type : sdfNodeTypesForCategory(SdfNodeCategory::Boolean)) {
            const SdfNodeDefinition* definition = sdfNodeDefinition(type);
            if (definition == nullptr) {
                continue;
            }

            const char* inputSocket = (type == SdfNodeType::Subtract || type == SdfNodeType::SmoothSubtract) ? "base" : "left";
            if (ImGui::MenuItem(definition->displayName.c_str())) {
                addAndLinkSelected(makeSdfNodeFromDefinition(type), inputSocket);
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Materials")) {
        for (SdfNodeType type : sdfNodeTypesForCategory(SdfNodeCategory::Material)) {
            const SdfNodeDefinition* definition = sdfNodeDefinition(type);
            if (definition != nullptr && ImGui::MenuItem(definition->displayName.c_str())) {
                addAndLinkSelected(makeSdfNodeFromDefinition(type), "sdf");
            }
        }
        ImGui::EndMenu();
    }

    return sceneDirty;
}

void AddMenu::addPrimitive(SceneGraph& sceneGraph, SdfNodePtr node, bool linkToSelection)
{
    const SdfGraphNodeId previousSelection = sceneGraph.graph().selectedNode();
    // AGENT: New primitive creation targets the graph model so render output
    // and UI selection share the same future-facing scene representation.
    const SdfGraphNodeId id = sceneGraph.graph().createNode(node->type, node->name);
    if (SdfGraphNode* graphNode = sceneGraph.graph().node(id)) {
        graphNode->payload = *node;
        if (m_spawnEditorX && m_spawnEditorY) {
            graphNode->editorX = *m_spawnEditorX;
            graphNode->editorY = *m_spawnEditorY;
        }
    }
    if (activeOutputNodeExists(sceneGraph) && !outputSurfaceLinked(sceneGraph) && node->type != SdfNodeType::Output) {
        sceneGraph.graph().link(id, "sdf", sceneGraph.graph().outputNode(), "surface");
    }
    if (linkToSelection) {
        tryLinkNewNodeToSelectedInput(sceneGraph.graph(), id, previousSelection);
    }
    if (!activeOutputNodeExists(sceneGraph)) {
        sceneGraph.graph().setOutputNode(id);
    }
    linkCreatedNode(sceneGraph, id);
}

void AddMenu::linkCreatedNode(SceneGraph& sceneGraph, SdfGraphNodeId createdNode)
{
    if (m_linkFromNode != 0 && !m_linkFromSocket.empty()) {
        // AGENT: Drag-from-output popup links the dragged output into the first
        // compatible SDF input on the newly created node.
        tryLinkOutputToNewNodeInput(sceneGraph.graph(), m_linkFromNode, m_linkFromSocket, createdNode);
    }
    if (m_linkToNode != 0 && !m_linkToSocket.empty()) {
        // AGENT: Drag-from-input popup restores the detached input by linking
        // the new node's first compatible SDF output back into that socket.
        tryLinkNewNodeOutputToInput(sceneGraph.graph(), createdNode, m_linkToNode, m_linkToSocket);
    }
}

} // namespace sdf3d
