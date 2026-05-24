#include "sdf3d/ui/MaterialGraphPanel.h"

#include "sdf3d/systems/GraphSystem.h"
#include "sdf3d/ui/GraphCanvas.h"
#include "sdf3d/ui/GraphEditorCore.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

namespace sdf3d {
namespace {

constexpr float NODE_WIDTH = 240.0f;
constexpr float TITLE_HEIGHT = 28.0f;
constexpr float SOCKET_ROW_HEIGHT = 48.0f;
constexpr float SOCKET_LABEL_OFFSET_Y = 18.0f;
constexpr float EMBEDDED_INPUT_OFFSET_Y = 24.0f;
constexpr float MIN_ZOOM = 0.4f;
constexpr float MAX_ZOOM = 2.0f;

struct MaterialNodeLayout {
    MaterialGraphNodeId id = 0;
    MaterialGraphNode* node = nullptr;
    ImVec2 position = {0.0f, 0.0f};
    ImVec2 size = {0.0f, 0.0f};
};

struct MaterialSocketAnchor {
    MaterialGraphNodeId node = 0;
    std::string socket;
    bool output = false;
    ImVec2 position = {0.0f, 0.0f};
    MaterialGraphSocketType type = MaterialGraphSocketType::Float;
};

const char* displayName(MaterialGraphNodeType type)
{
    switch (type) {
    case MaterialGraphNodeType::PbrMaterial:
        return "PBR Material";
    case MaterialGraphNodeType::ColorConstant:
        return "Color";
    case MaterialGraphNodeType::FloatConstant:
        return "Float";
    case MaterialGraphNodeType::MixColor:
        return "Mix Color";
    case MaterialGraphNodeType::CheckerPattern:
        return "Checker";
    case MaterialGraphNodeType::ValueNoisePattern:
        return "Value Noise";
    case MaterialGraphNodeType::MaterialOutput:
        return "Material Output";
    }
    return "Node";
}

const char* socketTypeName(MaterialGraphSocketType type)
{
    switch (type) {
    case MaterialGraphSocketType::Color:
        return "Color";
    case MaterialGraphSocketType::Float:
        return "Float";
    case MaterialGraphSocketType::Material:
        return "Material";
    }
    return "Socket";
}

ImU32 socketColor(MaterialGraphSocketType type)
{
    switch (type) {
    case MaterialGraphSocketType::Color:
        return IM_COL32(210, 145, 90, 255);
    case MaterialGraphSocketType::Float:
        return IM_COL32(125, 190, 245, 255);
    case MaterialGraphSocketType::Material:
        return IM_COL32(170, 135, 235, 255);
    }
    return IM_COL32(180, 180, 180, 255);
}

bool drawName(const char* label, std::string& name)
{
    char buffer[128] = {};
    const size_t copyLength = std::min(name.size(), sizeof(buffer) - 1);
    std::copy_n(name.data(), copyLength, buffer);
    if (!ImGui::InputText(label, buffer, sizeof(buffer))) {
        return false;
    }
    name = buffer;
    return true;
}

float nodeHeight(const MaterialGraphNode& node)
{
    const float inputRows = static_cast<float>(materialGraphInputs(node.type).size());
    const float outputRows = static_cast<float>(materialGraphOutputs(node.type).size());
    float rows = std::max(inputRows, outputRows);
    switch (node.type) {
    case MaterialGraphNodeType::ColorConstant:
        rows += 2.0f;
        break;
    case MaterialGraphNodeType::FloatConstant:
        rows += 1.0f;
        break;
    default:
        break;
    }
    return TITLE_HEIGHT + 24.0f + std::max(1.0f, rows) * SOCKET_ROW_HEIGHT;
}

std::optional<MaterialSocketAnchor> findAnchor(const std::vector<MaterialSocketAnchor>& anchors, MaterialGraphNodeId node, const std::string& socket, bool output)
{
    for (const MaterialSocketAnchor& anchor : anchors) {
        if (anchor.node == node && anchor.socket == socket && anchor.output == output) {
            return anchor;
        }
    }
    return std::nullopt;
}

bool hasInputLink(const MaterialGraph& graph, MaterialGraphNodeId node, const std::string& socket)
{
    for (const MaterialGraphLink& link : graph.links()) {
        if (link.toNode == node && link.toSocket == socket) {
            return true;
        }
    }
    return false;
}

bool drawEmbeddedInput(MaterialGraph& graph, MaterialGraphNode& node, const MaterialGraphSocket& socket, ImVec2 position, float width, float zoom)
{
    if (socket.type == MaterialGraphSocketType::Material) {
        return false;
    }

    bool dirty = false;
    const bool linked = hasInputLink(graph, node.id, socket.name);
    ImGui::SetCursorScreenPos(position);
    ImGui::SetNextItemWidth(width);
    if (linked) {
        ImGui::BeginDisabled();
    }

    if (socket.type == MaterialGraphSocketType::Color) {
        glm::vec3* color = &node.color;
        if (socket.name == "b") {
            color = &node.secondaryColor;
        }
        dirty = ImGui::ColorEdit3(("##material-input-color-" + std::to_string(node.id) + socket.name).c_str(), &color->x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    } else if (socket.type == MaterialGraphSocketType::Float) {
        float* value = &node.value;
        if (socket.name == "roughness") {
            value = &node.roughness;
        } else if (socket.name == "metallic") {
            value = &node.metallic;
        } else if (socket.name == "emission") {
            value = &node.emission;
        }
        dirty = ImGui::DragFloat(("##material-input-float-" + std::to_string(node.id) + socket.name).c_str(), value, 0.01f, -100.0f, 100.0f);
    }

    if (linked) {
        ImGui::EndDisabled();
    }
    (void)zoom;
    return dirty && !linked;
}

void buildLayouts(
    MaterialGraph& graph,
    const ui::GraphCanvasFrame& frame,
    std::vector<MaterialNodeLayout>& layouts,
    std::vector<MaterialSocketAnchor>& anchors)
{
    std::vector<MaterialGraphNode*> nodes;
    for (MaterialGraphNode& node : graph.nodes()) {
        nodes.push_back(&node);
    }
    std::sort(nodes.begin(), nodes.end(), [](const MaterialGraphNode* left, const MaterialGraphNode* right) {
        return left->id < right->id;
    });

    for (std::size_t i = 0; i < nodes.size(); ++i) {
        MaterialGraphNode& node = *nodes[i];
        if (node.editorX == 0.0f && node.editorY == 0.0f && node.id != graph.outputNode()) {
            node.editorX = 40.0f + static_cast<float>(i % 3) * 260.0f;
            node.editorY = 50.0f + static_cast<float>(i / 3) * 150.0f;
        }
        if (node.id == graph.outputNode() && node.editorX == 0.0f && node.editorY == 0.0f) {
            node.editorX = 640.0f;
            node.editorY = 140.0f;
        }
        const ImVec2 position = ui::graphToScreen(frame, {node.editorX, node.editorY});
        const ImVec2 size = {ui::scaleValue(frame, NODE_WIDTH), ui::scaleValue(frame, nodeHeight(node))};
        layouts.push_back({node.id, &node, position, size});

        const std::vector<MaterialGraphSocket> inputs = materialGraphInputs(node.type);
        for (std::size_t input = 0; input < inputs.size(); ++input) {
            anchors.push_back({node.id, inputs[input].name, false, {position.x, position.y + ui::scaleValue(frame, TITLE_HEIGHT + SOCKET_LABEL_OFFSET_Y + static_cast<float>(input) * SOCKET_ROW_HEIGHT)}, inputs[input].type});
        }
        const std::vector<MaterialGraphSocket> outputs = materialGraphOutputs(node.type);
        for (std::size_t output = 0; output < outputs.size(); ++output) {
            anchors.push_back({node.id, outputs[output].name, true, {position.x + size.x, position.y + ui::scaleValue(frame, TITLE_HEIGHT + SOCKET_LABEL_OFFSET_Y + static_cast<float>(output) * SOCKET_ROW_HEIGHT)}, outputs[output].type});
        }
    }
}

bool drawLinks(MaterialGraph& graph, const std::vector<MaterialSocketAnchor>& anchors, const ui::GraphCanvasFrame& frame)
{
    bool dirty = false;
    for (const MaterialGraphLink& link : graph.links()) {
        const std::optional<MaterialSocketAnchor> from = findAnchor(anchors, link.fromNode, link.fromSocket, true);
        const std::optional<MaterialSocketAnchor> to = findAnchor(anchors, link.toNode, link.toSocket, false);
        if (!from || !to) {
            continue;
        }
        ui::drawGraphBezier(frame, from->position, to->position, socketColor(from->type));
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const ImVec2 mid = {(from->position.x + to->position.x) * 0.5f, (from->position.y + to->position.y) * 0.5f};
            const float radius = ui::scaleValue(frame, 28.0f);
            if (ui::distanceSquared(mouse, mid) <= radius * radius && graph.unlinkInput(link.toNode, link.toSocket)) {
                dirty = true;
            }
        }
    }
    return dirty;
}

MaterialGraphNodeId addMaterialNodeAt(MaterialGraph& graph, MaterialGraphNodeType type, ImVec2 graphPosition)
{
    const MaterialGraphNodeId nodeId = graph.createNode(type, displayName(type));
    if (MaterialGraphNode* node = graph.node(nodeId)) {
        node->editorX = graphPosition.x;
        node->editorY = graphPosition.y;
    }
    return nodeId;
}

bool drawMaterialNode(
    MaterialGraph& graph,
    const MaterialNodeLayout& layout,
    const std::vector<MaterialSocketAnchor>& anchors,
    bool& draggingLink,
    MaterialGraphNodeId& dragFromNode,
    std::string& dragFromSocket,
    const ui::GraphCanvasFrame& frame)
{
    bool dirty = false;
    MaterialGraphNode& node = *layout.node;
    const float zoom = frame.zoom;
    const bool selected = graph.selectedNode() == node.id;

    const std::string title = node.name + " #" + std::to_string(node.id);
    ui::GraphNodeStyle style;
    style.bodyColor = selected ? IM_COL32(58, 66, 84, 255) : IM_COL32(42, 45, 52, 255);
    style.titleColor = node.id == graph.outputNode() ? IM_COL32(86, 64, 112, 255) : IM_COL32(54, 58, 68, 255);
    style.borderColor = selected ? IM_COL32(120, 170, 255, 255) : IM_COL32(78, 82, 92, 255);
    style.borderThickness = selected ? 2.0f : 1.0f;
    ui::drawGraphNodeShell(frame, layout.position, layout.size, title, style);

    const float titleActionWidth = ui::graphNodeTitleActionWidth(frame, 1);
    ui::drawGraphNodeTitleDragRegion(frame, layout.position, layout.size, TITLE_HEIGHT, titleActionWidth, "material-node-title##" + std::to_string(node.id));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        graph.setSelectedNode(node.id);
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        node.editorX += delta.x / zoom;
        node.editorY += delta.y / zoom;
        dirty = true;
    }
    if (node.id == graph.outputNode()) {
        (void)ui::drawGraphNodeTitleActionButton(frame, layout.position, layout.size, 0, "X##material-delete-" + std::to_string(node.id), false);
    } else if (ui::drawGraphNodeTitleActionButton(frame, layout.position, layout.size, 0, "X##material-delete-" + std::to_string(node.id), true)) {
        dirty = graph.deleteNode(node.id) || dirty;
        return dirty;
    }

    const std::vector<MaterialGraphSocket> inputs = materialGraphInputs(node.type);
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        const MaterialGraphSocket& socket = inputs[i];
        const std::optional<MaterialSocketAnchor> anchor = findAnchor(anchors, node.id, socket.name, false);
        if (!anchor) {
            continue;
        }
        ui::drawGraphSocket(frame, anchor->position, socketColor(socket.type));
        const std::string label = socket.name + " : " + socketTypeName(socket.type);
        ui::drawGraphText(frame, {anchor->position.x + ui::scaleValue(frame, 10.0f), anchor->position.y - ui::scaleValue(frame, 7.0f)}, IM_COL32(220, 224, 230, 255), label);
        dirty = drawEmbeddedInput(
                    graph,
                    node,
                    socket,
                    {layout.position.x + ui::scaleValue(frame, 18.0f), anchor->position.y + ui::scaleValue(frame, EMBEDDED_INPUT_OFFSET_Y - SOCKET_LABEL_OFFSET_Y)},
                    layout.size.x - ui::scaleValue(frame, 36.0f),
                    zoom)
            || dirty;

        ImGui::SetCursorScreenPos({anchor->position.x - ui::scaleValue(frame, 11.0f), anchor->position.y - ui::scaleValue(frame, 11.0f)});
        ImGui::InvisibleButton(("material-input##" + std::to_string(node.id) + socket.name).c_str(), {ui::scaleValue(frame, 22.0f), ui::scaleValue(frame, 22.0f)});
        if (draggingLink && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) {
            if (graph.link(dragFromNode, dragFromSocket, node.id, socket.name)) {
                dirty = true;
            }
            draggingLink = false;
            dragFromNode = 0;
            dragFromSocket.clear();
        }
    }

    const std::vector<MaterialGraphSocket> outputs = materialGraphOutputs(node.type);
    for (std::size_t i = 0; i < outputs.size(); ++i) {
        const MaterialGraphSocket& socket = outputs[i];
        const std::optional<MaterialSocketAnchor> anchor = findAnchor(anchors, node.id, socket.name, true);
        if (!anchor) {
            continue;
        }
        ui::drawGraphSocket(frame, anchor->position, socketColor(socket.type));
        const std::string label = socket.name + " : " + socketTypeName(socket.type);
        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        ui::drawGraphText(frame, {anchor->position.x - ui::scaleValue(frame, 10.0f) - textSize.x * zoom, anchor->position.y - ui::scaleValue(frame, 7.0f)}, IM_COL32(220, 224, 230, 255), label);

        ImGui::SetCursorScreenPos({anchor->position.x - ui::scaleValue(frame, 11.0f), anchor->position.y - ui::scaleValue(frame, 11.0f)});
        ImGui::InvisibleButton(("material-output##" + std::to_string(node.id) + socket.name).c_str(), {ui::scaleValue(frame, 22.0f), ui::scaleValue(frame, 22.0f)});
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            draggingLink = true;
            dragFromNode = node.id;
            dragFromSocket = socket.name;
            graph.setSelectedNode(node.id);
        }
    }

    float y = layout.position.y + ui::scaleValue(frame, TITLE_HEIGHT + 26.0f + std::max(inputs.size(), outputs.size()) * SOCKET_ROW_HEIGHT);
    if (node.type == MaterialGraphNodeType::ColorConstant) {
        ImGui::SetCursorScreenPos({layout.position.x + ui::scaleValue(frame, 10.0f), y});
        ImGui::SetNextItemWidth(layout.size.x - ui::scaleValue(frame, 20.0f));
        if (ImGui::ColorEdit3(("##material-color-" + std::to_string(node.id)).c_str(), &node.color.x, ImGuiColorEditFlags_NoInputs)) {
            dirty = true;
        }
    } else if (node.type == MaterialGraphNodeType::FloatConstant) {
        ImGui::SetCursorScreenPos({layout.position.x + ui::scaleValue(frame, 10.0f), y});
        ImGui::SetNextItemWidth(layout.size.x - ui::scaleValue(frame, 20.0f));
        if (ImGui::DragFloat(("##material-float-" + std::to_string(node.id)).c_str(), &node.value, 0.01f, -100.0f, 100.0f)) {
            dirty = true;
        }
    }

    return dirty;
}

bool drawMaterialGraphCanvas(
    MaterialGraph& graph,
    float& panX,
    float& panY,
    float& zoom,
    bool& draggingLink,
    MaterialGraphNodeId& dragFromNode,
    std::string& dragFromSocket,
    float& addPopupGraphX,
    float& addPopupGraphY)
{
    bool dirty = false;
    ui::GraphCanvasFrame frame = ui::beginGraphCanvas("MaterialGraphCanvas", panX, panY, zoom, {360.0f, 300.0f}, MIN_ZOOM, MAX_ZOOM);
    ui::updateGraphCanvasView(frame, panX, panY, zoom, MIN_ZOOM, MAX_ZOOM);
    ui::drawGraphCanvasGrid(frame);

    std::vector<MaterialNodeLayout> layouts;
    std::vector<MaterialSocketAnchor> anchors;
    buildLayouts(graph, frame, layouts, anchors);
    dirty = drawLinks(graph, anchors, frame) || dirty;

    if (draggingLink) {
        const std::optional<MaterialSocketAnchor> from = findAnchor(anchors, dragFromNode, dragFromSocket, true);
        if (from) {
            ui::drawGraphBezier(frame, from->position, ImGui::GetIO().MousePos, IM_COL32(255, 210, 110, 255));
        }
    }

    for (const MaterialNodeLayout& layout : layouts) {
        dirty = drawMaterialNode(graph, layout, anchors, draggingLink, dragFromNode, dragFromSocket, frame) || dirty;
    }

    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        const MaterialGraphNodeId selected = graph.selectedNode();
        if (selected != graph.outputNode() && graph.deleteNode(selected)) {
            dirty = true;
        }
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered()) {
        const ImVec2 popupPosition = ui::screenToGraph(frame, ImGui::GetIO().MousePos);
        addPopupGraphX = popupPosition.x;
        addPopupGraphY = popupPosition.y;
        ImGui::OpenPopup("MaterialGraphAddPopup");
    }
    if (ImGui::BeginPopup("MaterialGraphAddPopup")) {
        const ImVec2 graphPosition = {addPopupGraphX, addPopupGraphY};
        if (ImGui::MenuItem("PBR Material")) {
            (void)addMaterialNodeAt(graph, MaterialGraphNodeType::PbrMaterial, graphPosition);
            dirty = true;
        }
        if (ImGui::MenuItem("Color")) {
            (void)addMaterialNodeAt(graph, MaterialGraphNodeType::ColorConstant, graphPosition);
            dirty = true;
        }
        if (ImGui::MenuItem("Float")) {
            (void)addMaterialNodeAt(graph, MaterialGraphNodeType::FloatConstant, graphPosition);
            dirty = true;
        }
        if (ImGui::MenuItem("Mix Color")) {
            (void)addMaterialNodeAt(graph, MaterialGraphNodeType::MixColor, graphPosition);
            dirty = true;
        }
        if (ImGui::MenuItem("Checker")) {
            (void)addMaterialNodeAt(graph, MaterialGraphNodeType::CheckerPattern, graphPosition);
            dirty = true;
        }
        if (ImGui::MenuItem("Value Noise")) {
            (void)addMaterialNodeAt(graph, MaterialGraphNodeType::ValueNoisePattern, graphPosition);
            dirty = true;
        }
        ImGui::EndPopup();
    }

    if (draggingLink && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        draggingLink = false;
        dragFromNode = 0;
        dragFromSocket.clear();
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
        graph.setSelectedNode(graph.outputNode());
    }

    ImGui::EndChild();
    return dirty;
}

void drawNodeProperties(MaterialGraph& graph, MaterialGraphNode& node, EditorDirtyState& dirty)
{
    if (drawName("Node Name", node.name)) {
        dirty.material = true;
    }
    ImGui::TextDisabled("%s", displayName(node.type));
    if (node.type != MaterialGraphNodeType::MaterialOutput && ImGui::Button("Delete Node")) {
        if (graph.deleteNode(node.id)) {
            dirty.material = true;
        }
    }
}

void addNodeButton(MaterialGraph& graph, MaterialGraphNodeType type, EditorDirtyState& dirty)
{
    if (ImGui::Button(displayName(type))) {
        (void)addMaterialNodeAt(graph, type, {80.0f, 80.0f});
        dirty.material = true;
    }
}

} // namespace

EditorDirtyState MaterialGraphPanel::draw(SdfGraph& graph)
{
    EditorDirtyState dirty;
    ImGui::Begin("Material Graph");

    if (m_selectedMaterial == 0 || graph.materials().material(m_selectedMaterial) == nullptr) {
        const auto& materials = graph.materials().materials();
        m_selectedMaterial = materials.empty() ? 0 : materials.front().id;
    }

    if (ImGui::BeginChild("material-assets", {220.0f, 0.0f}, true)) {
        if (ImGui::Button("+ Material")) {
            m_selectedMaterial = graph.materials().createMaterial("Material");
            dirty.material = true;
        }
        ImGui::Separator();

        MaterialId pendingDelete = 0;
        for (const MaterialDefinition& material : graph.materials().materials()) {
            ImGui::PushID(static_cast<int>(material.id));
            const bool selected = material.id == m_selectedMaterial;
            ImGui::ColorButton("##swatch", {material.material.albedo.x, material.material.albedo.y, material.material.albedo.z, 1.0f});
            ImGui::SameLine();
            if (ImGui::Selectable(material.name.c_str(), selected)) {
                m_selectedMaterial = material.id;
            }
            const bool canDelete = GraphSystem::canDeleteMaterial(graph, material.id);
            if (!canDelete) {
                ImGui::BeginDisabled();
            }
            if (ImGui::SmallButton("Delete")) {
                pendingDelete = material.id;
            }
            if (!canDelete) {
                ImGui::EndDisabled();
            }
            ImGui::PopID();
        }
        if (pendingDelete != 0 && GraphSystem::deleteMaterial(graph, pendingDelete)) {
            if (m_selectedMaterial == pendingDelete) {
                m_selectedMaterial = 0;
            }
            dirty.material = true;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    if (ImGui::BeginChild("material-graph", {0.0f, 0.0f}, true)) {
        MaterialDefinition* material = graph.materials().material(m_selectedMaterial);
        if (material != nullptr) {
            if (drawName("Material Name", material->name)) {
                (void)GraphSystem::renameMaterial(graph, material->id, material->name);
                dirty.material = true;
            }

            ImGui::SeparatorText("Add Node");
            addNodeButton(material->graph, MaterialGraphNodeType::PbrMaterial, dirty);
            ImGui::SameLine();
            addNodeButton(material->graph, MaterialGraphNodeType::ColorConstant, dirty);
            ImGui::SameLine();
            addNodeButton(material->graph, MaterialGraphNodeType::FloatConstant, dirty);
            addNodeButton(material->graph, MaterialGraphNodeType::MixColor, dirty);
            ImGui::SameLine();
            addNodeButton(material->graph, MaterialGraphNodeType::CheckerPattern, dirty);
            ImGui::SameLine();
            addNodeButton(material->graph, MaterialGraphNodeType::ValueNoisePattern, dirty);

            ImGui::SeparatorText("Graph");
            if (drawMaterialGraphCanvas(
                    material->graph,
                    m_canvasPanX,
                    m_canvasPanY,
                    m_canvasZoom,
                    m_draggingLink,
                    m_dragFromNode,
                    m_dragFromSocket,
                    m_addPopupGraphX,
                    m_addPopupGraphY)) {
                dirty.material = true;
            }

            ImGui::SeparatorText("Selected Node");
            if (MaterialGraphNode* node = material->graph.node(material->graph.selectedNode())) {
                drawNodeProperties(material->graph, *node, dirty);
            }
        } else {
            ImGui::TextUnformatted("No material selected.");
        }
    }
    ImGui::EndChild();

    ImGui::End();
    return dirty;
}

} // namespace sdf3d
