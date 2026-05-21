#include "sdf3d/ui/AddMenu.h"

#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/systems/GraphSystem.h"
#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <string>
#include <optional>
#include <utility>

#include <imgui.h>

namespace sdf3d {
namespace {

bool activeOutputNodeExists(const SdfGraph& graph)
{
    const SdfGraphNode* outputNode = graph.node(graph.outputNode());
    return outputNode != nullptr && outputNode->payload.type == SdfNodeType::Output;
}

bool outputSurfaceLinked(const SdfGraph& graph)
{
    const SdfGraphNodeId outputNode = graph.outputNode();
    for (const SdfGraphLink& link : graph.links()) {
        if (link.toNode == outputNode && link.toSocket == "surface") {
            return true;
        }
    }

    return false;
}

bool nodeFeedsOutputSurface(const SdfGraph& graph, SdfGraphNodeId node)
{
    const SdfGraphNodeId outputNode = graph.outputNode();
    for (const SdfGraphLink& link : graph.links()) {
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

bool inputAcceptsAutoLink(const SdfGraph& graph, SdfGraphNodeId node, const SdfGraphSocket& input)
{
    if (input.type != SdfSocketType::Sdf) {
        return false;
    }

    return input.multiInput || !inputHasLink(graph, node, input.name);
}

bool tryLinkNewNodeToSelectedInput(SdfGraph& graph, SdfGraphNodeId newNode, SdfGraphNodeId selectedNode)
{
    const SdfGraphNode* selected = graph.node(selectedNode);
    if (selected == nullptr || selectedNode == newNode || graph.isOutputNode(selectedNode)) {
        return false;
    }

    for (const SdfGraphSocket& input : selected->inputs) {
        if (inputAcceptsAutoLink(graph, selectedNode, input)) {
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
    return draw(sceneGraph.graph());
}

bool AddMenu::draw(SdfGraph& graph)
{
    if (!ImGui::BeginMenu("Add")) {
        return false;
    }

    const bool sceneDirty = drawItems(graph);
    ImGui::EndMenu();
    return sceneDirty;
}

bool AddMenu::drawPopup(SceneGraph& sceneGraph, float editorX, float editorY)
{
    return drawPopup(sceneGraph.graph(), editorX, editorY);
}

bool AddMenu::drawPopup(SdfGraph& graph, float editorX, float editorY)
{
    bool sceneDirty = false;
    if (ImGui::BeginPopup(node_editor::NODE_ADD_POPUP_ID)) {
        m_spawnEditorX = editorX;
        m_spawnEditorY = editorY;
        m_linkFromNode = 0;
        m_linkFromSocket.clear();
        m_linkToNode = 0;
        m_linkToSocket.clear();
        m_spawnWorldPosition.reset();
        sceneDirty = drawItems(graph);
        m_spawnEditorX.reset();
        m_spawnEditorY.reset();
        ImGui::EndPopup();
    }

    return sceneDirty;
}

bool AddMenu::drawPopupFromOutput(SceneGraph& sceneGraph, float editorX, float editorY, SdfGraphNodeId fromNode, std::string fromSocket)
{
    return drawPopupFromOutput(sceneGraph.graph(), editorX, editorY, fromNode, std::move(fromSocket));
}

bool AddMenu::drawPopupFromOutput(SdfGraph& graph, float editorX, float editorY, SdfGraphNodeId fromNode, std::string fromSocket)
{
    bool sceneDirty = false;
    if (ImGui::BeginPopup(node_editor::NODE_ADD_POPUP_ID)) {
        m_spawnEditorX = editorX;
        m_spawnEditorY = editorY;
        m_linkFromNode = fromNode;
        m_linkFromSocket = std::move(fromSocket);
        m_linkToNode = 0;
        m_linkToSocket.clear();
        m_spawnWorldPosition.reset();
        sceneDirty = drawItems(graph);
        m_spawnEditorX.reset();
        m_spawnEditorY.reset();
        ImGui::EndPopup();
    }

    return sceneDirty;
}

bool AddMenu::drawPopupToInput(SceneGraph& sceneGraph, float editorX, float editorY, SdfGraphNodeId toNode, std::string toSocket)
{
    return drawPopupToInput(sceneGraph.graph(), editorX, editorY, toNode, std::move(toSocket));
}

bool AddMenu::drawPopupToInput(SdfGraph& graph, float editorX, float editorY, SdfGraphNodeId toNode, std::string toSocket)
{
    bool sceneDirty = false;
    if (ImGui::BeginPopup(node_editor::NODE_ADD_POPUP_ID)) {
        m_spawnEditorX = editorX;
        m_spawnEditorY = editorY;
        m_linkFromNode = 0;
        m_linkFromSocket.clear();
        m_linkToNode = toNode;
        m_linkToSocket = std::move(toSocket);
        m_spawnWorldPosition.reset();
        sceneDirty = drawItems(graph);
        m_spawnEditorX.reset();
        m_spawnEditorY.reset();
        ImGui::EndPopup();
    }

    return sceneDirty;
}

bool AddMenu::drawPopupBetween(SceneGraph& sceneGraph, float editorX, float editorY, SdfGraphNodeId fromNode, std::string fromSocket, SdfGraphNodeId toNode, std::string toSocket)
{
    return drawPopupBetween(sceneGraph.graph(), editorX, editorY, fromNode, std::move(fromSocket), toNode, std::move(toSocket));
}

bool AddMenu::drawPopupBetween(SdfGraph& graph, float editorX, float editorY, SdfGraphNodeId fromNode, std::string fromSocket, SdfGraphNodeId toNode, std::string toSocket)
{
    bool sceneDirty = false;
    if (ImGui::BeginPopup(node_editor::NODE_ADD_POPUP_ID)) {
        m_spawnEditorX = editorX;
        m_spawnEditorY = editorY;
        m_linkFromNode = fromNode;
        m_linkFromSocket = std::move(fromSocket);
        m_linkToNode = toNode;
        m_linkToSocket = std::move(toSocket);
        m_spawnWorldPosition.reset();
        sceneDirty = drawItems(graph);
        m_spawnEditorX.reset();
        m_spawnEditorY.reset();
        ImGui::EndPopup();
    }

    return sceneDirty;
}

bool AddMenu::drawViewportPopup(SceneGraph& sceneGraph, glm::vec3 worldPosition)
{
    return drawViewportPopup(sceneGraph.graph(), worldPosition);
}

bool AddMenu::drawViewportPopup(SdfGraph& graph, glm::vec3 worldPosition)
{
    bool sceneDirty = false;
    if (ImGui::BeginPopup(node_editor::NODE_ADD_POPUP_ID)) {
        m_spawnEditorX.reset();
        m_spawnEditorY.reset();
        m_spawnWorldPosition = worldPosition;
        m_linkFromNode = 0;
        m_linkFromSocket.clear();
        m_linkToNode = 0;
        m_linkToSocket.clear();
        sceneDirty = drawItems(graph);
        m_spawnWorldPosition.reset();
        ImGui::EndPopup();
    }

    return sceneDirty;
}

bool AddMenu::drawItems(SdfGraph& graph)
{
    bool sceneDirty = false;
    auto addAndLinkSelected = [&](SdfNodePtr node, const char* inputSocket) {
        const bool popupLinkMode = m_linkFromNode != 0 || m_linkToNode != 0;
        const SdfGraphNodeId previousSelection = graph.selectedNode();
        const bool previousFedOutput = nodeFeedsOutputSurface(graph, previousSelection);
        addPrimitive(graph, std::move(node), false);
        sceneDirty = true;

        const SdfGraphNodeId createdNode = graph.selectedNode();
        if (!popupLinkMode && previousSelection != 0 && createdNode != 0 && previousSelection != createdNode) {
            // AGENT: Operation shortcuts wrap the selected graph node by linking
            // it into the new operation, matching common node-editor behavior.
            if (graph.link(previousSelection, createdNode, inputSocket)) {
                if (!activeOutputNodeExists(graph)) {
                    graph.setOutputNode(createdNode);
                } else if (previousFedOutput) {
                    graph.link(createdNode, "sdf", graph.outputNode(), "surface");
                }
                sceneDirty = true;
            }
        }
    };

    for (SdfNodeType type : sdfNodeTypesForCategory(SdfNodeCategory::Primitive)) {
        const SdfNodeDefinition* definition = sdfNodeDefinition(type);
        if (definition != nullptr && ImGui::MenuItem(definition->displayName.c_str())) {
            addPrimitive(graph, makeSdfNodeFromDefinition(type));
            sceneDirty = true;
        }
    }

    if (ImGui::BeginMenu("Transform")) {
        const bool hasSelection = graph.selectedNode() != 0;
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

            const char* inputSocket = (type == SdfNodeType::Subtract || type == SdfNodeType::SmoothSubtract) ? "base" : "inputs";
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

void AddMenu::addPrimitive(SdfGraph& graph, SdfNodePtr node, bool linkToSelection)
{
    const SdfGraphNodeId previousSelection = graph.selectedNode();
    // AGENT: New primitive creation targets the graph model so render output
    // and UI selection share the same future-facing scene representation.
    const SdfGraphNodeId id = graph.createNode(node->type, node->name);
    if (SdfGraphNode* graphNode = graph.node(id)) {
        const uint64_t stableId = graphNode->payload.stableId;
        const MaterialId materialId = graphNode->payload.materialId;
        graphNode->payload = *node;
        graphNode->payload.stableId = stableId;
        if (isSdfMaterialNode(graphNode->payload.type)) {
            graphNode->payload.materialId = materialId;
            if (MaterialDefinition* material = graph.materials().material(materialId)) {
                material->material = graphNode->payload.material;
            }
        }
        if (m_spawnEditorX && m_spawnEditorY) {
            graphNode->editorX = *m_spawnEditorX;
            graphNode->editorY = *m_spawnEditorY;
        }
    }
    const bool viewportPrimitive = m_spawnWorldPosition && isSdfPrimitiveNode(node->type);
    if (!viewportPrimitive && activeOutputNodeExists(graph) && !outputSurfaceLinked(graph) && node->type != SdfNodeType::Output) {
        graph.link(id, "sdf", graph.outputNode(), "surface");
    }
    if (viewportPrimitive) {
        GraphSystem::placePrimitiveAtWorldPosition(graph, id, *m_spawnWorldPosition);
    }
    if (linkToSelection) {
        tryLinkNewNodeToSelectedInput(graph, id, previousSelection);
    }
    if (!activeOutputNodeExists(graph)) {
        graph.setOutputNode(id);
    }
    linkCreatedNode(graph, id);
}

void AddMenu::linkCreatedNode(SdfGraph& graph, SdfGraphNodeId createdNode)
{
    if (m_linkFromNode != 0 && !m_linkFromSocket.empty()) {
        // AGENT: Drag-from-output popup links the dragged output into the first
        // compatible SDF input on the newly created node.
        tryLinkOutputToNewNodeInput(graph, m_linkFromNode, m_linkFromSocket, createdNode);
    }
    if (m_linkToNode != 0 && !m_linkToSocket.empty()) {
        // AGENT: Drag-from-input popup restores the detached input by linking
        // the new node's first compatible SDF output back into that socket.
        tryLinkNewNodeOutputToInput(graph, createdNode, m_linkToNode, m_linkToSocket);
    }
}

} // namespace sdf3d
