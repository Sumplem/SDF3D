#include "sdf3d/systems/JsonGraphSerializer.h"

#include "sdf3d/systems/GraphSystem.h"

#include "json_graph_serializer/JsonGraphSerializerConversions.h"

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

namespace sdf3d {
namespace {

using json = nlohmann::json;

constexpr const char* GRAPH_SCHEMA = "sdf3d.graph";
constexpr int GRAPH_SCHEMA_VERSION = 1;

} // namespace

bool JsonGraphSerializer::save(const SdfGraph& graph, const std::filesystem::path& path)
{
    clearLastError();

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
        json serializedNode = json_graph_serializer::nodeToJson(*node);
        for (const SdfGraphSocket& input : node->inputs) {
            serializedNode["inputs"].push_back(json_graph_serializer::socketToJson(input));
        }
        for (const SdfGraphSocket& output : node->outputs) {
            serializedNode["outputs"].push_back(json_graph_serializer::socketToJson(output));
        }
        nodes.push_back(std::move(serializedNode));
    }

    json links = json::array();
    for (const SdfGraphLink& link : graph.links()) {
        links.push_back(json_graph_serializer::linkToJson(link));
    }

    json materials = json::array();
    for (const MaterialDefinition& material : graph.materials().materials()) {
        materials.push_back(json_graph_serializer::materialDefinitionToJson(material));
    }

    json root{
        {"schema", GRAPH_SCHEMA},
        {"version", GRAPH_SCHEMA_VERSION},
        {"nextId", graph.nextNodeIdForSerialization()},
        {"materials", {{"nextId", graph.materials().nextMaterialIdForSerialization()}, {"items", materials}}},
        {"outputNode", graph.outputNode()},
        {"selection", {{"primary", graph.selectedNode()}, {"nodes", graph.selectedNodes()}}},
        {"nodes", nodes},
        {"links", links},
    };

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

        MaterialRegistry materials;
        if (root.contains("materials")) {
            std::vector<MaterialDefinition> materialItems;
            for (const json& materialValue : root.at("materials").at("items")) {
                materialItems.push_back(json_graph_serializer::materialDefinitionFromJson(materialValue));
            }
            if (!materials.replaceMaterials(std::move(materialItems), root.at("materials").at("nextId").get<MaterialId>())) {
                setLastError("Serialized material registry failed validation.");
                return false;
            }
        }

        std::unordered_map<SdfGraphNodeId, SdfGraphNode> nodes;
        for (const json& nodeValue : root.at("nodes")) {
            SdfGraphNode node = json_graph_serializer::nodeFromJson(nodeValue);
            if (node.payload.type == SdfNodeType::MaterialOverride) {
                if (node.payload.materialId == 0 || materials.material(node.payload.materialId) == nullptr) {
                    node.payload.materialId = materials.createMaterial(node.payload.name.empty() ? "Material" : node.payload.name, node.payload.material);
                } else if (const MaterialDefinition* material = materials.material(node.payload.materialId)) {
                    node.payload.material = material->material;
                }
            }
            const SdfGraphNodeId id = node.id;
            if (!nodes.emplace(id, std::move(node)).second) {
                setLastError("Duplicate graph node id in file.");
                return false;
            }
        }

        std::vector<SdfGraphLink> links;
        for (const json& linkValue : root.at("links")) {
            links.push_back(json_graph_serializer::linkFromJson(linkValue));
        }

        std::vector<SdfGraphNodeId> selectedNodes = root.at("selection").at("nodes").get<std::vector<SdfGraphNodeId>>();
        const bool replaced = GraphSystem::replaceGraphData(
            graph,
            root.at("nextId").get<SdfGraphNodeId>(),
            root.at("outputNode").get<SdfGraphNodeId>(),
            root.at("selection").at("primary").get<SdfGraphNodeId>(),
            std::move(selectedNodes),
            std::move(nodes),
            std::move(links));
        if (!replaced) {
            setLastError("Serialized graph failed structural validation.");
            return false;
        }

        if (!graph.materials().replaceMaterials(std::vector<MaterialDefinition>{materials.materials().begin(), materials.materials().end()}, materials.nextMaterialIdForSerialization())) {
            setLastError("Serialized material registry failed validation.");
            return false;
        }

        return true;
    } catch (const std::exception& error) {
        setLastError(std::string("Failed to load graph JSON: ") + error.what());
        return false;
    }
}

} // namespace sdf3d
