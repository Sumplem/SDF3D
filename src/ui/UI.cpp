#include "sdf3d/ui/UI.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

namespace sdf3d {
namespace {

const char* graphNodeTypeName(SdfNodeType type)
{
    switch (type) {
    case SdfNodeType::Sphere:
        return "Sphere";
    case SdfNodeType::Box:
        return "Box";
    case SdfNodeType::Cylinder:
        return "Cylinder";
    case SdfNodeType::Torus:
        return "Torus";
    case SdfNodeType::Plane:
        return "Plane";
    case SdfNodeType::Union:
        return "Union";
    case SdfNodeType::SmoothUnion:
        return "Smooth Union";
    case SdfNodeType::Subtract:
        return "Subtract";
    case SdfNodeType::SmoothSubtract:
        return "Smooth Subtract";
    case SdfNodeType::Intersect:
        return "Intersect";
    case SdfNodeType::SmoothIntersect:
        return "Smooth Intersect";
    case SdfNodeType::Translate:
        return "Translate";
    case SdfNodeType::Rotate:
        return "Rotate";
    case SdfNodeType::Scale:
        return "Scale";
    default:
        return "Node";
    }
}

const char* socketTypeName(SdfSocketType type)
{
    switch (type) {
    case SdfSocketType::Sdf:
        return "SDF";
    case SdfSocketType::Float:
        return "Float";
    case SdfSocketType::Vector3:
        return "Vector3";
    case SdfSocketType::Material:
        return "Material";
    }

    return "Unknown";
}

std::string graphNodeLabel(const SdfGraphNode& node)
{
    std::string label = node.payload.name.empty() ? graphNodeTypeName(node.payload.type) : node.payload.name;
    label += " #";
    label += std::to_string(node.id);
    label += "##graph-node-";
    label += std::to_string(node.id);
    return label;
}

std::string socketDisplayName(const SdfGraphSocket& socket)
{
    std::string label = socket.name;
    label += " : ";
    label += socketTypeName(socket.type);
    return label;
}

std::string graphNodeDisplayName(const SdfGraphNode* node)
{
    if (node == nullptr) {
        return "None";
    }

    std::string label = node->payload.name.empty() ? graphNodeTypeName(node->payload.type) : node->payload.name;
    label += " #";
    label += std::to_string(node->id);
    return label;
}

const char* secondarySocketFor(SdfNodeType type)
{
    switch (type) {
    case SdfNodeType::Subtract:
    case SdfNodeType::SmoothSubtract:
        return "cutter";
    case SdfNodeType::Union:
    case SdfNodeType::SmoothUnion:
    case SdfNodeType::Intersect:
    case SdfNodeType::SmoothIntersect:
        return "right";
    default:
        return nullptr;
    }
}

struct GraphNodeLayout {
    SdfGraphNodeId id = 0;
    SdfGraphNode* node = nullptr;
    ImVec2 position = {0.0f, 0.0f};
    ImVec2 size = {0.0f, 0.0f};
};

struct GraphSocketAnchor {
    SdfGraphNodeId node = 0;
    std::string socket;
    bool output = false;
    ImVec2 position = {0.0f, 0.0f};
};

std::optional<ImVec2> findSocketAnchor(
    const std::vector<GraphSocketAnchor>& anchors,
    SdfGraphNodeId node,
    const std::string& socket,
    bool output)
{
    for (const GraphSocketAnchor& anchor : anchors) {
        if (anchor.node == node && anchor.socket == socket && anchor.output == output) {
            return anchor.position;
        }
    }

    return std::nullopt;
}

} // namespace

void UI::drawMainMenu(SceneGraph& sceneGraph)
{
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        ImGui::MenuItem("Exit");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        ImGui::EndMenu();
    }

    drawAddMenu(sceneGraph);

    if (ImGui::BeginMenu("View")) {
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void UI::drawPanels(SceneGraph& sceneGraph)
{
    drawScenePanel(sceneGraph);
    drawProperties(sceneGraph);
}

bool UI::consumeSceneDirty()
{
    const bool dirty = m_sceneDirty;
    m_sceneDirty = false;
    return dirty;
}

void UI::drawAddMenu(SceneGraph& sceneGraph)
{
    if (!ImGui::BeginMenu("Add")) {
        return;
    }

    auto addAndLinkSelected = [&](SdfNodePtr node, const char* inputSocket) {
        const SdfGraphNodeId previousSelection = sceneGraph.graph().selectedNode();
        addPrimitive(sceneGraph, std::move(node));

        const SdfGraphNodeId createdNode = sceneGraph.graph().selectedNode();
        if (previousSelection != 0 && createdNode != 0 && previousSelection != createdNode) {
            // AGENT: Operation shortcuts wrap the selected graph node by linking
            // it into the new operation, matching common node-editor behavior.
            if (sceneGraph.graph().link(previousSelection, createdNode, inputSocket)) {
                sceneGraph.graph().setOutputNode(createdNode);
                markSceneDirty();
            }
        }
    };

    if (ImGui::MenuItem("Sphere")) {
        addPrimitive(sceneGraph, makeSphereNode());
    }
    if (ImGui::MenuItem("Box")) {
        addPrimitive(sceneGraph, makeBoxNode());
    }
    if (ImGui::MenuItem("Cylinder")) {
        addPrimitive(sceneGraph, makeCylinderNode());
    }
    if (ImGui::MenuItem("Torus")) {
        addPrimitive(sceneGraph, makeTorusNode());
    }
    if (ImGui::MenuItem("Plane")) {
        addPrimitive(sceneGraph, makePlaneNode());
    }

    if (ImGui::BeginMenu("Transform")) {
        const bool hasSelection = sceneGraph.graph().selectedNode() != 0 || sceneGraph.selectedNode() != nullptr;
        if (ImGui::MenuItem("Translate", nullptr, false, hasSelection)) {
            addAndLinkSelected(makeTranslateNode(nullptr, {0.0f, 0.0f, 0.0f}), "child");
        }
        if (ImGui::MenuItem("Rotate", nullptr, false, hasSelection)) {
            addAndLinkSelected(makeRotateNode(nullptr, {0.0f, 0.0f, 0.0f}), "child");
        }
        if (ImGui::MenuItem("Scale", nullptr, false, hasSelection)) {
            addAndLinkSelected(makeScaleNode(nullptr, 1.0f), "child");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Boolean")) {
        if (ImGui::MenuItem("Union")) {
            addAndLinkSelected(makeUnionNode({}), "left");
        }
        if (ImGui::MenuItem("Subtract")) {
            addAndLinkSelected(makeSdfNode(SdfNodeType::Subtract, "Subtract"), "base");
        }
        if (ImGui::MenuItem("Intersect")) {
            addAndLinkSelected(makeIntersectNode({}), "left");
        }
        if (ImGui::MenuItem("Smooth Union")) {
            addAndLinkSelected(makeSmoothUnionNode({}), "left");
        }
        if (ImGui::MenuItem("Smooth Subtract")) {
            SdfNodePtr node = makeSdfNode(SdfNodeType::SmoothSubtract, "Smooth Subtract");
            node->parameters["smoothness"] = 0.25f;
            addAndLinkSelected(std::move(node), "base");
        }
        if (ImGui::MenuItem("Smooth Intersect")) {
            addAndLinkSelected(makeSmoothIntersectNode({}), "left");
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenu();
}

void UI::drawScenePanel(SceneGraph& sceneGraph)
{
    ImGui::Begin("Scene");

    if (ImGui::GetCurrentContext() != nullptr) {
        SdfGraph& graph = sceneGraph.graph();

        if (!graph.nodes().empty()) {
            if (ImGui::Button("Clear Graph")) {
                std::vector<SdfGraphNodeId> idsToDelete;
                idsToDelete.reserve(graph.nodes().size());
                for (const auto& [id, node] : graph.nodes()) {
                    (void)node;
                    idsToDelete.push_back(id);
                }
                for (SdfGraphNodeId id : idsToDelete) {
                    graph.deleteNode(id);
                }
                markSceneDirty();
            }

            ImGui::SameLine();
            if (SdfGraphNode* selectedGraphNode = graph.node(graph.selectedNode())) {
                const char* secondarySocket = secondarySocketFor(selectedGraphNode->payload.type);
                if (secondarySocket != nullptr && ImGui::Button("Add Operand")) {
                    const SdfGraphNodeId operationNode = selectedGraphNode->id;
                    const SdfGraphNodeId operandNode = graph.createNode(SdfNodeType::Sphere, "Operand");
                    if (SdfGraphNode* operand = graph.node(operandNode)) {
                        operand->payload = *makeSphereNode("Operand");
                        operand->payload.parameters["radius"] = 0.5f;
                    }
                    // AGENT: Boolean helper creates a visible editable operand
                    // and links it into the selected operation's secondary input.
                    graph.link(operandNode, operationNode, secondarySocket);
                    graph.setSelectedNode(operandNode);
                    graph.setOutputNode(operationNode);
                    markSceneDirty();
                }
            }
        }

        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 canvasSize = {std::max(available.x, 320.0f), std::max(available.y, 260.0f)};
        ImGui::BeginChild("GraphCanvas", canvasSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
        const ImVec2 canvasEnd = {canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y};
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(canvasOrigin, canvasEnd, IM_COL32(30, 32, 36, 255));

        constexpr float gridStep = 32.0f;
        for (float x = canvasOrigin.x; x < canvasEnd.x; x += gridStep) {
            drawList->AddLine({x, canvasOrigin.y}, {x, canvasEnd.y}, IM_COL32(48, 50, 56, 255));
        }
        for (float y = canvasOrigin.y; y < canvasEnd.y; y += gridStep) {
            drawList->AddLine({canvasOrigin.x, y}, {canvasEnd.x, y}, IM_COL32(48, 50, 56, 255));
        }

        std::vector<SdfGraphNodeId> ids;
        ids.reserve(graph.nodes().size());
        for (const auto& [id, node] : graph.nodes()) {
            (void)node;
            ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end());

        std::vector<GraphNodeLayout> layouts;
        std::vector<GraphSocketAnchor> anchors;
        layouts.reserve(ids.size());

        constexpr float nodeWidth = 220.0f;
        constexpr float titleHeight = 28.0f;
        constexpr float socketRowHeight = 22.0f;
        for (size_t index = 0; index < ids.size(); ++index) {
            SdfGraphNode* node = graph.node(ids[index]);
            if (node == nullptr) {
                continue;
            }

            if (node->editorX == 0.0f && node->editorY == 0.0f && ids[index] != 1) {
                node->editorX = 40.0f + static_cast<float>(index % 3) * 260.0f;
                node->editorY = 40.0f + static_cast<float>(index / 3) * 150.0f;
            }

            const size_t socketRows = std::max(node->inputs.size(), node->outputs.size());
            const float nodeHeight = titleHeight + 34.0f + std::max<size_t>(1, socketRows) * socketRowHeight;
            const ImVec2 nodePosition = {canvasOrigin.x + 24.0f + node->editorX, canvasOrigin.y + 24.0f + node->editorY};
            layouts.push_back({ids[index], node, nodePosition, {nodeWidth, nodeHeight}});

            for (size_t i = 0; i < node->inputs.size(); ++i) {
                anchors.push_back({ids[index], node->inputs[i].name, false, {nodePosition.x, nodePosition.y + titleHeight + 18.0f + static_cast<float>(i) * socketRowHeight}});
            }
            for (size_t i = 0; i < node->outputs.size(); ++i) {
                anchors.push_back({ids[index], node->outputs[i].name, true, {nodePosition.x + nodeWidth, nodePosition.y + titleHeight + 18.0f + static_cast<float>(i) * socketRowHeight}});
            }
        }

        for (const SdfGraphLink& link : graph.links()) {
            const std::optional<ImVec2> from = findSocketAnchor(anchors, link.fromNode, link.fromSocket, true);
            const std::optional<ImVec2> to = findSocketAnchor(anchors, link.toNode, link.toSocket, false);
            if (from && to) {
                drawList->AddBezierCubic(*from, {from->x + 70.0f, from->y}, {to->x - 70.0f, to->y}, *to, IM_COL32(130, 170, 255, 255), 3.0f);
            }
        }

        static SdfGraphNodeId pendingOutputNode = 0;
        static std::string pendingOutputSocket;
        SdfGraphNodeId pendingDelete = 0;

        if (layouts.empty()) {
            drawList->AddText({canvasOrigin.x + 16.0f, canvasOrigin.y + 16.0f}, IM_COL32(210, 215, 225, 255), "Empty graph");
        }

        for (const GraphNodeLayout& layout : layouts) {
            SdfGraphNode& node = *layout.node;
            const bool selected = graph.selectedNode() == layout.id;
            const bool output = graph.outputNode() == layout.id;
            const ImU32 bodyColor = selected ? IM_COL32(58, 66, 84, 255) : IM_COL32(42, 45, 52, 255);
            const ImU32 titleColor = output ? IM_COL32(76, 96, 70, 255) : IM_COL32(54, 58, 68, 255);
            const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};

            drawList->AddRectFilled(layout.position, nodeEnd, bodyColor, 6.0f);
            drawList->AddRectFilled(layout.position, {nodeEnd.x, layout.position.y + titleHeight}, titleColor, 6.0f, ImDrawFlags_RoundCornersTop);
            drawList->AddRect(layout.position, nodeEnd, selected ? IM_COL32(120, 170, 255, 255) : IM_COL32(78, 82, 92, 255), 6.0f, 0, selected ? 2.0f : 1.0f);

            const std::string title = graphNodeDisplayName(&node) + (output ? "  [Output]" : "");
            drawList->AddText({layout.position.x + 10.0f, layout.position.y + 7.0f}, IM_COL32(235, 238, 242, 255), title.c_str());

            ImGui::SetCursorScreenPos(layout.position);
            ImGui::InvisibleButton(("node-title##" + std::to_string(layout.id)).c_str(), {layout.size.x, titleHeight});
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                graph.setSelectedNode(layout.id);
            }
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                node.editorX += delta.x;
                node.editorY += delta.y;
            }

            for (size_t i = 0; i < node.inputs.size(); ++i) {
                const SdfGraphSocket& socket = node.inputs[i];
                const ImVec2 pin = {layout.position.x, layout.position.y + titleHeight + 18.0f + static_cast<float>(i) * socketRowHeight};
                drawList->AddCircleFilled(pin, 5.0f, IM_COL32(120, 180, 120, 255));
                drawList->AddText({pin.x + 10.0f, pin.y - 7.0f}, IM_COL32(220, 224, 230, 255), socketDisplayName(socket).c_str());
                ImGui::SetCursorScreenPos({pin.x - 8.0f, pin.y - 8.0f});
                ImGui::InvisibleButton(("input##" + std::to_string(layout.id) + "-" + socket.name).c_str(), {16.0f, 16.0f});
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    if (pendingOutputNode != 0) {
                        if (graph.link(pendingOutputNode, pendingOutputSocket, layout.id, socket.name)) {
                            graph.setSelectedNode(layout.id);
                            graph.setOutputNode(layout.id);
                            markSceneDirty();
                        }
                        pendingOutputNode = 0;
                        pendingOutputSocket.clear();
                    } else {
                        graph.setSelectedNode(layout.id);
                    }
                }
            }

            for (size_t i = 0; i < node.outputs.size(); ++i) {
                const SdfGraphSocket& socket = node.outputs[i];
                const ImVec2 pin = {layout.position.x + layout.size.x, layout.position.y + titleHeight + 18.0f + static_cast<float>(i) * socketRowHeight};
                drawList->AddCircleFilled(pin, 5.0f, pendingOutputNode == layout.id && pendingOutputSocket == socket.name ? IM_COL32(255, 210, 110, 255) : IM_COL32(120, 160, 240, 255));
                const std::string label = socketDisplayName(socket);
                const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
                drawList->AddText({pin.x - 10.0f - labelSize.x, pin.y - 7.0f}, IM_COL32(220, 224, 230, 255), label.c_str());
                ImGui::SetCursorScreenPos({pin.x - 8.0f, pin.y - 8.0f});
                ImGui::InvisibleButton(("output##" + std::to_string(layout.id) + "-" + socket.name).c_str(), {16.0f, 16.0f});
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    pendingOutputNode = layout.id;
                    pendingOutputSocket = socket.name;
                    graph.setSelectedNode(layout.id);
                }
            }

            ImGui::SetCursorScreenPos({layout.position.x + 10.0f, nodeEnd.y - 26.0f});
            if (ImGui::SmallButton(("Set Output##" + std::to_string(layout.id)).c_str())) {
                graph.setOutputNode(layout.id);
                markSceneDirty();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(("Delete##" + std::to_string(layout.id)).c_str())) {
                pendingDelete = layout.id;
            }
        }

        if (pendingOutputNode != 0) {
            const std::string label = "Linking from " + std::to_string(pendingOutputNode) + "." + pendingOutputSocket;
            drawList->AddText({canvasOrigin.x + 16.0f, canvasEnd.y - 26.0f}, IM_COL32(255, 220, 130, 255), label.c_str());
        }

        if (pendingDelete != 0) {
            graph.deleteNode(pendingDelete);
            markSceneDirty();
        }

        ImGui::EndChild();
        // AGENT: Runtime branch keeps old text-list graph UI compiled as
        // fallback during this migration, but normal editor path is canvas.
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Graph");
    SdfGraph& graph = sceneGraph.graph();
    if (graph.nodes().empty()) {
        ImGui::TextUnformatted("Empty graph");
    } else {
        std::vector<SdfGraphNodeId> ids;
        ids.reserve(graph.nodes().size());
        for (const auto& [id, node] : graph.nodes()) {
            (void)node;
            ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end());

        SdfGraphNodeId pendingDelete = 0;
        for (SdfGraphNodeId id : ids) {
            const SdfGraphNode* node = graph.node(id);
            if (node == nullptr) {
                continue;
            }

            std::string label = node->payload.name.empty() ? graphNodeTypeName(node->payload.type) : node->payload.name;
            if (graph.outputNode() == id) {
                label += " [Output]";
            }

            const bool selected = graph.selectedNode() == id;
            if (ImGui::Selectable(label.c_str(), selected)) {
                graph.setSelectedNode(id);
            }

            ImGui::SameLine();
            if (ImGui::SmallButton(("Output##" + std::to_string(id)).c_str())) {
                graph.setOutputNode(id);
                markSceneDirty();
            }

            ImGui::SameLine();
            if (ImGui::SmallButton(("Delete##" + std::to_string(id)).c_str())) {
                pendingDelete = id;
            }

            ImGui::Indent();
            for (const SdfGraphSocket& input : node->inputs) {
                ImGui::Text("in  %s", socketDisplayName(input).c_str());
            }
            for (const SdfGraphSocket& output : node->outputs) {
                ImGui::Text("out %s", socketDisplayName(output).c_str());
            }
            ImGui::Unindent();
        }

        if (pendingDelete != 0) {
            graph.deleteNode(pendingDelete);
            markSceneDirty();
        }

        if (ImGui::Button("Clear Graph")) {
            std::vector<SdfGraphNodeId> idsToDelete = ids;
            // AGENT: Clear uses public delete API so link/output/selection
            // cleanup stays centralized in SdfGraph while UI remains thin.
            for (SdfGraphNodeId id : idsToDelete) {
                graph.deleteNode(id);
            }
            markSceneDirty();
            ImGui::End();
            return;
        }

        if (SdfGraphNode* selectedGraphNode = graph.node(graph.selectedNode())) {
            const char* secondarySocket = secondarySocketFor(selectedGraphNode->payload.type);
            if (secondarySocket != nullptr) {
                if (ImGui::Button("Add Operand")) {
                    const SdfGraphNodeId operationNode = selectedGraphNode->id;
                    const SdfGraphNodeId operandNode = graph.createNode(SdfNodeType::Sphere, "Operand");
                    if (SdfGraphNode* operand = graph.node(operandNode)) {
                        operand->payload = *makeSphereNode("Operand");
                        operand->payload.parameters["radius"] = 0.5f;
                    }
                    // AGENT: Boolean helper creates a visible editable operand
                    // and links it into the selected operation's secondary input.
                    graph.link(operandNode, operationNode, secondarySocket);
                    graph.setSelectedNode(operandNode);
                    graph.setOutputNode(operationNode);
                    markSceneDirty();
                }
            }
        }

        ImGui::SeparatorText("Links");
        static SdfGraphNodeId fromNode = 0;
        static SdfGraphNodeId toNode = 0;
        static std::string fromSocket = "sdf";
        static std::string toSocket = "child";

        auto drawNodeCombo = [&](const char* label, SdfGraphNodeId& value) {
            const SdfGraphNode* selectedNode = graph.node(value);
            const std::string preview = graphNodeDisplayName(selectedNode);
            if (ImGui::BeginCombo(label, preview.c_str())) {
                if (ImGui::Selectable("None", value == 0)) {
                    value = 0;
                }
                for (SdfGraphNodeId id : ids) {
                    const SdfGraphNode* node = graph.node(id);
                    if (node == nullptr) {
                        continue;
                    }
                    const std::string labelText = graphNodeLabel(*node);
                    if (ImGui::Selectable(labelText.c_str(), value == id)) {
                        value = id;
                    }
                }
                ImGui::EndCombo();
            }
        };

        drawNodeCombo("From", fromNode);
        drawNodeCombo("To", toNode);

        auto drawSocketCombo = [](const char* label, const std::vector<SdfGraphSocket>& sockets, std::string& value) {
            const char* preview = value.empty() ? "None" : value.c_str();
            if (ImGui::BeginCombo(label, preview)) {
                for (const SdfGraphSocket& socket : sockets) {
                    const std::string labelText = socketDisplayName(socket);
                    if (ImGui::Selectable(labelText.c_str(), value == socket.name)) {
                        value = socket.name;
                    }
                }
                ImGui::EndCombo();
            }
        };

        const SdfGraphNode* fromGraphNode = graph.node(fromNode);
        const SdfGraphNode* toGraphNode = graph.node(toNode);
        if (fromGraphNode != nullptr) {
            drawSocketCombo("From Socket", fromGraphNode->outputs, fromSocket);
        }
        if (toGraphNode != nullptr) {
            drawSocketCombo("To Socket", toGraphNode->inputs, toSocket);
        }

        // AGENT: Link controls now use declared node sockets instead of freeform
        // socket text, matching Blender Geometry Nodes' typed socket model.
        if (ImGui::Button("Link")) {
            if (graph.link(fromNode, fromSocket, toNode, toSocket)) {
                graph.setOutputNode(toNode);
                markSceneDirty();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Unlink Input")) {
            if (graph.unlinkInput(toNode, toSocket)) {
                markSceneDirty();
            }
        }

        if (!graph.links().empty()) {
            ImGui::SeparatorText("Current Links");
            for (const SdfGraphLink& link : graph.links()) {
                const SdfGraphNode* from = graph.node(link.fromNode);
                const SdfGraphNode* to = graph.node(link.toNode);
                const std::string fromName = graphNodeDisplayName(from);
                const std::string toName = graphNodeDisplayName(to);
                ImGui::Text("%s.%s -> %s.%s", fromName.c_str(), link.fromSocket.c_str(), toName.c_str(), link.toSocket.c_str());
            }
        }
    }

    // AGENT: The editor has moved to graph-first scene editing; legacy tree UI
    // remains compiled for migration helpers but is no longer displayed.
    ImGui::End();
}

void UI::drawProperties(SceneGraph& sceneGraph)
{
    ImGui::Begin("Properties");

    SdfNode* selected = nullptr;
    SdfGraphNode* selectedGraphNode = sceneGraph.graph().node(sceneGraph.graph().selectedNode());
    if (selectedGraphNode != nullptr) {
        selected = &selectedGraphNode->payload;
    } else if (sceneGraph.selectedNode()) {
        selected = sceneGraph.selectedNode().get();
    }

    if (selected == nullptr) {
        ImGui::TextUnformatted("No node selected.");
        ImGui::End();
        return;
    }

    char nameBuffer[128] = {};
    const std::string& currentName = selected->name;
    const size_t copyLength = std::min(currentName.size(), sizeof(nameBuffer) - 1);
    std::copy_n(currentName.data(), copyLength, nameBuffer);

    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        selected->name = nameBuffer;
        markSceneDirty();
    }

    ImGui::SeparatorText("Material");

    // AGENT: Material edits live beside generic float parameters so M4 users
    // can author per-node appearance before the renderer consumes material IDs.
    if (ImGui::ColorEdit3("Albedo", &selected->material.albedo.x)) {
        markSceneDirty();
    }
    if (ImGui::DragFloat("Roughness", &selected->material.roughness, 0.01f, 0.0f, 1.0f)) {
        markSceneDirty();
    }
    if (ImGui::DragFloat("Metallic", &selected->material.metallic, 0.01f, 0.0f, 1.0f)) {
        markSceneDirty();
    }
    if (ImGui::DragFloat("Emission", &selected->material.emission, 0.01f, 0.0f, 100.0f)) {
        markSceneDirty();
    }

    ImGui::SeparatorText("Parameters");

    if (selected->parameters.empty()) {
        ImGui::TextUnformatted("No editable parameters.");
        ImGui::End();
        return;
    }

    // AGENT: Sorting parameter keys gives deterministic UI order even though
    // the underlying storage is an unordered_map for simple serialization.
    std::vector<std::string> keys;
    keys.reserve(selected->parameters.size());
    for (const auto& [key, value] : selected->parameters) {
        (void)value;
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());

    for (const std::string& key : keys) {
        float& value = selected->parameters[key];
        if (ImGui::DragFloat(key.c_str(), &value, 0.01f)) {
            markSceneDirty();
        }
    }

    ImGui::End();
}

void UI::addPrimitive(SceneGraph& sceneGraph, SdfNodePtr node)
{
    // AGENT: New primitive creation targets the graph model so render output
    // and UI selection share the same future-facing scene representation.
    const SdfGraphNodeId id = sceneGraph.graph().createNode(node->type, node->name);
    if (SdfGraphNode* graphNode = sceneGraph.graph().node(id)) {
        graphNode->payload = *node;
    }
    sceneGraph.graph().setOutputNode(id);
    markSceneDirty();
}

void UI::markSceneDirty()
{
    m_sceneDirty = true;
}

} // namespace sdf3d
