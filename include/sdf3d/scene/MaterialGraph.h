#pragma once

#include "sdf3d/components/SdfMaterial.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace sdf3d {

using MaterialGraphNodeId = uint64_t;

enum class MaterialGraphNodeType {
    PbrMaterial,
    ColorConstant,
    FloatConstant,
    MixColor,
    MultiplyColor,
    ColorRamp,
    AddColor,
    SubtractColor,
    PowerFloat,
    ClampFloat,
    CheckerPattern,
    ValueNoise,
    ValueNoisePattern,
    MaterialOutput,
};

enum class MaterialGraphSocketType {
    Color,
    Float,
    Material,
};

struct MaterialGraphSocket {
    std::string name;
    MaterialGraphSocketType type = MaterialGraphSocketType::Float;
};

struct MaterialGraphLink {
    MaterialGraphNodeId fromNode = 0;
    std::string fromSocket;
    MaterialGraphNodeId toNode = 0;
    std::string toSocket;
};

struct MaterialGraphNode {
    MaterialGraphNodeId id = 0;
    MaterialGraphNodeType type = MaterialGraphNodeType::ColorConstant;
    std::string name;
    glm::vec3 color = {0.8f, 0.8f, 0.8f};
    glm::vec3 secondaryColor = {0.08f, 0.08f, 0.08f};
    float value = 0.0f;
    float secondaryValue = 0.0f;
    float tertiaryValue = 1.0f;
    float roughness = 0.5f;
    float metallic = 0.0f;
    float emission = 0.0f;
    float editorX = 0.0f;
    float editorY = 0.0f;
    bool editorCollapsed = false;
    bool selected = false;
};

inline const char* materialGraphNodeTypeName(MaterialGraphNodeType type)
{
    switch (type) {
    case MaterialGraphNodeType::PbrMaterial:
        return "PbrMaterial";
    case MaterialGraphNodeType::ColorConstant:
        return "ColorConstant";
    case MaterialGraphNodeType::FloatConstant:
        return "FloatConstant";
    case MaterialGraphNodeType::MixColor:
        return "MixColor";
    case MaterialGraphNodeType::MultiplyColor:
        return "MultiplyColor";
    case MaterialGraphNodeType::ColorRamp:
        return "ColorRamp";
    case MaterialGraphNodeType::AddColor:
        return "AddColor";
    case MaterialGraphNodeType::SubtractColor:
        return "SubtractColor";
    case MaterialGraphNodeType::PowerFloat:
        return "PowerFloat";
    case MaterialGraphNodeType::ClampFloat:
        return "ClampFloat";
    case MaterialGraphNodeType::CheckerPattern:
        return "CheckerPattern";
    case MaterialGraphNodeType::ValueNoise:
        return "ValueNoise";
    case MaterialGraphNodeType::ValueNoisePattern:
        return "ValueNoisePattern";
    case MaterialGraphNodeType::MaterialOutput:
        return "MaterialOutput";
    }
    return "Unknown";
}

inline std::vector<MaterialGraphSocket> materialGraphInputs(MaterialGraphNodeType type)
{
    switch (type) {
    case MaterialGraphNodeType::PbrMaterial:
        return {{"albedo", MaterialGraphSocketType::Color}, {"roughness", MaterialGraphSocketType::Float}, {"metallic", MaterialGraphSocketType::Float}, {"emission", MaterialGraphSocketType::Float}};
    case MaterialGraphNodeType::MixColor:
    case MaterialGraphNodeType::MultiplyColor:
        return {{"a", MaterialGraphSocketType::Color}, {"b", MaterialGraphSocketType::Color}, {"factor", MaterialGraphSocketType::Float}};
    case MaterialGraphNodeType::ColorRamp:
        return {{"factor", MaterialGraphSocketType::Float}};
    case MaterialGraphNodeType::AddColor:
    case MaterialGraphNodeType::SubtractColor:
        return {{"a", MaterialGraphSocketType::Color}, {"b", MaterialGraphSocketType::Color}};
    case MaterialGraphNodeType::PowerFloat:
        return {{"base", MaterialGraphSocketType::Float}, {"exponent", MaterialGraphSocketType::Float}};
    case MaterialGraphNodeType::ClampFloat:
        return {{"value", MaterialGraphSocketType::Float}, {"min", MaterialGraphSocketType::Float}, {"max", MaterialGraphSocketType::Float}};
    case MaterialGraphNodeType::CheckerPattern:
    case MaterialGraphNodeType::ValueNoisePattern:
        return {{"a", MaterialGraphSocketType::Color}, {"b", MaterialGraphSocketType::Color}, {"scale", MaterialGraphSocketType::Float}};
    case MaterialGraphNodeType::ValueNoise:
        return {{"scale", MaterialGraphSocketType::Float}};
    case MaterialGraphNodeType::MaterialOutput:
        return {{"material", MaterialGraphSocketType::Material}};
    default:
        return {};
    }
}

inline std::vector<MaterialGraphSocket> materialGraphOutputs(MaterialGraphNodeType type)
{
    switch (type) {
    case MaterialGraphNodeType::PbrMaterial:
        return {{"material", MaterialGraphSocketType::Material}};
    case MaterialGraphNodeType::ColorConstant:
    case MaterialGraphNodeType::MixColor:
    case MaterialGraphNodeType::MultiplyColor:
    case MaterialGraphNodeType::ColorRamp:
    case MaterialGraphNodeType::AddColor:
    case MaterialGraphNodeType::SubtractColor:
    case MaterialGraphNodeType::ValueNoisePattern:
        return {{"color", MaterialGraphSocketType::Color}};
    case MaterialGraphNodeType::CheckerPattern:
        return {{"color", MaterialGraphSocketType::Color}, {"factor", MaterialGraphSocketType::Float}};
    case MaterialGraphNodeType::FloatConstant:
    case MaterialGraphNodeType::PowerFloat:
    case MaterialGraphNodeType::ClampFloat:
        return {{"value", MaterialGraphSocketType::Float}};
    case MaterialGraphNodeType::ValueNoise:
        return {{"factor", MaterialGraphSocketType::Float}};
    default:
        return {};
    }
}

inline const MaterialGraphSocket* findMaterialGraphSocket(const std::vector<MaterialGraphSocket>& sockets, const std::string& name)
{
    const auto it = std::find_if(sockets.begin(), sockets.end(), [&](const MaterialGraphSocket& socket) {
        return socket.name == name;
    });
    return it == sockets.end() ? nullptr : &(*it);
}

class MaterialGraph {
public:
    MaterialGraph()
    {
        m_outputNode = createNode(MaterialGraphNodeType::MaterialOutput, "Material Output");
    }

    MaterialGraphNodeId createNode(MaterialGraphNodeType type, std::string name = {})
    {
        const MaterialGraphNodeId id = m_nextId++;
        MaterialGraphNode node;
        node.id = id;
        node.type = type;
        node.name = name.empty() ? materialGraphNodeTypeName(type) : std::move(name);
        if (type == MaterialGraphNodeType::FloatConstant) {
            node.value = 1.0f;
        } else if (type == MaterialGraphNodeType::CheckerPattern || type == MaterialGraphNodeType::ValueNoisePattern || type == MaterialGraphNodeType::ValueNoise) {
            node.value = 4.0f;
        } else if (type == MaterialGraphNodeType::MixColor) {
            node.secondaryColor = {0.2f, 0.2f, 0.2f};
            node.value = 0.5f;
        } else if (type == MaterialGraphNodeType::MultiplyColor) {
            node.secondaryColor = {1.0f, 1.0f, 1.0f};
            node.value = 1.0f;
        } else if (type == MaterialGraphNodeType::ColorRamp) {
            node.secondaryColor = {1.0f, 1.0f, 1.0f};
            node.value = 0.5f;
        } else if (type == MaterialGraphNodeType::AddColor || type == MaterialGraphNodeType::SubtractColor) {
            node.secondaryColor = {0.2f, 0.2f, 0.2f};
        } else if (type == MaterialGraphNodeType::PowerFloat) {
            node.value = 1.0f;
            node.secondaryValue = 1.0f;
        } else if (type == MaterialGraphNodeType::ClampFloat) {
            node.value = 0.0f;
            node.secondaryValue = 0.0f;
            node.tertiaryValue = 1.0f;
        }
        m_nodes.push_back(node);
        setSelectedNode(id);
        return id;
    }

    bool deleteNode(MaterialGraphNodeId id)
    {
        if (id == 0 || id == m_outputNode) {
            return false;
        }
        const auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [id](const MaterialGraphNode& node) {
            return node.id == id;
        });
        if (it == m_nodes.end()) {
            return false;
        }
        m_nodes.erase(it);
        m_links.erase(std::remove_if(m_links.begin(), m_links.end(), [id](const MaterialGraphLink& link) {
            return link.fromNode == id || link.toNode == id;
        }), m_links.end());
        if (m_selectedNode == id) {
            setSelectedNode(m_outputNode);
        }
        return true;
    }

    bool link(MaterialGraphNodeId fromNode, std::string fromSocket, MaterialGraphNodeId toNode, std::string toSocket)
    {
        MaterialGraphNode* from = node(fromNode);
        MaterialGraphNode* to = node(toNode);
        if (from == nullptr || to == nullptr || fromNode == toNode) {
            return false;
        }
        const std::vector<MaterialGraphSocket> outputs = materialGraphOutputs(from->type);
        const std::vector<MaterialGraphSocket> inputs = materialGraphInputs(to->type);
        const MaterialGraphSocket* output = findMaterialGraphSocket(outputs, fromSocket);
        const MaterialGraphSocket* input = findMaterialGraphSocket(inputs, toSocket);
        if (output == nullptr || input == nullptr || output->type != input->type) {
            return false;
        }

        unlinkInput(toNode, toSocket);
        MaterialGraphLink link{fromNode, std::move(fromSocket), toNode, std::move(toSocket)};
        m_links.push_back(std::move(link));
        if (hasCycle()) {
            m_links.pop_back();
            return false;
        }
        return true;
    }

    bool unlinkInput(MaterialGraphNodeId toNode, const std::string& toSocket)
    {
        const auto oldSize = m_links.size();
        m_links.erase(std::remove_if(m_links.begin(), m_links.end(), [&](const MaterialGraphLink& link) {
            return link.toNode == toNode && link.toSocket == toSocket;
        }), m_links.end());
        return m_links.size() != oldSize;
    }

    bool setSelectedNode(MaterialGraphNodeId id)
    {
        if (node(id) == nullptr) {
            return false;
        }
        m_selectedNode = id;
        for (MaterialGraphNode& item : m_nodes) {
            item.selected = item.id == id;
        }
        return true;
    }

    bool replaceData(MaterialGraphNodeId nextId, MaterialGraphNodeId outputNode, std::vector<MaterialGraphNode> nodes, std::vector<MaterialGraphLink> links)
    {
        if (nextId == 0 || outputNode == 0) {
            return false;
        }
        std::unordered_set<MaterialGraphNodeId> ids;
        for (const MaterialGraphNode& node : nodes) {
            if (node.id == 0 || !ids.insert(node.id).second || node.id >= nextId) {
                return false;
            }
        }
        if (ids.find(outputNode) == ids.end()) {
            return false;
        }
        for (const MaterialGraphLink& link : links) {
            if (ids.find(link.fromNode) == ids.end() || ids.find(link.toNode) == ids.end()) {
                return false;
            }
        }

        m_nextId = nextId;
        m_outputNode = outputNode;
        m_nodes = std::move(nodes);
        m_links = std::move(links);
        if (hasCycle()) {
            return false;
        }
        setSelectedNode(m_outputNode);
        return true;
    }

    MaterialGraphNode* node(MaterialGraphNodeId id)
    {
        for (MaterialGraphNode& item : m_nodes) {
            if (item.id == id) {
                return &item;
            }
        }
        return nullptr;
    }

    const MaterialGraphNode* node(MaterialGraphNodeId id) const
    {
        for (const MaterialGraphNode& item : m_nodes) {
            if (item.id == id) {
                return &item;
            }
        }
        return nullptr;
    }

    const std::vector<MaterialGraphNode>& nodes() const { return m_nodes; }
    std::vector<MaterialGraphNode>& nodes() { return m_nodes; }
    const std::vector<MaterialGraphLink>& links() const { return m_links; }
    MaterialGraphNodeId outputNode() const { return m_outputNode; }
    MaterialGraphNodeId selectedNode() const { return m_selectedNode; }
    MaterialGraphNodeId nextNodeIdForSerialization() const { return m_nextId; }

    bool hasCycle() const
    {
        std::unordered_set<MaterialGraphNodeId> visiting;
        std::unordered_set<MaterialGraphNodeId> visited;
        for (const MaterialGraphNode& node : m_nodes) {
            if (hasCycleFrom(node.id, visiting, visited)) {
                return true;
            }
        }
        return false;
    }

private:
    bool hasCycleFrom(MaterialGraphNodeId id, std::unordered_set<MaterialGraphNodeId>& visiting, std::unordered_set<MaterialGraphNodeId>& visited) const
    {
        if (visited.find(id) != visited.end()) {
            return false;
        }
        if (!visiting.insert(id).second) {
            return true;
        }
        for (const MaterialGraphLink& link : m_links) {
            if (link.fromNode == id && hasCycleFrom(link.toNode, visiting, visited)) {
                return true;
            }
        }
        visiting.erase(id);
        visited.insert(id);
        return false;
    }

    MaterialGraphNodeId m_nextId = 1;
    MaterialGraphNodeId m_outputNode = 0;
    MaterialGraphNodeId m_selectedNode = 0;
    std::vector<MaterialGraphNode> m_nodes;
    std::vector<MaterialGraphLink> m_links;
};

inline MaterialGraph makeMaterialGraphFromMaterial(const SdfMaterial& material)
{
    MaterialGraph graph;
    MaterialGraphNodeId albedo = 0;
    if (material.type == SdfMaterialType::Checker) {
        albedo = graph.createNode(MaterialGraphNodeType::CheckerPattern, "Pattern");
        if (MaterialGraphNode* node = graph.node(albedo)) {
            node->color = material.albedo;
            node->secondaryColor = material.secondaryAlbedo;
            node->value = material.patternScale;
        }
    } else if (material.type == SdfMaterialType::ValueNoise) {
        const MaterialGraphNodeId noise = graph.createNode(MaterialGraphNodeType::ValueNoise, "Value Noise");
        if (MaterialGraphNode* node = graph.node(noise)) {
            node->value = material.patternScale;
        }
        albedo = graph.createNode(MaterialGraphNodeType::MixColor, "Mix Color");
        if (MaterialGraphNode* node = graph.node(albedo)) {
            node->color = material.albedo;
            node->secondaryColor = material.secondaryAlbedo;
            node->value = 0.5f;
        }
        (void)graph.link(noise, "factor", albedo, "factor");
    } else {
        albedo = graph.createNode(MaterialGraphNodeType::ColorConstant, "Albedo");
        if (MaterialGraphNode* node = graph.node(albedo)) {
            node->color = material.albedo;
        }
    }

    const MaterialGraphNodeId pbr = graph.createNode(MaterialGraphNodeType::PbrMaterial, "PBR Material");
    if (MaterialGraphNode* node = graph.node(pbr)) {
        node->color = material.albedo;
        node->roughness = material.roughness;
        node->metallic = material.metallic;
        node->emission = material.emission;
    }
    (void)graph.link(albedo, "color", pbr, "albedo");
    (void)graph.link(pbr, "material", graph.outputNode(), "material");
    graph.setSelectedNode(graph.outputNode());
    return graph;
}

} // namespace sdf3d
