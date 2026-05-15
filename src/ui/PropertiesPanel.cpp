#include "sdf3d/ui/PropertiesPanel.h"

#include "sdf3d/scene/SdfNodeDefinition.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <imgui.h>

namespace sdf3d {

bool PropertiesPanel::draw(SceneGraph& sceneGraph)
{
    bool sceneDirty = false;

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
        return sceneDirty;
    }

    char nameBuffer[128] = {};
    const std::string& currentName = selected->name;
    const size_t copyLength = std::min(currentName.size(), sizeof(nameBuffer) - 1);
    std::copy_n(currentName.data(), copyLength, nameBuffer);

    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        selected->name = nameBuffer;
        sceneDirty = true;
    }

    ImGui::SeparatorText("Material");

    // AGENT: Material edits live beside generic float parameters so M4 users
    // can author per-node appearance before the renderer consumes material IDs.
    if (ImGui::ColorEdit3("Albedo", &selected->material.albedo.x)) {
        sceneDirty = true;
    }
    if (ImGui::DragFloat("Roughness", &selected->material.roughness, 0.01f, 0.0f, 1.0f)) {
        sceneDirty = true;
    }
    if (ImGui::DragFloat("Metallic", &selected->material.metallic, 0.01f, 0.0f, 1.0f)) {
        sceneDirty = true;
    }
    if (ImGui::DragFloat("Emission", &selected->material.emission, 0.01f, 0.0f, 100.0f)) {
        sceneDirty = true;
    }

    ImGui::SeparatorText("Parameters");

    if (selected->parameters.empty()) {
        ImGui::TextUnformatted("No editable parameters.");
        ImGui::End();
        return sceneDirty;
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
        if (addedKeys.find(key) == addedKeys.end()) {
            customKeys.push_back(key);
        }
    }
    std::sort(customKeys.begin(), customKeys.end());
    keys.insert(keys.end(), customKeys.begin(), customKeys.end());

    for (const std::string& key : keys) {
        float& value = selected->parameters[key];
        const auto definition = parameterDefinitions.find(key);
        const float step = definition != parameterDefinitions.end() ? definition->second.step : 0.01f;
        const float minValue = definition != parameterDefinitions.end() ? definition->second.minValue : 0.0f;
        const float maxValue = definition != parameterDefinitions.end() ? definition->second.maxValue : 0.0f;
        if (ImGui::DragFloat(key.c_str(), &value, step, minValue, maxValue)) {
            sceneDirty = true;
        }
    }

    ImGui::End();
    return sceneDirty;
}

} // namespace sdf3d
