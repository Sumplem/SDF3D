#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/ui/node_editor/NodeEditorProperties.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sdf3d::node_editor {
namespace {

constexpr float FIELD_HEIGHT = 20.0f;
constexpr float LABEL_WIDTH = 76.0f;
constexpr float enabledThreshold = 0.5f;
constexpr float disabledValue = 0.0f;
constexpr float enabledValue = 1.0f;
constexpr int axisX = 0;
constexpr int axisZ = 2;
constexpr int axisCount = 3;
constexpr float defaultWindowFontScale = 1.0f;

class ScopedWidgetZoom {
public:
    explicit ScopedWidgetZoom(float zoom)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::SetWindowFontScale(zoom);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {style.FramePadding.x * zoom, style.FramePadding.y * zoom});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, {style.ItemInnerSpacing.x * zoom, style.ItemInnerSpacing.y * zoom});
        ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, style.GrabMinSize * zoom);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, style.FrameRounding * zoom);
    }

    ~ScopedWidgetZoom()
    {
        ImGui::PopStyleVar(4);
        ImGui::SetWindowFontScale(defaultWindowFontScale);
    }
};

bool isBooleanParameter(const SdfParameterDefinition& definition)
{
    return definition.minValue == disabledValue && definition.maxValue == enabledValue && definition.step == enabledValue;
}

bool drawBoolParameter(const std::string& id, float& value)
{
    bool enabled = value >= enabledThreshold;
    if (!ImGui::Checkbox(id.c_str(), &enabled)) {
        return false;
    }

    value = enabled ? enabledValue : disabledValue;
    return true;
}

bool drawAxisParameter(const std::string& id, float& value)
{
    const char* axisLabels[] = {"X", "Y", "Z"};
    int axis = static_cast<int>(std::clamp(value, static_cast<float>(axisX), static_cast<float>(axisZ)) + enabledThreshold);
    if (!ImGui::Combo(id.c_str(), &axis, axisLabels, axisCount)) {
        return false;
    }

    value = static_cast<float>(axis);
    return true;
}

std::vector<std::string> orderedParameterKeys(const SdfNode& node, std::unordered_map<std::string, SdfParameterDefinition>& parameterDefinitions)
{
    std::vector<std::string> keys;
    std::unordered_set<std::string> addedKeys;

    if (const SdfNodeDefinition* definition = sdfNodeDefinition(node.type)) {
        for (const SdfParameterDefinition& parameter : definition->parameters) {
            if (parameter.visibility == SdfParameterVisibility::Hidden) {
                continue;
            }
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
        if (!isInlinePropertyParameterVisible(node, key)) {
            continue;
        }
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

EditorDirtyState drawNodeInlineProperties(const GraphNodeLayout& layout, const CanvasFrame& frame)
{
    if (layout.node->editorPropertiesCollapsed) {
        return {};
    }

    SdfNode& node = layout.node->payload;
    EditorDirtyState dirty;
    float y = layout.contentPosition.y;
    const float labelWidth = scaleValue(frame, LABEL_WIDTH);
    const float fieldX = layout.contentPosition.x + labelWidth;
    const float fieldWidth = layout.size.x - labelWidth - scaleValue(frame, 20.0f);
    const float rowHeight = scaleValue(frame, 24.0f);
    const ScopedWidgetZoom widgetZoom(frame.zoom);

    char nameBuffer[96] = {};
    const size_t copyLength = std::min(node.name.size(), sizeof(nameBuffer) - 1);
    std::copy_n(node.name.data(), copyLength, nameBuffer);
    drawLabel(frame, {layout.contentPosition.x, y + scaleValue(frame, 3.0f)}, "Name");
    ImGui::SetCursorScreenPos({fieldX, y});
    ImGui::SetNextItemWidth(fieldWidth);
    if (ImGui::InputText(("##node-name-" + std::to_string(layout.id)).c_str(), nameBuffer, sizeof(nameBuffer))) {
        node.name = nameBuffer;
        dirty.scene = true;
    }
    y += rowHeight;

    if (node.type == SdfNodeType::MaterialOverride) {
        drawLabel(frame, {layout.contentPosition.x, y + scaleValue(frame, 3.0f)}, "Albedo");
        ImGui::SetCursorScreenPos({fieldX, y});
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::ColorEdit3(("##node-albedo-" + std::to_string(layout.id)).c_str(), &node.material.albedo.x, ImGuiColorEditFlags_NoInputs)) {
            dirty.material = true;
        }
        y += rowHeight;

        drawLabel(frame, {layout.contentPosition.x, y + scaleValue(frame, 3.0f)}, "Roughness");
        ImGui::SetCursorScreenPos({fieldX, y});
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##node-roughness-" + std::to_string(layout.id)).c_str(), &node.material.roughness, 0.01f, 0.0f, 1.0f, "%.2f")) {
            dirty.material = true;
        }
        y += rowHeight;

        drawLabel(frame, {layout.contentPosition.x, y + scaleValue(frame, 3.0f)}, "Metallic");
        ImGui::SetCursorScreenPos({fieldX, y});
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##node-metallic-" + std::to_string(layout.id)).c_str(), &node.material.metallic, 0.01f, 0.0f, 1.0f, "%.2f")) {
            dirty.material = true;
        }
        y += rowHeight;

        drawLabel(frame, {layout.contentPosition.x, y + scaleValue(frame, 3.0f)}, "Emission");
        ImGui::SetCursorScreenPos({fieldX, y});
        ImGui::SetNextItemWidth(fieldWidth);
        if (ImGui::DragFloat(("##node-emission-" + std::to_string(layout.id)).c_str(), &node.material.emission, 0.01f, 0.0f, 100.0f, "%.2f")) {
            dirty.material = true;
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
        const std::string id = "##node-param-" + std::to_string(layout.id) + "-" + key;
        bool changed = false;
        if (definition != parameterDefinitions.end() && isBooleanParameter(definition->second)) {
            changed = drawBoolParameter(id, value);
        } else if ((node.type == SdfNodeType::Twist || node.type == SdfNodeType::Bend) && key == "axis") {
            changed = drawAxisParameter(id, value);
        } else {
            changed = ImGui::DragFloat(id.c_str(), &value, step, minValue, maxValue);
        }
        if (changed) {
            if (node.type == SdfNodeType::Rotate && (key == "xDegrees" || key == "yDegrees" || key == "zDegrees")) {
                storeRotationQuaternion(node, rotationQuaternionFromEulerDegrees({
                    node.parameters["xDegrees"],
                    node.parameters["yDegrees"],
                    node.parameters["zDegrees"],
                }));
            }
            dirty.scene = true;
        }
        y += rowHeight;
    }
    return dirty;
}

} // namespace sdf3d::node_editor
