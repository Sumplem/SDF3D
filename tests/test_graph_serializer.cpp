#include "sdf3d/systems/JsonGraphSerializer.h"

#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/scene/SdfCompiler.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

struct TestFailure {
    std::string name;
    std::string message;
};

void expect(bool condition, const std::string& testName, const std::string& message, std::vector<TestFailure>& failures)
{
    if (!condition) {
        failures.push_back({testName, message});
    }
}

bool hasLink(
    const sdf3d::SdfGraph& graph,
    sdf3d::SdfGraphNodeId fromNode,
    const std::string& fromSocket,
    sdf3d::SdfGraphNodeId toNode,
    const std::string& toSocket)
{
    for (const sdf3d::SdfGraphLink& link : graph.links()) {
        if (link.fromNode == fromNode
            && link.fromSocket == fromSocket
            && link.toNode == toNode
            && link.toSocket == toSocket) {
            return true;
        }
    }

    return false;
}

std::filesystem::path testPath(const std::string& fileName)
{
    return std::filesystem::temp_directory_path() / fileName;
}

void testJsonGraphRoundTrip(std::vector<TestFailure>& failures)
{
    const std::string testName = "json graph round trip";
    const std::filesystem::path path = testPath("sdf3d_graph_round_trip.json");
    sdf3d::JsonGraphSerializer serializer;
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId output = graph.outputNode();
    const sdf3d::SdfGraphNodeId sphere = graph.createNode(sdf3d::SdfNodeType::Sphere, "Sphere A");
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::CheckerMaterial, "Paint");
    const sdf3d::SdfGraphNodeId materialOverride = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Apply Paint");
    if (sdf3d::SdfGraphNode* sphereNode = graph.node(sphere)) {
        sphereNode->payload.parameters["radius"] = 1.75f;
        sphereNode->editorX = 42.0f;
        sphereNode->editorY = 84.0f;
        sphereNode->editorPropertiesCollapsed = true;
    }
    if (sdf3d::SdfGraphNode* materialNode = graph.node(material)) {
        if (sdf3d::MaterialDefinition* definition = graph.materials().material(materialNode->payload.materialId)) {
            definition->material.albedo = {0.25f, 0.5f, 0.75f};
            definition->material.type = sdf3d::SdfMaterialType::Checker;
            definition->material.secondaryAlbedo = {0.75f, 0.25f, 0.125f};
            definition->material.roughness = 0.35f;
            definition->material.metallic = 0.2f;
            definition->material.emission = 1.25f;
            definition->material.patternScale = 9.0f;
            definition->graph = sdf3d::makeMaterialGraphFromMaterial(definition->material);
        }
    }
    expect(graph.link(sphere, "sdf", materialOverride, "sdf"), testName, "Expected sphere to override link.", failures);
    expect(graph.link(material, "material", materialOverride, "material"), testName, "Expected material to override link.", failures);
    expect(graph.link(materialOverride, "sdf", output, "surface"), testName, "Expected override to output link.", failures);
    expect(graph.setSelectedNodes({sphere, material, materialOverride}, materialOverride), testName, "Expected graph selection.", failures);

    expect(serializer.save(graph, path), testName, "Expected save success: " + serializer.lastError(), failures);
    nlohmann::json saved;
    {
        std::ifstream input(path);
        input >> saved;
    }
    expect(!saved.contains("selection"), testName, "Expected selection omitted from saved JSON.", failures);
    for (const nlohmann::json& node : saved.at("nodes")) {
        expect(node.at("stableId").get<std::uint64_t>() != 0, testName, "Expected saved stableId assigned.", failures);
        expect(!node.contains("inputs"), testName, "Expected inputs omitted from saved JSON.", failures);
        expect(!node.contains("outputs"), testName, "Expected outputs omitted from saved JSON.", failures);
        expect(!node.contains("material"), testName, "Expected node material blob omitted from saved JSON.", failures);
        if (node.at("type").get<std::string>() == "MaterialOverride") {
            const sdf3d::SdfGraphNode* overrideNode = graph.node(materialOverride);
            expect(overrideNode != nullptr && node.contains("materialId") && node.at("materialId").get<sdf3d::MaterialId>() == overrideNode->payload.materialId, testName, "Expected MaterialOverride materialId saved.", failures);
        }
    }

    sdf3d::SdfGraph loaded;
    expect(serializer.load(loaded, path), testName, "Expected load success: " + serializer.lastError(), failures);

    const sdf3d::SdfGraphNode* loadedSphere = loaded.node(sphere);
    const sdf3d::SdfGraphNode* loadedMaterial = loaded.node(material);
    const sdf3d::SdfGraphNode* loadedMaterialOverride = loaded.node(materialOverride);
    expect(loaded.outputNode() == output, testName, "Expected output node ID preserved.", failures);
    expect(loadedSphere != nullptr, testName, "Expected sphere loaded.", failures);
    expect(loadedMaterial != nullptr, testName, "Expected material loaded.", failures);
    expect(loadedMaterialOverride != nullptr, testName, "Expected material override loaded.", failures);
    expect(loaded.node(output) != nullptr && loaded.node(output)->payload.stableId == output, testName, "Expected output stableId preserved.", failures);
    if (loadedSphere != nullptr) {
        expect(loadedSphere->payload.name == "Sphere A", testName, "Expected sphere name preserved.", failures);
        expect(loadedSphere->payload.stableId == sphere, testName, "Expected sphere stableId preserved.", failures);
        expect(loadedSphere->payload.parameters.at("radius") == 1.75f, testName, "Expected sphere radius preserved.", failures);
        expect(loadedSphere->editorX == 42.0f && loadedSphere->editorY == 84.0f, testName, "Expected editor position preserved.", failures);
        expect(loadedSphere->editorPropertiesCollapsed, testName, "Expected editor collapsed state preserved.", failures);
        expect(loadedSphere->outputs.size() == 1 && loadedSphere->outputs[0].name == "sdf", testName, "Expected sockets reconstructed.", failures);
    }
    if (loadedMaterial != nullptr) {
        expect(loadedMaterial->payload.stableId == material, testName, "Expected material stableId preserved.", failures);
        expect(loadedMaterial->payload.materialId != 0, testName, "Expected material id preserved.", failures);
        const sdf3d::MaterialDefinition* definition = loaded.materials().material(loadedMaterial->payload.materialId);
        expect(definition != nullptr, testName, "Expected registry material loaded.", failures);
        if (definition != nullptr) {
            expect(definition->material.albedo.z == 0.75f, testName, "Expected albedo preserved.", failures);
            expect(definition->material.type == sdf3d::SdfMaterialType::Checker, testName, "Expected material type preserved.", failures);
            expect(definition->material.secondaryAlbedo.x == 0.75f, testName, "Expected secondary albedo preserved.", failures);
            expect(definition->material.emission == 1.25f, testName, "Expected emission preserved.", failures);
            expect(definition->material.patternScale == 9.0f, testName, "Expected pattern scale preserved.", failures);
            expect(definition->graph.outputNode() != 0, testName, "Expected material graph loaded.", failures);
        }
    }
    if (loadedMaterialOverride != nullptr && loadedMaterial != nullptr) {
        expect(loadedMaterialOverride->payload.stableId == materialOverride, testName, "Expected override stableId preserved.", failures);
        expect(loadedMaterialOverride->payload.materialId == loadedMaterial->payload.materialId, testName, "Expected override material id preserved.", failures);
    }
    expect(hasLink(loaded, sphere, "sdf", materialOverride, "sdf"), testName, "Expected override SDF input link preserved.", failures);
    expect(hasLink(loaded, material, "material", materialOverride, "material"), testName, "Expected material input link preserved.", failures);
    expect(hasLink(loaded, materialOverride, "sdf", output, "surface"), testName, "Expected output link preserved.", failures);
    expect(loaded.selectedNode() == 0 && loaded.selectedNodes().empty(), testName, "Expected selection not restored from scene file.", failures);
    expect(loaded.createNode(sdf3d::SdfNodeType::Box, "Next") == 5, testName, "Expected next stable ID preserved.", failures);

    const sdf3d::SdfCompileResult compileResult = sdf3d::SdfCompiler{}.compile(loaded);
    expect(compileResult.errors.empty(), testName, "Expected loaded graph to compile without errors.", failures);
    expect(compileResult.glsl.find("sdf3d_material_") != std::string::npos, testName, "Expected loaded registry material graph helper.", failures);
    expect(compileResult.glsl.find("9.000000") != std::string::npos, testName, "Expected loaded material graph scale in GLSL.", failures);
    expect(compileResult.glsl.find("0.350000") != std::string::npos, testName, "Expected loaded material graph roughness in GLSL.", failures);
    expect(compileResult.glsl.find("1.250000") != std::string::npos, testName, "Expected loaded material graph emission in GLSL.", failures);

    std::filesystem::remove(path);
}

void testJsonGraphValueNoiseMaterialRoundTrip(std::vector<TestFailure>& failures)
{
    const std::string testName = "json value noise material round trip";
    const std::filesystem::path path = testPath("sdf3d_value_noise_material_round_trip.json");
    sdf3d::JsonGraphSerializer serializer;
    sdf3d::SdfGraph graph;

    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::ValueNoiseMaterial, "Cloud Paint");
    if (sdf3d::SdfGraphNode* materialNode = graph.node(material)) {
        if (sdf3d::MaterialDefinition* definition = graph.materials().material(materialNode->payload.materialId)) {
            definition->material.albedo = {0.2f, 0.3f, 0.4f};
            definition->material.secondaryAlbedo = {0.8f, 0.7f, 0.6f};
            definition->material.patternScale = 15.0f;
            definition->graph = sdf3d::makeMaterialGraphFromMaterial(definition->material);
        }
    }

    expect(serializer.save(graph, path), testName, "Expected save success: " + serializer.lastError(), failures);
    sdf3d::SdfGraph loaded;
    expect(serializer.load(loaded, path), testName, "Expected load success: " + serializer.lastError(), failures);

    const sdf3d::SdfGraphNode* loadedMaterial = loaded.node(material);
    expect(loadedMaterial != nullptr && loadedMaterial->payload.type == sdf3d::SdfNodeType::ValueNoiseMaterial, testName, "Expected value-noise node type loaded.", failures);
    if (loadedMaterial != nullptr) {
        const sdf3d::MaterialDefinition* definition = loaded.materials().material(loadedMaterial->payload.materialId);
        expect(definition != nullptr, testName, "Expected value-noise registry material loaded.", failures);
        if (definition != nullptr) {
            expect(definition->material.type == sdf3d::SdfMaterialType::ValueNoise, testName, "Expected value-noise material type preserved.", failures);
            expect(definition->material.secondaryAlbedo.x == 0.8f, testName, "Expected secondary albedo preserved.", failures);
            expect(definition->material.patternScale == 15.0f, testName, "Expected pattern scale preserved.", failures);
            expect(definition->graph.outputNode() != 0, testName, "Expected value-noise material graph preserved.", failures);
        }
    }

    std::filesystem::remove(path);
}

void testJsonGraphMigratesInlineMaterial(std::vector<TestFailure>& failures)
{
    const std::string testName = "json graph migrates inline material";
    const std::filesystem::path path = testPath("sdf3d_graph_old_material.json");
    {
        std::ofstream output(path);
        output
            << "{\n"
            << "  \"schema\": \"sdf3d.graph\",\n"
            << "  \"version\": 1,\n"
            << "  \"nextId\": 3,\n"
            << "  \"outputNode\": 1,\n"
            << "  \"selection\": {\"primary\": 2, \"nodes\": [2]},\n"
            << "  \"nodes\": [\n"
            << "    {\"id\": 1, \"type\": \"Output\", \"stableId\": 0, \"name\": \"Output\", \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {}, \"material\": {\"albedo\": [0.8, 0.8, 0.8], \"roughness\": 0.5, \"metallic\": 0.0, \"emission\": 0.0}, \"inputs\": [{\"name\": \"surface\", \"type\": \"Sdf\", \"direction\": \"Input\", \"multiInput\": false}], \"outputs\": []},\n"
            << "    {\"id\": 2, \"type\": \"MaterialOverride\", \"stableId\": 0, \"name\": \"Old Paint\", \"editor\": {\"x\": 10, \"y\": 20, \"propertiesCollapsed\": false}, \"parameters\": {}, \"material\": {\"albedo\": [0.1, 0.2, 0.3], \"roughness\": 0.4, \"metallic\": 0.5, \"emission\": 0.6}, \"inputs\": [{\"name\": \"sdf\", \"type\": \"Sdf\", \"direction\": \"Input\", \"multiInput\": false}], \"outputs\": [{\"name\": \"sdf\", \"type\": \"Sdf\", \"direction\": \"Output\", \"multiInput\": false}]}\n"
            << "  ],\n"
            << "  \"links\": [{\"from\": {\"node\": 2, \"socket\": \"sdf\"}, \"to\": {\"node\": 1, \"socket\": \"surface\"}}]\n"
            << "}\n";
    }

    sdf3d::SdfGraph graph;
    sdf3d::JsonGraphSerializer serializer;
    expect(serializer.load(graph, path), testName, "Expected old material load success: " + serializer.lastError(), failures);

    const sdf3d::SdfGraphNode* materialNode = graph.node(2);
    expect(materialNode != nullptr, testName, "Expected migrated material node.", failures);
    expect(materialNode != nullptr && materialNode->payload.materialId != 0, testName, "Expected migrated material id.", failures);
    if (materialNode != nullptr) {
        const sdf3d::MaterialDefinition* definition = graph.materials().material(materialNode->payload.materialId);
        expect(definition != nullptr, testName, "Expected migrated registry material.", failures);
        if (definition != nullptr) {
            expect(definition->name == "Old Paint", testName, "Expected migrated material name.", failures);
            expect(definition->material.albedo.z == 0.3f, testName, "Expected migrated albedo.", failures);
            expect(definition->material.emission == 0.6f, testName, "Expected migrated emission.", failures);
        }
    }

    std::filesystem::remove(path);
}

void testJsonGraphMigratesSourceMaterialNode(std::vector<TestFailure>& failures)
{
    const std::string testName = "json graph migrates source material node";
    const std::filesystem::path path = testPath("sdf3d_graph_source_material_no_registry.json");
    {
        std::ofstream output(path);
        output
            << "{\n"
            << "  \"schema\": \"sdf3d.graph\",\n"
            << "  \"version\": 1,\n"
            << "  \"nextId\": 3,\n"
            << "  \"outputNode\": 1,\n"
            << "  \"selection\": {\"primary\": 2, \"nodes\": [2]},\n"
            << "  \"nodes\": [\n"
            << "    {\"id\": 1, \"type\": \"Output\", \"stableId\": 0, \"name\": \"Output\", \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {}, \"material\": {\"albedo\": [0.8, 0.8, 0.8], \"roughness\": 0.5, \"metallic\": 0.0, \"emission\": 0.0}, \"inputs\": [{\"name\": \"surface\", \"type\": \"Sdf\", \"direction\": \"Input\", \"multiInput\": false}], \"outputs\": []},\n"
            << "    {\"id\": 2, \"type\": \"CheckerMaterial\", \"stableId\": 0, \"name\": \"Loose Paint\", \"materialId\": 0, \"editor\": {\"x\": 10, \"y\": 20, \"propertiesCollapsed\": false}, \"parameters\": {}, \"material\": {\"type\": 1, \"albedo\": [0.1, 0.2, 0.3], \"secondaryAlbedo\": [0.7, 0.8, 0.9], \"roughness\": 0.4, \"metallic\": 0.5, \"emission\": 0.6, \"patternScale\": 12.0}, \"inputs\": [], \"outputs\": [{\"name\": \"material\", \"type\": \"Material\", \"direction\": \"Output\", \"multiInput\": false}]}\n"
            << "  ],\n"
            << "  \"links\": []\n"
            << "}\n";
    }

    sdf3d::SdfGraph graph;
    sdf3d::JsonGraphSerializer serializer;
    expect(serializer.load(graph, path), testName, "Expected source material load success: " + serializer.lastError(), failures);

    const sdf3d::SdfGraphNode* materialNode = graph.node(2);
    expect(materialNode != nullptr, testName, "Expected migrated source material node.", failures);
    expect(materialNode != nullptr && materialNode->payload.materialId != 0, testName, "Expected migrated source material id.", failures);
    if (materialNode != nullptr) {
        const sdf3d::MaterialDefinition* definition = graph.materials().material(materialNode->payload.materialId);
        expect(definition != nullptr, testName, "Expected migrated source registry material.", failures);
        if (definition != nullptr) {
            expect(definition->name == "Loose Paint", testName, "Expected source material name.", failures);
            expect(definition->material.type == sdf3d::SdfMaterialType::Checker, testName, "Expected checker material type.", failures);
            expect(definition->material.secondaryAlbedo.y == 0.8f, testName, "Expected checker secondary color.", failures);
            expect(definition->material.patternScale == 12.0f, testName, "Expected checker pattern scale.", failures);
        }
    }

    std::filesystem::remove(path);
}

void testJsonGraphLoadRepairsLinkedOverrideMaterialId(std::vector<TestFailure>& failures)
{
    const std::string testName = "json graph load repairs linked override material id";
    const std::filesystem::path path = testPath("sdf3d_graph_repair_override_material.json");
    const std::filesystem::path savedPath = testPath("sdf3d_graph_repaired_override_material.json");
    {
        std::ofstream output(path);
        output
            << "{\n"
            << "  \"schema\": \"sdf3d.graph\",\n"
            << "  \"version\": 1,\n"
            << "  \"nextId\": 5,\n"
            << "  \"outputNode\": 1,\n"
            << "  \"materials\": {\"nextId\": 4, \"items\": [\n"
            << "    {\"id\": 1, \"name\": \"Stale Override\", \"material\": {\"type\": 0, \"albedo\": [1.0, 0.0, 0.0], \"secondaryAlbedo\": [0.0, 0.0, 0.0], \"roughness\": 0.5, \"metallic\": 0.0, \"emission\": 0.0, \"patternScale\": 4.0}},\n"
            << "    {\"id\": 2, \"name\": \"Linked Checker\", \"material\": {\"type\": 1, \"albedo\": [0.1, 0.2, 0.3], \"secondaryAlbedo\": [0.4, 0.5, 0.6], \"roughness\": 0.7, \"metallic\": 0.0, \"emission\": 0.0, \"patternScale\": 8.0}},\n"
            << "    {\"id\": 3, \"name\": \"Deleted Material\", \"material\": {\"type\": 0, \"albedo\": [0.9, 0.9, 0.9], \"secondaryAlbedo\": [0.0, 0.0, 0.0], \"roughness\": 0.5, \"metallic\": 0.0, \"emission\": 0.0, \"patternScale\": 4.0}}\n"
            << "  ]},\n"
            << "  \"nodes\": [\n"
            << "    {\"id\": 1, \"type\": \"Output\", \"stableId\": 1, \"name\": \"Output\", \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {}},\n"
            << "    {\"id\": 2, \"type\": \"Sphere\", \"stableId\": 2, \"name\": \"Sphere\", \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {\"radius\": 1.0}},\n"
            << "    {\"id\": 3, \"type\": \"MaterialOverride\", \"stableId\": 3, \"name\": \"Override\", \"materialId\": 1, \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {}},\n"
            << "    {\"id\": 4, \"type\": \"CheckerMaterial\", \"stableId\": 4, \"name\": \"Linked Checker\", \"materialId\": 2, \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {}}\n"
            << "  ],\n"
            << "  \"links\": [\n"
            << "    {\"from\": {\"node\": 2, \"socket\": \"sdf\"}, \"to\": {\"node\": 3, \"socket\": \"sdf\"}},\n"
            << "    {\"from\": {\"node\": 4, \"socket\": \"material\"}, \"to\": {\"node\": 3, \"socket\": \"material\"}},\n"
            << "    {\"from\": {\"node\": 3, \"socket\": \"sdf\"}, \"to\": {\"node\": 1, \"socket\": \"surface\"}}\n"
            << "  ]\n"
            << "}\n";
    }

    sdf3d::SdfGraph graph;
    sdf3d::JsonGraphSerializer serializer;
    expect(serializer.load(graph, path), testName, "Expected load success: " + serializer.lastError(), failures);
    const sdf3d::SdfGraphNode* overrideNode = graph.node(3);
    expect(overrideNode != nullptr && overrideNode->payload.materialId == 2, testName, "Expected linked material id to win.", failures);

    expect(serializer.save(graph, savedPath), testName, "Expected repaired save success: " + serializer.lastError(), failures);
    nlohmann::json saved;
    {
        std::ifstream input(savedPath);
        input >> saved;
    }
    bool savedLinked = false;
    bool savedStale = false;
    for (const nlohmann::json& material : saved.at("materials").at("items")) {
        savedLinked = savedLinked || material.at("name").get<std::string>() == "Linked Checker";
        savedStale = savedStale || material.at("name").get<std::string>() == "Stale Override";
    }
    expect(savedLinked, testName, "Expected linked material retained.", failures);
    expect(!savedStale, testName, "Expected stale override material pruned.", failures);

    std::filesystem::remove(path);
    std::filesystem::remove(savedPath);
}

void testJsonGraphGroupDefinitionsRoundTrip(std::vector<TestFailure>& failures)
{
    const std::string testName = "json graph group definitions round trip";
    const std::filesystem::path path = testPath("sdf3d_graph_groups_round_trip.json");
    sdf3d::JsonGraphSerializer serializer;

    sdf3d::SdfGraph groupGraph;
    const sdf3d::SdfGraphNodeId groupSphere = groupGraph.createNode(sdf3d::SdfNodeType::Sphere, "Group Sphere");
    expect(groupGraph.link(groupSphere, "sdf", groupGraph.outputNode(), "surface"), testName, "Expected group output link.", failures);

    sdf3d::GraphGroupRegistry groups;
    const sdf3d::GroupDefId definitionId = groups.createDefinition("Ball Group", groupGraph);

    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId groupInstance = graph.createNode(sdf3d::SdfNodeType::Group, "Ball Instance");
    if (sdf3d::SdfGraphNode* node = graph.node(groupInstance)) {
        node->payload.groupDefinitionId = definitionId;
    }
    expect(graph.link(groupInstance, "sdf", graph.outputNode(), "surface"), testName, "Expected group instance output link.", failures);
    expect(serializer.save(graph, groups, path), testName, "Expected save success: " + serializer.lastError(), failures);

    nlohmann::json saved;
    {
        std::ifstream input(path);
        input >> saved;
    }
    expect(saved.contains("definitions"), testName, "Expected root definitions array.", failures);
    expect(saved.at("definitions").size() == 1, testName, "Expected one group definition.", failures);

    sdf3d::SdfGraph loadedGraph;
    sdf3d::GraphGroupRegistry loadedGroups;
    expect(serializer.load(loadedGraph, loadedGroups, path), testName, "Expected load success: " + serializer.lastError(), failures);

    const sdf3d::SdfGraphNode* loadedGroup = loadedGraph.node(groupInstance);
    expect(loadedGroups.definition(definitionId) != nullptr, testName, "Expected group definition loaded.", failures);
    expect(loadedGroup != nullptr && loadedGroup->payload.groupDefinitionId == definitionId, testName, "Expected group instance definition id preserved.", failures);

    const sdf3d::SdfCompileResult compileResult = sdf3d::SdfCompiler{}.compile(loadedGraph, loadedGroups);
    expect(compileResult.errors.empty(), testName, "Expected loaded group graph to compile without errors.", failures);
    expect(compileResult.glsl.find("sceneSDF") != std::string::npos, testName, "Expected group compile GLSL.", failures);

    std::filesystem::remove(path);
}

void testJsonGraphPrunesUnreferencedGroupDefinitions(std::vector<TestFailure>& failures)
{
    const std::string testName = "json graph prunes unreferenced group definitions";
    const std::filesystem::path path = testPath("sdf3d_graph_groups_pruned.json");
    sdf3d::JsonGraphSerializer serializer;

    sdf3d::SdfGraph usedGraph;
    const sdf3d::SdfGraphNodeId usedSphere = usedGraph.createNode(sdf3d::SdfNodeType::Sphere, "Used Sphere");
    expect(usedGraph.link(usedSphere, "sdf", usedGraph.outputNode(), "surface"), testName, "Expected used group output link.", failures);

    sdf3d::SdfGraph orphanGraph;
    const sdf3d::SdfGraphNodeId orphanSphere = orphanGraph.createNode(sdf3d::SdfNodeType::Sphere, "Orphan Sphere");
    expect(orphanGraph.link(orphanSphere, "sdf", orphanGraph.outputNode(), "surface"), testName, "Expected orphan group output link.", failures);

    sdf3d::GraphGroupRegistry groups;
    const sdf3d::GroupDefId usedDefinitionId = groups.createDefinition("Used Group", usedGraph);
    const sdf3d::GroupDefId orphanDefinitionId = groups.createDefinition("Orphan Group", orphanGraph);

    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId groupInstance = graph.createNode(sdf3d::SdfNodeType::Group, "Used Instance");
    if (sdf3d::SdfGraphNode* node = graph.node(groupInstance)) {
        node->payload.groupDefinitionId = usedDefinitionId;
    }
    expect(graph.link(groupInstance, "sdf", graph.outputNode(), "surface"), testName, "Expected group instance output link.", failures);

    expect(serializer.save(graph, groups, path), testName, "Expected save success: " + serializer.lastError(), failures);
    nlohmann::json saved;
    {
        std::ifstream input(path);
        input >> saved;
    }

    expect(saved.at("definitions").size() == 1, testName, "Expected only referenced group definition saved.", failures);
    if (saved.at("definitions").size() == 1) {
        expect(saved.at("definitions").front().at("id").get<sdf3d::GroupDefId>() == usedDefinitionId, testName, "Expected used definition saved.", failures);
        expect(saved.at("definitions").front().at("id").get<sdf3d::GroupDefId>() != orphanDefinitionId, testName, "Expected orphan definition omitted.", failures);
    }

    std::filesystem::remove(path);
}

void testJsonGraphSavesNestedReachableGroupDefinitions(std::vector<TestFailure>& failures)
{
    const std::string testName = "json graph saves nested reachable group definitions";
    const std::filesystem::path path = testPath("sdf3d_graph_nested_groups.json");
    sdf3d::JsonGraphSerializer serializer;

    sdf3d::SdfGraph leafGraph;
    const sdf3d::SdfGraphNodeId leafSphere = leafGraph.createNode(sdf3d::SdfNodeType::Sphere, "Leaf Sphere");
    expect(leafGraph.link(leafSphere, "sdf", leafGraph.outputNode(), "surface"), testName, "Expected leaf group output link.", failures);

    sdf3d::GraphGroupRegistry groups;
    const sdf3d::GroupDefId leafDefinitionId = groups.createDefinition("Leaf Group", leafGraph);

    sdf3d::SdfGraph parentGraph;
    const sdf3d::SdfGraphNodeId nestedInstance = parentGraph.createNode(sdf3d::SdfNodeType::Group, "Nested Leaf");
    if (sdf3d::SdfGraphNode* node = parentGraph.node(nestedInstance)) {
        node->payload.groupDefinitionId = leafDefinitionId;
    }
    expect(parentGraph.link(nestedInstance, "sdf", parentGraph.outputNode(), "surface"), testName, "Expected parent group output link.", failures);
    const sdf3d::GroupDefId parentDefinitionId = groups.createDefinition("Parent Group", parentGraph);

    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId parentInstance = graph.createNode(sdf3d::SdfNodeType::Group, "Parent Instance");
    if (sdf3d::SdfGraphNode* node = graph.node(parentInstance)) {
        node->payload.groupDefinitionId = parentDefinitionId;
    }
    expect(graph.link(parentInstance, "sdf", graph.outputNode(), "surface"), testName, "Expected root group output link.", failures);

    expect(serializer.save(graph, groups, path), testName, "Expected save success: " + serializer.lastError(), failures);
    nlohmann::json saved;
    {
        std::ifstream input(path);
        input >> saved;
    }
    expect(saved.at("definitions").size() == 2, testName, "Expected parent and nested group definitions saved.", failures);

    sdf3d::SdfGraph loadedGraph;
    sdf3d::GraphGroupRegistry loadedGroups;
    expect(serializer.load(loadedGraph, loadedGroups, path), testName, "Expected load success: " + serializer.lastError(), failures);
    expect(loadedGroups.definition(parentDefinitionId) != nullptr, testName, "Expected parent definition loaded.", failures);
    expect(loadedGroups.definition(leafDefinitionId) != nullptr, testName, "Expected nested definition loaded.", failures);

    const sdf3d::SdfCompileResult compileResult = sdf3d::SdfCompiler{}.compile(loadedGraph, loadedGroups);
    expect(compileResult.errors.empty(), testName, "Expected loaded nested groups to compile without errors.", failures);

    std::filesystem::remove(path);
}

void testJsonGraphLoadFailureKeepsGraph(std::vector<TestFailure>& failures)
{
    const std::string testName = "json graph load failure keeps graph";
    const std::filesystem::path path = testPath("sdf3d_graph_invalid.json");
    {
        std::ofstream output(path);
        output << "{ \"schema\": \"sdf3d.graph\", \"version\": 999, \"nodes\": [], \"links\": [] }\n";
    }

    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId originalOutput = graph.outputNode();
    sdf3d::JsonGraphSerializer serializer;

    expect(!serializer.load(graph, path), testName, "Expected invalid schema load to fail.", failures);
    expect(!serializer.lastError().empty(), testName, "Expected load error.", failures);
    expect(graph.outputNode() == originalOutput, testName, "Expected graph output unchanged.", failures);
    expect(graph.node(originalOutput) != nullptr, testName, "Expected graph node unchanged.", failures);

    std::filesystem::remove(path);
}

void testJsonGraphMigratesLegacyBooleanSockets(std::vector<TestFailure>& failures)
{
    const std::string testName = "json graph migrates legacy boolean sockets";
    const std::filesystem::path path = testPath("sdf3d_legacy_boolean_sockets.json");
    sdf3d::SdfGraph graph;

    {
        std::ofstream output(path);
        output
            << "{\n"
            << "  \"schema\": \"sdf3d.graph\",\n"
            << "  \"version\": 1,\n"
            << "  \"nextId\": 5,\n"
            << "  \"outputNode\": 1,\n"
            << "  \"nodes\": [\n"
            << "    {\"id\": 1, \"type\": \"Output\", \"stableId\": 1, \"name\": \"Output\", \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {}},\n"
            << "    {\"id\": 2, \"type\": \"Sphere\", \"stableId\": 2, \"name\": \"Sphere\", \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {\"radius\": 1.0}},\n"
            << "    {\"id\": 3, \"type\": \"Box\", \"stableId\": 3, \"name\": \"Box\", \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {\"x\": 1.0, \"y\": 1.0, \"z\": 1.0}},\n"
            << "    {\"id\": 4, \"type\": \"Union\", \"stableId\": 4, \"name\": \"Union\", \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {}}\n"
            << "  ],\n"
            << "  \"links\": [\n"
            << "    {\"from\": {\"node\": 2, \"socket\": \"sdf\"}, \"to\": {\"node\": 4, \"socket\": \"left\"}},\n"
            << "    {\"from\": {\"node\": 3, \"socket\": \"sdf\"}, \"to\": {\"node\": 4, \"socket\": \"right\"}},\n"
            << "    {\"from\": {\"node\": 4, \"socket\": \"sdf\"}, \"to\": {\"node\": 1, \"socket\": \"surface\"}}\n"
            << "  ]\n"
            << "}\n";
    }

    sdf3d::JsonGraphSerializer serializer;
    expect(serializer.load(graph, path), testName, "Expected load success: " + serializer.lastError(), failures);
    expect(hasLink(graph, 2, "sdf", 4, "inputs"), testName, "Expected legacy left migrated to inputs.", failures);
    expect(hasLink(graph, 3, "sdf", 4, "inputs"), testName, "Expected legacy right migrated to inputs.", failures);
    expect(graph.node(4) != nullptr && graph.node(4)->inputs.size() == 1 && graph.node(4)->inputs[0].multiInput, testName, "Expected Union multi-input socket reconstructed.", failures);

    std::filesystem::remove(path);
}

void testJsonGraphGroupLoadFailureKeepsGraphAndGroups(std::vector<TestFailure>& failures)
{
    const std::string testName = "json graph group load failure keeps graph and groups";
    const std::filesystem::path path = testPath("sdf3d_graph_invalid_groups.json");

    sdf3d::SdfGraph originalSubgraph;
    const sdf3d::SdfGraphNodeId originalSphere = originalSubgraph.createNode(sdf3d::SdfNodeType::Sphere, "Original Sphere");
    expect(originalSubgraph.link(originalSphere, "sdf", originalSubgraph.outputNode(), "surface"), testName, "Expected original group output link.", failures);

    sdf3d::SdfGraph graph;
    const sdf3d::SdfGraphNodeId originalOutput = graph.outputNode();
    sdf3d::GraphGroupRegistry groups;
    const sdf3d::GroupDefId originalDefinitionId = groups.createDefinition("Original Group", originalSubgraph);

    {
        std::ofstream output(path);
        output
            << "{\n"
            << "  \"schema\": \"sdf3d.graph\",\n"
            << "  \"version\": 1,\n"
            << "  \"nextId\": 1,\n"
            << "  \"outputNode\": 999,\n"
            << "  \"materials\": {\"nextId\": 1, \"items\": []},\n"
            << "  \"nodes\": [],\n"
            << "  \"links\": [],\n"
            << "  \"nextDefinitionId\": 3,\n"
            << "  \"definitions\": [{\n"
            << "    \"id\": 2,\n"
            << "    \"name\": \"Loaded Group\",\n"
            << "    \"graph\": {\n"
            << "      \"nextId\": 3,\n"
            << "      \"outputNode\": 1,\n"
            << "      \"materials\": {\"nextId\": 1, \"items\": []},\n"
            << "      \"nodes\": [\n"
            << "        {\"id\": 1, \"type\": \"Output\", \"stableId\": 1, \"name\": \"Output\", \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {}},\n"
            << "        {\"id\": 2, \"type\": \"Sphere\", \"stableId\": 2, \"name\": \"Loaded Sphere\", \"editor\": {\"x\": 0, \"y\": 0, \"propertiesCollapsed\": false}, \"parameters\": {\"radius\": 1.0}}\n"
            << "      ],\n"
            << "      \"links\": [{\"from\": {\"node\": 2, \"socket\": \"sdf\"}, \"to\": {\"node\": 1, \"socket\": \"surface\"}}]\n"
            << "    }\n"
            << "  }]\n"
            << "}\n";
    }

    sdf3d::JsonGraphSerializer serializer;
    expect(!serializer.load(graph, groups, path), testName, "Expected invalid grouped load to fail.", failures);
    expect(!serializer.lastError().empty(), testName, "Expected load error.", failures);
    expect(graph.outputNode() == originalOutput, testName, "Expected graph output unchanged.", failures);
    const sdf3d::GraphGroupDefinition* originalDefinition = groups.definition(originalDefinitionId);
    expect(originalDefinition != nullptr && originalDefinition->name == "Original Group", testName, "Expected original group registry unchanged.", failures);
    expect(groups.definition(2) == nullptr, testName, "Expected loaded group not committed on root graph failure.", failures);

    std::filesystem::remove(path);
}

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testJsonGraphRoundTrip(failures);
    testJsonGraphValueNoiseMaterialRoundTrip(failures);
    testJsonGraphMigratesInlineMaterial(failures);
    testJsonGraphMigratesSourceMaterialNode(failures);
    testJsonGraphLoadRepairsLinkedOverrideMaterialId(failures);
    testJsonGraphGroupDefinitionsRoundTrip(failures);
    testJsonGraphPrunesUnreferencedGroupDefinitions(failures);
    testJsonGraphSavesNestedReachableGroupDefinitions(failures);
    testJsonGraphLoadFailureKeepsGraph(failures);
    testJsonGraphMigratesLegacyBooleanSockets(failures);
    testJsonGraphGroupLoadFailureKeepsGraphAndGroups(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All GraphSerializer tests passed.\n";
    return 0;
}
