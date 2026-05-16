#include "sdf3d/ui/SceneOutliner.h"

#include "sdf3d/scene/SdfNodeDefinition.h"

#include <algorithm>
#include <string>
#include <vector>

#include <imgui.h>

namespace sdf3d {
namespace {

const char* graphNodeTypeName(SdfNodeType type)
{
    if (const SdfNodeDefinition* definition = sdfNodeDefinition(type)) {
        return definition->displayName.c_str();
    }

    return "Node";
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

bool activeOutputNodeExists(const SdfGraph& graph)
{
    const SdfGraphNode* outputNode = graph.node(graph.outputNode());
    return outputNode != nullptr && outputNode->payload.type == SdfNodeType::Output;
}

bool drawGraphControls(SdfGraph& graph)
{
    bool sceneDirty = false;

    if (!graph.nodes().empty()) {
        if (ImGui::Button("Clear Graph")) {
            std::vector<SdfGraphNodeId> idsToDelete;
            idsToDelete.reserve(graph.nodes().size());
            for (const auto& [id, node] : graph.nodes()) {
                (void)node;
                idsToDelete.push_back(id);
            }
            for (SdfGraphNodeId id : idsToDelete) {
                if (!graph.isOutputNode(id)) {
                    graph.deleteNode(id);
                }
            }
            sceneDirty = true;
        }
    }

    return sceneDirty;
}

bool drawAddOperandControl(SdfGraph& graph)
{
    SdfGraphNode* selectedGraphNode = graph.node(graph.selectedNode());
    if (selectedGraphNode == nullptr) {
        return false;
    }

    const char* secondarySocket = secondarySocketFor(selectedGraphNode->payload.type);
    if (secondarySocket == nullptr || !ImGui::Button("Add Operand")) {
        return false;
    }

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
    if (!activeOutputNodeExists(graph)) {
        graph.setOutputNode(operationNode);
    }
    return true;
}

std::vector<SdfGraphNodeId> sortedGraphNodeIds(const SdfGraph& graph)
{
    std::vector<SdfGraphNodeId> ids;
    ids.reserve(graph.nodes().size());
    for (const auto& [id, node] : graph.nodes()) {
        (void)node;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

bool SceneOutliner::draw(SceneGraph& sceneGraph)
{
    bool sceneDirty = false;
    SdfGraph& graph = sceneGraph.graph();

    if (ImGui::GetCurrentContext() != nullptr) {
        if (drawGraphControls(graph)) {
            sceneDirty = true;
        }

        if (!graph.nodes().empty()) {
            ImGui::SameLine();
            if (drawAddOperandControl(graph)) {
                sceneDirty = true;
            }
        }

        return sceneDirty;
    }

    ImGui::SeparatorText("Graph");
    if (graph.nodes().empty()) {
        ImGui::TextUnformatted("Empty graph");
    } else {
        std::vector<SdfGraphNodeId> ids = sortedGraphNodeIds(graph);

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

            const bool selected = graph.isNodeSelected(id);
            if (ImGui::Selectable(label.c_str(), selected)) {
                if (ImGui::GetIO().KeyShift) {
                    graph.toggleSelectedNode(id);
                } else {
                    graph.setSelectedNode(id);
                }
            }

            ImGui::SameLine();
            if (graph.isOutputNode(id)) {
                ImGui::BeginDisabled();
                ImGui::SmallButton(("Delete##" + std::to_string(id)).c_str());
                ImGui::EndDisabled();
            } else if (ImGui::SmallButton(("Delete##" + std::to_string(id)).c_str())) {
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
            sceneDirty = true;
        }

        if (ImGui::Button("Clear Graph")) {
            std::vector<SdfGraphNodeId> idsToDelete = ids;
            // AGENT: Clear uses public delete API so link/output/selection
            // cleanup stays centralized in SdfGraph while UI remains thin.
            for (SdfGraphNodeId id : idsToDelete) {
                if (!graph.isOutputNode(id)) {
                    graph.deleteNode(id);
                }
            }
            return true;
        }

        if (drawAddOperandControl(graph)) {
            sceneDirty = true;
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
                const SdfGraphNode* target = graph.node(toNode);
                if (target != nullptr && (target->payload.type == SdfNodeType::Output || !activeOutputNodeExists(graph))) {
                    graph.setOutputNode(toNode);
                }
                sceneDirty = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Unlink Input")) {
            if (graph.unlinkInput(toNode, toSocket)) {
                sceneDirty = true;
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

    return sceneDirty;
}

} // namespace sdf3d
