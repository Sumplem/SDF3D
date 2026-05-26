#include "sdf3d/ui/PropertiesPanel.h"

#include "sdf3d/scene/SdfNodeDefinition.h"
#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/systems/GraphSystem.h"

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
constexpr int minInstanceBatchCount = 1;
constexpr int maxInstanceBatchCount = 10000;
constexpr float instancePositionDragSpeed = 0.01f;
constexpr float instancePositionMin = -100.0f;
constexpr float instancePositionMax = 100.0f;

bool drawBoolParameter(const std::string& key, float& value)
{
    bool enabled = value >= enabledThreshold;
    if (!ImGui::Checkbox(key.c_str(), &enabled)) {
        return false;
    }

    value = enabled ? enabledValue : disabledValue;
    return true;
}

bool drawEnumParameter(const std::string& key, const SdfParameterDefinition& definition, float& value)
{
    if (definition.enumValues.empty()) {
        return false;
    }

    const int currentValue = static_cast<int>(value);
    int selectedIndex = 0;
    std::vector<const char*> labels;
    labels.reserve(definition.enumValues.size());
    for (int i = 0; i < static_cast<int>(definition.enumValues.size()); ++i) {
        labels.push_back(definition.enumValues[i].name.c_str());
        if (definition.enumValues[i].value == currentValue) {
            selectedIndex = i;
        }
    }

    if (!ImGui::Combo(key.c_str(), &selectedIndex, labels.data(), static_cast<int>(labels.size()))) {
        return false;
    }

    value = static_cast<float>(definition.enumValues[selectedIndex].value);
    return true;
}

bool drawMaterialTypeCombo(SdfMaterial& material)
{
    const char* labels[] = {"Solid", "Checker", "Value Noise"};
    int type = static_cast<int>(material.type);
    if (!ImGui::Combo("Type", &type, labels, 3)) {
        return false;
    }

    material.type = static_cast<SdfMaterialType>(type);
    return true;
}

const char* materialTypeName(SdfMaterialType type)
{
    switch (type) {
    case SdfMaterialType::Checker:
        return "Checker";
    case SdfMaterialType::ValueNoise:
        return "Value Noise";
    case SdfMaterialType::Solid:
        return "Solid";
    }
    return "Solid";
}

bool drawMaterialAssignmentCombo(SdfGraph& graph, SdfGraphNodeId nodeId, MaterialId currentMaterialId)
{
    const MaterialDefinition* current = graph.materials().material(currentMaterialId);
    const std::string preview = current == nullptr ? "None" : current->name;
    if (!ImGui::BeginCombo("Material", preview.c_str())) {
        return false;
    }

    bool changed = false;
    for (const MaterialDefinition& material : graph.materials().materials()) {
        const bool selected = material.id == currentMaterialId;
        const std::string label = material.name + " #" + std::to_string(material.id);
        if (ImGui::Selectable(label.c_str(), selected)) {
            changed = GraphSystem::assignMaterialToNode(graph, nodeId, material.id);
        }
        if (selected) {
            ImGui::SetItemDefaultFocus();
        }
    }

    ImGui::EndCombo();
    return changed;
}

EditorDirtyState drawMaterialPalette(SdfGraph& graph)
{
    EditorDirtyState dirty;
    ImGui::SeparatorText("Material Palette");

    const std::vector<MaterialDefinition>& materials = graph.materials().materials();
    if (materials.empty()) {
        ImGui::TextUnformatted("No registry materials.");
        return dirty;
    }

    MaterialId pendingDelete = 0;
    for (const MaterialDefinition& material : materials) {
        ImGui::PushID(static_cast<int>(material.id));
        ImGui::ColorButton("##material-swatch", {material.material.albedo.x, material.material.albedo.y, material.material.albedo.z, 1.0f});
        ImGui::SameLine();

        char nameBuffer[96] = {};
        const size_t copyLength = std::min(material.name.size(), sizeof(nameBuffer) - 1);
        std::copy_n(material.name.data(), copyLength, nameBuffer);
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::InputText("##material-name", nameBuffer, sizeof(nameBuffer))) {
            if (GraphSystem::renameMaterial(graph, material.id, nameBuffer)) {
                dirty.scene = true;
            }
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(materialTypeName(material.material.type));
        ImGui::SameLine();
        ImGui::Text("ID %llu", static_cast<unsigned long long>(material.id));
        ImGui::SameLine();

        const bool canDelete = GraphSystem::canDeleteMaterial(graph, material.id);
        if (!canDelete) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Delete")) {
            pendingDelete = material.id;
        }
        if (!canDelete) {
            ImGui::EndDisabled();
        }
        if (!canDelete && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Delete material node first.");
        }
        ImGui::PopID();
    }

    if (pendingDelete != 0 && GraphSystem::deleteMaterial(graph, pendingDelete)) {
        dirty.scene = true;
    }
    return dirty;
}

bool drawInstancePositions(SdfNode& node)
{
    bool changed = false;
    if (node.type != SdfNodeType::SphereInstances) {
        return changed;
    }

    static int batchCount = 10;
    static glm::vec3 batchSpacing = {1.0f, 0.0f, 0.0f};

    ImGui::SeparatorText("Instances");
    ImGui::Text("Count: %d", static_cast<int>(node.instancePositions.size()));

    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("Add Count", &batchCount)) {
        batchCount = std::clamp(batchCount, minInstanceBatchCount, maxInstanceBatchCount);
    }
    ImGui::SetNextItemWidth(180.0f);
    (void)ImGui::DragFloat3("Spacing", &batchSpacing.x, instancePositionDragSpeed, instancePositionMin, instancePositionMax);

    if (ImGui::Button("Add Batch")) {
        const glm::vec3 origin = node.instancePositions.empty()
            ? glm::vec3{0.0f, 0.0f, 0.0f}
            : node.instancePositions.back() + batchSpacing;
        node.instancePositions.reserve(node.instancePositions.size() + static_cast<size_t>(batchCount));
        for (int i = 0; i < batchCount; ++i) {
            node.instancePositions.push_back(origin + batchSpacing * static_cast<float>(i));
        }
        changed = true;
    }
    ImGui::SameLine();
    if (!node.instancePositions.empty() && ImGui::Button("Clear Instances")) {
        node.instancePositions.clear();
        changed = true;
    }

    int deleteIndex = -1;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(node.instancePositions.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(180.0f);
            changed = ImGui::DragFloat3("Position", &node.instancePositions[i].x, instancePositionDragSpeed, instancePositionMin, instancePositionMax) || changed;
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                deleteIndex = i;
            }
            ImGui::PopID();
        }
    }

    if (deleteIndex >= 0) {
        node.instancePositions.erase(node.instancePositions.begin() + deleteIndex);
        changed = true;
    }
    if (ImGui::Button("Add Instance")) {
        const glm::vec3 position = node.instancePositions.empty()
            ? glm::vec3{0.0f, 0.0f, 0.0f}
            : node.instancePositions.back() + batchSpacing;
        node.instancePositions.push_back(position);
        changed = true;
    }

    return changed;
}

} // namespace

EditorDirtyState PropertiesPanel::draw(SdfGraph& graph, GraphGroupRegistry& groups)
{
    EditorDirtyState dirty;

    ImGui::Begin("Properties");

    SdfNode* selected = nullptr;
    if (graph.selectedNodes().size() > 1) {
        ImGui::Text("Multiple nodes selected: %d", static_cast<int>(graph.selectedNodes().size()));
        ImGui::End();
        return dirty;
    }

    SdfGraphNode* selectedGraphNode = graph.node(graph.selectedNode());
    if (selectedGraphNode != nullptr) {
        selected = &selectedGraphNode->payload;
    }

    if (selected == nullptr) {
        ImGui::TextUnformatted("No node selected.");
        ImGui::End();
        return dirty;
    }

    char nameBuffer[128] = {};
    const std::string currentName = GraphSystem::displayNameForNode(*selectedGraphNode, groups);
    const size_t copyLength = std::min(currentName.size(), sizeof(nameBuffer) - 1);
    std::copy_n(currentName.data(), copyLength, nameBuffer);

    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        (void)GraphSystem::renameNode(graph, groups, selectedGraphNode->id, nameBuffer);
        dirty.scene = true;
    }

    if (selected->type == SdfNodeType::MaterialOverride) {
        ImGui::SeparatorText("Material");
        if (graph.materials().materials().empty()) {
            ImGui::TextUnformatted("No registry materials.");
        } else if (drawMaterialAssignmentCombo(graph, selectedGraphNode->id, selected->materialId)) {
            dirty.scene = true;
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
            if (parameter.visibility == SdfParameterVisibility::Hidden) {
                continue;
            }
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
        if (!isSdfParameterVisible(selected->type, key)) {
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
        if (definition != parameterDefinitions.end() && definition->second.type == SdfParameterType::Bool) {
            if (drawBoolParameter(key, value)) {
                dirty.scene = true;
            }
            continue;
        }
        if (definition != parameterDefinitions.end() && definition->second.type == SdfParameterType::Enum) {
            if (drawEnumParameter(key, definition->second, value)) {
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
            dirty.params = true;
        }
    }

    if (drawInstancePositions(*selected)) {
        dirty.params = true;
    }

    ImGui::End();
    return dirty;
}

} // namespace sdf3d
