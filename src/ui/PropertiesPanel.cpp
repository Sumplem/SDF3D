#include "sdf3d/ui/PropertiesPanel.h"

#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/scene/SdfRotationParams.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <imgui.h>

namespace sdf3d {

namespace {

constexpr float enabledThreshold = 0.5f;
constexpr float disabledValue = 0.0f;
constexpr float enabledValue = 1.0f;
constexpr int axisX = 0;
constexpr int axisY = 1;
constexpr int axisZ = 2;
constexpr int axisCount = 3;

bool isBooleanParameter(const SdfParameterDefinition& definition)
{
    return definition.minValue == disabledValue && definition.maxValue == enabledValue && definition.step == enabledValue;
}

bool drawBoolParameter(const std::string& key, float& value)
{
    bool enabled = value >= enabledThreshold;
    if (!ImGui::Checkbox(key.c_str(), &enabled)) {
        return false;
    }

    value = enabled ? enabledValue : disabledValue;
    return true;
}

bool drawAxisParameter(float& value)
{
    const char* axisLabels[] = {"X", "Y", "Z"};
    int axis = static_cast<int>(std::clamp(value, static_cast<float>(axisX), static_cast<float>(axisZ)) + enabledThreshold);
    if (!ImGui::Combo("Axis", &axis, axisLabels, axisCount)) {
        return false;
    }

    value = static_cast<float>(axis);
    return true;
}

} // namespace

EditorDirtyState PropertiesPanel::draw(SceneGraph& sceneGraph)
{
    EditorDirtyState dirty;

    ImGui::Begin("Properties");

    SdfNode* selected = nullptr;
    if (sceneGraph.graph().selectedNodes().size() > 1) {
        ImGui::Text("Multiple nodes selected: %d", static_cast<int>(sceneGraph.graph().selectedNodes().size()));
        ImGui::End();
        return dirty;
    }

    SdfGraphNode* selectedGraphNode = sceneGraph.graph().node(sceneGraph.graph().selectedNode());
    if (selectedGraphNode != nullptr) {
        selected = &selectedGraphNode->payload;
    } else if (sceneGraph.selectedNode()) {
        selected = sceneGraph.selectedNode().get();
    }

    if (selected == nullptr) {
        ImGui::TextUnformatted("No node selected.");
        ImGui::End();
        return dirty;
    }

    char nameBuffer[128] = {};
    const std::string& currentName = selected->name;
    const size_t copyLength = std::min(currentName.size(), sizeof(nameBuffer) - 1);
    std::copy_n(currentName.data(), copyLength, nameBuffer);

    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        selected->name = nameBuffer;
        dirty.scene = true;
    }

    if (selected->type == SdfNodeType::MaterialOverride) {
        ImGui::SeparatorText("Material");

        if (ImGui::ColorEdit3("Albedo", &selected->material.albedo.x)) {
            dirty.material = true;
        }
        if (ImGui::DragFloat("Roughness", &selected->material.roughness, 0.01f, 0.0f, 1.0f)) {
            dirty.material = true;
        }
        if (ImGui::DragFloat("Metallic", &selected->material.metallic, 0.01f, 0.0f, 1.0f)) {
            dirty.material = true;
        }
        if (ImGui::DragFloat("Emission", &selected->material.emission, 0.01f, 0.0f, 100.0f)) {
            dirty.material = true;
        }
    }

    ImGui::SeparatorText("Parameters");

    if (selected->parameters.empty()) {
        ImGui::TextUnformatted("No editable parameters.");
        ImGui::End();
        return dirty;
    }

    std::vector<std::string> keys;
    keys.reserve(selected->parameters.size());
    std::unordered_set<std::string> addedKeys;
    std::unordered_map<std::string, SdfParameterDefinition> parameterDefinitions;

    // AGENT: Metadata order matches constructor/default intent; custom
    // parameters still render after known parameters for forward compatibility.
    if (const SdfNodeDefinition* definition = sdfNodeDefinition(selected->type)) {
        for (const SdfParameterDefinition& parameter : definition->parameters) {
            parameterDefinitions.emplace(parameter.name, parameter);
            if (selected->parameters.find(parameter.name) != selected->parameters.end()) {
                keys.push_back(parameter.name);
                addedKeys.insert(parameter.name);
            }
        }
    }

    std::vector<std::string> customKeys;
    for (const auto& [key, value] : selected->parameters) {
        (void)value;
        if (selected->type == SdfNodeType::Rotate && isHiddenRotationQuaternionParameter(key)) {
            continue;
        }
        if (addedKeys.find(key) == addedKeys.end()) {
            customKeys.push_back(key);
        }
    }
    std::sort(customKeys.begin(), customKeys.end());
    keys.insert(keys.end(), customKeys.begin(), customKeys.end());

    for (const std::string& key : keys) {
        float& value = selected->parameters[key];
        const auto definition = parameterDefinitions.find(key);
        if (definition != parameterDefinitions.end() && isBooleanParameter(definition->second)) {
            if (drawBoolParameter(key, value)) {
                dirty.scene = true;
            }
            continue;
        }
        if ((selected->type == SdfNodeType::Twist || selected->type == SdfNodeType::Bend) && key == "axis") {
            if (drawAxisParameter(value)) {
                dirty.scene = true;
            }
            continue;
        }

        const float step = definition != parameterDefinitions.end() ? definition->second.step : 0.01f;
        const float minValue = definition != parameterDefinitions.end() ? definition->second.minValue : 0.0f;
        const float maxValue = definition != parameterDefinitions.end() ? definition->second.maxValue : 0.0f;
        if (ImGui::DragFloat(key.c_str(), &value, step, minValue, maxValue)) {
            if (selected->type == SdfNodeType::Rotate && (key == "xDegrees" || key == "yDegrees" || key == "zDegrees")) {
                storeRotationQuaternion(*selected, rotationQuaternionFromEulerDegrees({
                    selected->parameters["xDegrees"],
                    selected->parameters["yDegrees"],
                    selected->parameters["zDegrees"],
                }));
            }
            dirty.scene = true;
        }
    }

    ImGui::End();
    return dirty;
}

} // namespace sdf3d
