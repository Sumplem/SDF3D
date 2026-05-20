#include "sdf3d/systems/JsonGraphSerializer.h"

#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/scene/SdfNodeTraits.h"
#include "sdf3d/systems/GraphSystem.h"

#include "json_graph_serializer/JsonGraphSerializerConversions.h"

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

namespace sdf3d {
namespace {

using json = nlohmann::json;

constexpr const char* GRAPH_SCHEMA = "sdf3d.graph";
constexpr int GRAPH_SCHEMA_VERSION = 1;

std::unordered_set<MaterialId> referencedMaterialIds(const SdfGraph& graph)
{
    std::unordered_set<MaterialId> materialIds;
    for (const auto& [id, node] : graph.nodes()) {
        (void)id;
        if (node.payload.materialId != 0) {
            materialIds.insert(node.payload.materialId);
        }
    }

    return materialIds;
}

void synchronizeMaterialInputLinks(
    std::unordered_map<SdfGraphNodeId, SdfGraphNode>& nodes,
    const MaterialRegistry& materials,
    const std::vector<SdfGraphLink>& links)
{
    for (const SdfGraphLink& link : links) {
        if (link.fromSocket != "material" || link.toSocket != "material") {
            continue;
        }

        const auto fromIt = nodes.find(link.fromNode);
        const auto toIt = nodes.find(link.toNode);
        if (fromIt == nodes.end() || toIt == nodes.end() || toIt->second.payload.type != SdfNodeType::MaterialOverride) {
            continue;
        }

        const MaterialId materialId = fromIt->second.payload.materialId;
        if (materialId == 0 || materials.material(materialId) == nullptr) {
            continue;
        }

        toIt->second.payload.materialId = materialId;
        if (const MaterialDefinition* material = materials.material(materialId)) {
            toIt->second.payload.material = material->material;
        }
    }
}

json graphDataToJson(const SdfGraph& graph)
{
    std::vector<SdfGraphNodeId> ids;
    ids.reserve(graph.nodes().size());
    for (const auto& [id, node] : graph.nodes()) {
        (void)node;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());

    json nodes = json::array();
    for (const SdfGraphNodeId id : ids) {
        const SdfGraphNode* node = graph.node(id);
        if (node == nullptr) {
            continue;
        }
        nodes.push_back(json_graph_serializer::nodeToJson(*node));
    }

    json links = json::array();
    for (const SdfGraphLink& link : graph.links()) {
        links.push_back(json_graph_serializer::linkToJson(link));
    }

    json materials = json::array();
    const std::unordered_set<MaterialId> referencedMaterials = referencedMaterialIds(graph);
    for (const MaterialDefinition& material : graph.materials().materials()) {
        if (referencedMaterials.find(material.id) == referencedMaterials.end()) {
            continue;
        }
        materials.push_back(json_graph_serializer::materialDefinitionToJson(material));
    }

    json root{
        {"nextId", graph.nextNodeIdForSerialization()},
        {"materials", {{"nextId", graph.materials().nextMaterialIdForSerialization()}, {"items", materials}}},
        {"outputNode", graph.outputNode()},
        {"nodes", nodes},
        {"links", links},
    };
    return root;
}

bool loadGraphData(SdfGraph& graph, const json& root, std::string& error)
{
        MaterialRegistry materials;
        if (root.contains("materials")) {
            std::vector<MaterialDefinition> materialItems;
            for (const json& materialValue : root.at("materials").at("items")) {
                materialItems.push_back(json_graph_serializer::materialDefinitionFromJson(materialValue));
            }
            if (!materials.replaceMaterials(std::move(materialItems), root.at("materials").at("nextId").get<MaterialId>())) {
                error = "Serialized material registry failed validation.";
                return false;
            }
        }

        std::unordered_map<SdfGraphNodeId, SdfGraphNode> nodes;
        for (const json& nodeValue : root.at("nodes")) {
            SdfGraphNode node = json_graph_serializer::nodeFromJson(nodeValue);
            if (isSdfMaterialNode(node.payload.type)) {
                node.payload.material.type = node.payload.type == SdfNodeType::CheckerMaterial ? SdfMaterialType::Checker : SdfMaterialType::Solid;
                if (node.payload.materialId == 0 || materials.material(node.payload.materialId) == nullptr) {
                    node.payload.materialId = materials.createMaterial(node.payload.name.empty() ? "Material" : node.payload.name, node.payload.material);
                } else if (const MaterialDefinition* material = materials.material(node.payload.materialId)) {
                    node.payload.material = material->material;
                }
            }
            if (node.payload.type == SdfNodeType::MaterialOverride) {
                if (node.payload.materialId != 0 && materials.material(node.payload.materialId) != nullptr) {
                    node.payload.material = materials.material(node.payload.materialId)->material;
                } else if (nodeValue.contains("material")) {
                    node.payload.materialId = materials.createMaterial(node.payload.name.empty() ? "Material" : node.payload.name, node.payload.material);
                } else {
                    node.payload.materialId = 0;
                }
            }
            const SdfGraphNodeId id = node.id;
            if (!nodes.emplace(id, std::move(node)).second) {
                error = "Duplicate graph node id in file.";
                return false;
            }
        }

        std::vector<SdfGraphLink> links;
        for (const json& linkValue : root.at("links")) {
            links.push_back(json_graph_serializer::linkFromJson(linkValue));
        }
        synchronizeMaterialInputLinks(nodes, materials, links);

        const bool replaced = GraphSystem::replaceGraphData(
            graph,
            root.at("nextId").get<SdfGraphNodeId>(),
            root.at("outputNode").get<SdfGraphNodeId>(),
            0,
            {},
            std::move(nodes),
            std::move(links));
        if (!replaced) {
            error = "Serialized graph failed structural validation.";
            return false;
        }

        if (!graph.materials().replaceMaterials(std::vector<MaterialDefinition>{materials.materials().begin(), materials.materials().end()}, materials.nextMaterialIdForSerialization())) {
            error = "Serialized material registry failed validation.";
            return false;
        }

        return true;
}

json definitionsToJson(const GraphGroupRegistry& groups)
{
    json definitions = json::array();
    for (const GraphGroupDefinition& definition : groups.definitions()) {
        definitions.push_back({
            {"id", definition.id},
            {"name", definition.name},
            {"graph", graphDataToJson(definition.subgraph)},
        });
    }
    return definitions;
}

bool loadDefinitions(GraphGroupRegistry& groups, const json& root, std::string& error)
{
    if (!root.contains("definitions")) {
        return groups.replaceDefinitions({}, 1);
    }

    std::vector<GraphGroupDefinition> definitions;
    GroupDefId maxId = 0;
    for (const json& definitionValue : root.at("definitions")) {
        SdfGraph subgraph;
        if (!loadGraphData(subgraph, definitionValue.at("graph"), error)) {
            return false;
        }

        GraphGroupDefinition definition;
        definition.id = definitionValue.at("id").get<GroupDefId>();
        definition.name = definitionValue.at("name").get<std::string>();
        definition.subgraph = std::move(subgraph);
        maxId = std::max(maxId, definition.id);
        definitions.push_back(std::move(definition));
    }

    const GroupDefId nextId = root.value("nextDefinitionId", maxId + 1);
    if (!groups.replaceDefinitions(std::move(definitions), nextId)) {
        error = "Serialized group registry failed validation.";
        return false;
    }

    return true;
}

} // namespace

bool JsonGraphSerializer::save(const SdfGraph& graph, const std::filesystem::path& path)
{
    clearLastError();

    json root = graphDataToJson(graph);
    root["schema"] = GRAPH_SCHEMA;
    root["version"] = GRAPH_SCHEMA_VERSION;

    std::ofstream output(path);
    if (!output) {
        setLastError("Failed to open graph file for writing: " + path.string());
        return false;
    }
    output << root.dump(2) << '\n';
    return true;
}

bool JsonGraphSerializer::save(const SdfGraph& graph, const GraphGroupRegistry& groups, const std::filesystem::path& path)
{
    clearLastError();

    json root = graphDataToJson(graph);
    root["schema"] = GRAPH_SCHEMA;
    root["version"] = GRAPH_SCHEMA_VERSION;
    root["nextDefinitionId"] = groups.nextDefinitionIdForSerialization();
    root["definitions"] = definitionsToJson(groups);

    std::ofstream output(path);
    if (!output) {
        setLastError("Failed to open graph file for writing: " + path.string());
        return false;
    }
    output << root.dump(2) << '\n';
    return true;
}

bool JsonGraphSerializer::load(SdfGraph& graph, const std::filesystem::path& path)
{
    clearLastError();

    try {
        std::ifstream input(path);
        if (!input) {
            setLastError("Failed to open graph file for reading: " + path.string());
            return false;
        }

        json root;
        input >> root;
        if (root.at("schema").get<std::string>() != GRAPH_SCHEMA || root.at("version").get<int>() != GRAPH_SCHEMA_VERSION) {
            setLastError("Unsupported graph schema or version.");
            return false;
        }

        std::string error;
        if (!loadGraphData(graph, root, error)) {
            setLastError(error);
            return false;
        }

        return true;
    } catch (const std::exception& error) {
        setLastError(std::string("Failed to load graph JSON: ") + error.what());
        return false;
    }
}

bool JsonGraphSerializer::load(SdfGraph& graph, GraphGroupRegistry& groups, const std::filesystem::path& path)
{
    clearLastError();

    try {
        std::ifstream input(path);
        if (!input) {
            setLastError("Failed to open graph file for reading: " + path.string());
            return false;
        }

        json root;
        input >> root;
        if (root.at("schema").get<std::string>() != GRAPH_SCHEMA || root.at("version").get<int>() != GRAPH_SCHEMA_VERSION) {
            setLastError("Unsupported graph schema or version.");
            return false;
        }

        std::string error;
        if (!loadDefinitions(groups, root, error) || !loadGraphData(graph, root, error)) {
            setLastError(error);
            return false;
        }

        return true;
    } catch (const std::exception& error) {
        setLastError(std::string("Failed to load graph JSON: ") + error.what());
        return false;
    }
}

} // namespace sdf3d
