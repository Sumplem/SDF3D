#include "sdf3d/systems/JsonGraphSerializer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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
    const sdf3d::SdfGraphNodeId material = graph.createNode(sdf3d::SdfNodeType::MaterialOverride, "Paint");
    if (sdf3d::SdfGraphNode* sphereNode = graph.node(sphere)) {
        sphereNode->payload.parameters["radius"] = 1.75f;
        sphereNode->editorX = 42.0f;
        sphereNode->editorY = 84.0f;
        sphereNode->editorPropertiesCollapsed = true;
    }
    if (sdf3d::SdfGraphNode* materialNode = graph.node(material)) {
        materialNode->payload.material.albedo = {0.25f, 0.5f, 0.75f};
        materialNode->payload.material.roughness = 0.35f;
        materialNode->payload.material.metallic = 0.2f;
        materialNode->payload.material.emission = 1.25f;
    }
    expect(graph.link(sphere, "sdf", material, "sdf"), testName, "Expected sphere to material link.", failures);
    expect(graph.link(material, "sdf", output, "surface"), testName, "Expected material to output link.", failures);
    expect(graph.setSelectedNodes({sphere, material}, material), testName, "Expected graph selection.", failures);

    expect(serializer.save(graph, path), testName, "Expected save success: " + serializer.lastError(), failures);

    sdf3d::SdfGraph loaded;
    expect(serializer.load(loaded, path), testName, "Expected load success: " + serializer.lastError(), failures);

    const sdf3d::SdfGraphNode* loadedSphere = loaded.node(sphere);
    const sdf3d::SdfGraphNode* loadedMaterial = loaded.node(material);
    expect(loaded.outputNode() == output, testName, "Expected output node ID preserved.", failures);
    expect(loadedSphere != nullptr, testName, "Expected sphere loaded.", failures);
    expect(loadedMaterial != nullptr, testName, "Expected material loaded.", failures);
    if (loadedSphere != nullptr) {
        expect(loadedSphere->payload.name == "Sphere A", testName, "Expected sphere name preserved.", failures);
        expect(loadedSphere->payload.parameters.at("radius") == 1.75f, testName, "Expected sphere radius preserved.", failures);
        expect(loadedSphere->editorX == 42.0f && loadedSphere->editorY == 84.0f, testName, "Expected editor position preserved.", failures);
        expect(loadedSphere->editorPropertiesCollapsed, testName, "Expected editor collapsed state preserved.", failures);
        expect(loadedSphere->outputs.size() == 1 && loadedSphere->outputs[0].name == "sdf", testName, "Expected sockets preserved.", failures);
    }
    if (loadedMaterial != nullptr) {
        expect(loadedMaterial->payload.material.albedo.z == 0.75f, testName, "Expected albedo preserved.", failures);
        expect(loadedMaterial->payload.material.emission == 1.25f, testName, "Expected emission preserved.", failures);
    }
    expect(hasLink(loaded, sphere, "sdf", material, "sdf"), testName, "Expected material input link preserved.", failures);
    expect(hasLink(loaded, material, "sdf", output, "surface"), testName, "Expected output link preserved.", failures);
    expect(loaded.selectedNode() == material, testName, "Expected primary selection preserved.", failures);
    expect(loaded.selectedNodes().size() == 2 && loaded.selectedNodes()[0] == sphere && loaded.selectedNodes()[1] == material, testName, "Expected selection order preserved.", failures);
    expect(loaded.createNode(sdf3d::SdfNodeType::Box, "Next") == 4, testName, "Expected next stable ID preserved.", failures);

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

} // namespace

int main()
{
    std::vector<TestFailure> failures;

    testJsonGraphRoundTrip(failures);
    testJsonGraphLoadFailureKeepsGraph(failures);

    if (!failures.empty()) {
        for (const TestFailure& failure : failures) {
            std::cerr << "[FAIL] " << failure.name << ": " << failure.message << '\n';
        }
        return 1;
    }

    std::cout << "All GraphSerializer tests passed.\n";
    return 0;
}
