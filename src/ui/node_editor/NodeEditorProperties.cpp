#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include "sdf3d/scene/SdfNodeDefinition.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sdf3d::node_editor {
namespace {

constexpr float FIELD_HEIGHT = 20.0f;
constexpr float LABEL_WIDTH = 76.0f;

std::vector<std::string> orderedParameterKeys(const SdfNode& node, std::unordered_map<std::string, SdfParameterDefinition>& parameterDefinitions)
{
    std::vector<std::string> keys;
    std::unordered_set<std::string> addedKeys;

    if (const SdfNodeDefinition* definition = sdfNodeDefinition(node.type)) {
        for (const SdfParameterDefinition& parameter : definition->parameters) {
            parameterDefinitions.emplace(parameter.name, parameter);
            if (node.parameters.find(parameter.name) != node.parameters.end()) {
                keys.push_back(parameter.name);
                addedKeys.insert(parameter.name);
            }
        }
    }

    std::vector<std::string> customKeys;
    for (const auto& [key, value] : node.parameters) {
        (void)value;
        if (addedKeys.find(key) == addedKeys.end()) {
            customKeys.push_back(key);
        }
    }
    std::sort(customKeys.begin(), customKeys.end());
    keys.insert(keys.end(), customKeys.begin(), customKeys.end());
    return keys;
}

void drawLabel(const CanvasFrame& frame, ImVec2 position, const char* label)
{
    frame.drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * frame.zoom, position, IM_COL32(190, 196, 208, 255), label);
}

} // namespace

bool drawNodeInlineProperties(const GraphNodeLayout& layout, const CanvasFrame& frame)
{
    if (layout.node->editorPropertiesCollapsed) {
        return false;
    }

    SdfNode& node = layout.node->payload;
    bool sceneDirty = false;
    float y = layout.contentPosition.y;
    const float labelWidth = scaleValue(frame, LABEL_WIDTH);
    const float fieldX = layout.contentPosition.x + labelWidth;
    const float fieldWidth = layout.size.x - labelWidth - scaleValue(frame, 20.0f);
    const float rowHeight = scaleValue(frame, 24.0f);

    char nameBuffer[96] = {};
    const size_t copyLength = std::min(node.name.size(), sizeof(nameBuffer) - 1);
    std::copy_n(node.name.data(), copyLength, nameBuffer);
    drawLabel(frame, {layout.contentPosition.x, y + scaleValue(frame, 3.0f)}, "Name");
    ImGui::SetCursorScreenPos({fieldX, y});
    ImGui::SetNextItemWidth(fieldWidth);
    if (ImGui::InputText(("##node-name-" + std::to_string(layout.id)).c_str(), nameBuffer, sizeof(nameBuffer))) {
        node.name = nameBuffer;
        sceneDirty = true;
    }
    y += rowHeight;

    if (node.type == SdfNodeType::MaterialOverride) {
        drawLabel(frame, {layout.contentPosition.x, y + scaleValue(frame, 3.0f)}, "Albedo");
        ImGui::SetCursorScreenPos({fieldX, y});
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::ColorEdit3(("##node-albedo-" + std::to_string(layout.id)).c_str(), &node.material.albedo.x, ImGuiColorEditFlags_NoInputs)) {
            sceneDirty = true;
        }
        y += rowHeight;

        drawLabel(frame, {layout.contentPosition.x, y + scaleValue(frame, 3.0f)}, "Roughness");
        ImGui::SetCursorScreenPos({fieldX, y});
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##node-roughness-" + std::to_string(layout.id)).c_str(), &node.material.roughness, 0.01f, 0.0f, 1.0f, "%.2f")) {
            sceneDirty = true;
        }
        y += rowHeight;

        drawLabel(frame, {layout.contentPosition.x, y + scaleValue(frame, 3.0f)}, "Metallic");
        ImGui::SetCursorScreenPos({fieldX, y});
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##node-metallic-" + std::to_string(layout.id)).c_str(), &node.material.metallic, 0.01f, 0.0f, 1.0f, "%.2f")) {
            sceneDirty = true;
        }
        y += rowHeight;

        drawLabel(frame, {layout.contentPosition.x, y + scaleValue(frame, 3.0f)}, "Emission");
        ImGui::SetCursorScreenPos({fieldX, y});
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##node-emission-" + std::to_string(layout.id)).c_str(), &node.material.emission, 0.01f, 0.0f, 100.0f, "%.2f")) {
            sceneDirty = true;
        }
        y += rowHeight;
    }

    std::unordered_map<std::string, SdfParameterDefinition> parameterDefinitions;
    for (const std::string& key : orderedParameterKeys(node, parameterDefinitions)) {
        float& value = node.parameters[key];
        const auto definition = parameterDefinitions.find(key);
        const float step = definition != parameterDefinitions.end() ? definition->second.step : 0.01f;
        const float minValue = definition != parameterDefinitions.end() ? definition->second.minValue : 0.0f;
        const float maxValue = definition != parameterDefinitions.end() ? definition->second.maxValue : 0.0f;

        drawLabel(frame, {layout.contentPosition.x, y + scaleValue(frame, 3.0f)}, key.c_str());
        ImGui::SetCursorScreenPos({fieldX, y});
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##node-param-" + std::to_string(layout.id) + "-" + key).c_str(), &value, step, minValue, maxValue)) {
            sceneDirty = true;
        }
        y += rowHeight;
    }
    return sceneDirty;
}

} // namespace sdf3d::node_editor
