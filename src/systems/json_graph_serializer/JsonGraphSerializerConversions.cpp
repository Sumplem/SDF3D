#include "JsonGraphSerializerConversions.h"

#include "sdf3d/scene/SdfNodeDefinition.h"

#include <optional>
#include <stdexcept>

namespace sdf3d::json_graph_serializer {
namespace {

const char* nodeTypeName(SdfNodeType type)
{
    switch (type) {
    case SdfNodeType::Sphere:
        return "Sphere";
    case SdfNodeType::Box:
        return "Box";
    case SdfNodeType::Cylinder:
        return "Cylinder";
    case SdfNodeType::Torus:
        return "Torus";
    case SdfNodeType::Plane:
        return "Plane";
    case SdfNodeType::Capsule:
        return "Capsule";
    case SdfNodeType::Cone:
        return "Cone";
    case SdfNodeType::RoundBox:
        return "RoundBox";
    case SdfNodeType::Union:
        return "Union";
    case SdfNodeType::SmoothUnion:
        return "SmoothUnion";
    case SdfNodeType::Subtract:
        return "Subtract";
    case SdfNodeType::SmoothSubtract:
        return "SmoothSubtract";
    case SdfNodeType::Intersect:
        return "Intersect";
    case SdfNodeType::SmoothIntersect:
        return "SmoothIntersect";
    case SdfNodeType::Translate:
        return "Translate";
    case SdfNodeType::Rotate:
        return "Rotate";
    case SdfNodeType::Scale:
        return "Scale";
    case SdfNodeType::Repeat:
        return "Repeat";
    case SdfNodeType::Mirror:
        return "Mirror";
    case SdfNodeType::Twist:
        return "Twist";
    case SdfNodeType::Bend:
        return "Bend";
    case SdfNodeType::SolidMaterial:
        return "SolidMaterial";
    case SdfNodeType::CheckerMaterial:
        return "CheckerMaterial";
    case SdfNodeType::MaterialOverride:
        return "MaterialOverride";
    case SdfNodeType::Output:
        return "Output";
    }

    return "Unknown";
}

std::optional<SdfNodeType> parseNodeType(const std::string& name)
{
    for (SdfNodeType type : {
             SdfNodeType::Sphere,
             SdfNodeType::Box,
             SdfNodeType::Cylinder,
             SdfNodeType::Torus,
             SdfNodeType::Plane,
             SdfNodeType::Capsule,
             SdfNodeType::Cone,
             SdfNodeType::RoundBox,
             SdfNodeType::Union,
             SdfNodeType::SmoothUnion,
             SdfNodeType::Subtract,
             SdfNodeType::SmoothSubtract,
             SdfNodeType::Intersect,
             SdfNodeType::SmoothIntersect,
             SdfNodeType::Translate,
             SdfNodeType::Rotate,
             SdfNodeType::Scale,
             SdfNodeType::Repeat,
             SdfNodeType::Mirror,
             SdfNodeType::Twist,
             SdfNodeType::Bend,
             SdfNodeType::SolidMaterial,
             SdfNodeType::CheckerMaterial,
             SdfNodeType::MaterialOverride,
             SdfNodeType::Output,
         }) {
        if (name == nodeTypeName(type)) {
            return type;
        }
    }

    return std::nullopt;
}

const char* socketTypeName(SdfSocketType type)
{
    switch (type) {
    case SdfSocketType::Sdf:
        return "Sdf";
    case SdfSocketType::Float:
        return "Float";
    case SdfSocketType::Vector3:
        return "Vector3";
    case SdfSocketType::Material:
        return "Material";
    }

    return "Unknown";
}

std::optional<SdfSocketType> parseSocketType(const std::string& name)
{
    if (name == "Sdf") {
        return SdfSocketType::Sdf;
    }
    if (name == "Float") {
        return SdfSocketType::Float;
    }
    if (name == "Vector3") {
        return SdfSocketType::Vector3;
    }
    if (name == "Material") {
        return SdfSocketType::Material;
    }

    return std::nullopt;
}

const char* socketDirectionName(SdfSocketDirection direction)
{
    return direction == SdfSocketDirection::Input ? "Input" : "Output";
}

std::optional<SdfSocketDirection> parseSocketDirection(const std::string& name)
{
    if (name == "Input") {
        return SdfSocketDirection::Input;
    }
    if (name == "Output") {
        return SdfSocketDirection::Output;
    }

    return std::nullopt;
}

} // namespace

nlohmann::json socketToJson(const SdfGraphSocket& socket)
{
    return nlohmann::json{
        {"name", socket.name},
        {"type", socketTypeName(socket.type)},
        {"direction", socketDirectionName(socket.direction)},
        {"multiInput", socket.multiInput},
    };
}

SdfGraphSocket socketFromJson(const nlohmann::json& value)
{
    const std::optional<SdfSocketType> type = parseSocketType(value.at("type").get<std::string>());
    const std::optional<SdfSocketDirection> direction = parseSocketDirection(value.at("direction").get<std::string>());
    if (!type || !direction) {
        throw std::runtime_error("Unknown socket type or direction.");
    }

    return {value.at("name").get<std::string>(), *type, *direction, value.at("multiInput").get<bool>()};
}

nlohmann::json materialToJson(const SdfMaterial& material)
{
    return nlohmann::json{
        {"type", static_cast<int>(material.type)},
        {"albedo", {material.albedo.x, material.albedo.y, material.albedo.z}},
        {"secondaryAlbedo", {material.secondaryAlbedo.x, material.secondaryAlbedo.y, material.secondaryAlbedo.z}},
        {"roughness", material.roughness},
        {"metallic", material.metallic},
        {"emission", material.emission},
        {"patternScale", material.patternScale},
    };
}

SdfMaterial materialFromJson(const nlohmann::json& value)
{
    const nlohmann::json& albedo = value.at("albedo");
    if (!albedo.is_array() || albedo.size() != 3) {
        throw std::runtime_error("Material albedo must have exactly three values.");
    }

    SdfMaterial material;
    material.type = static_cast<SdfMaterialType>(value.value("type", 0));
    material.albedo = {albedo.at(0).get<float>(), albedo.at(1).get<float>(), albedo.at(2).get<float>()};
    if (value.contains("secondaryAlbedo")) {
        const nlohmann::json& secondaryAlbedo = value.at("secondaryAlbedo");
        if (!secondaryAlbedo.is_array() || secondaryAlbedo.size() != 3) {
            throw std::runtime_error("Material secondary albedo must have exactly three values.");
        }
        material.secondaryAlbedo = {
            secondaryAlbedo.at(0).get<float>(),
            secondaryAlbedo.at(1).get<float>(),
            secondaryAlbedo.at(2).get<float>(),
        };
    }
    material.roughness = value.at("roughness").get<float>();
    material.metallic = value.at("metallic").get<float>();
    material.emission = value.at("emission").get<float>();
    material.patternScale = value.value("patternScale", 4.0f);
    return material;
}

nlohmann::json materialDefinitionToJson(const MaterialDefinition& material)
{
    return nlohmann::json{
        {"id", material.id},
        {"name", material.name},
        {"material", materialToJson(material.material)},
    };
}

MaterialDefinition materialDefinitionFromJson(const nlohmann::json& value)
{
    return {
        value.at("id").get<MaterialId>(),
        value.at("name").get<std::string>(),
        materialFromJson(value.at("material")),
    };
}

nlohmann::json nodeToJson(const SdfGraphNode& node)
{
    nlohmann::json parameters = nlohmann::json::object();
    for (const auto& [key, value] : node.payload.parameters) {
        parameters[key] = value;
    }

    nlohmann::json value{
        {"id", node.id},
        {"type", nodeTypeName(node.payload.type)},
        {"stableId", node.payload.stableId},
        {"name", node.payload.name},
        {"editor", {{"x", node.editorX}, {"y", node.editorY}, {"propertiesCollapsed", node.editorPropertiesCollapsed}}},
        {"parameters", parameters},
    };
    if (node.payload.materialId != 0) {
        value["materialId"] = node.payload.materialId;
    }
    return value;
}

SdfGraphNode nodeFromJson(const nlohmann::json& value)
{
    const std::optional<SdfNodeType> type = parseNodeType(value.at("type").get<std::string>());
    if (!type) {
        throw std::runtime_error("Unknown graph node type.");
    }

    SdfNode payload{*type, value.at("name").get<std::string>()};
    payload.stableId = value.value("stableId", value.at("id").get<SdfGraphNodeId>());
    if (payload.stableId == 0) {
        payload.stableId = value.at("id").get<SdfGraphNodeId>();
    }
    payload.materialId = value.contains("materialId") ? value.at("materialId").get<MaterialId>() : 0;
    for (const auto& [key, parameter] : value.at("parameters").items()) {
        payload.parameters[key] = parameter.get<float>();
    }
    if (value.contains("material") && (*type == SdfNodeType::SolidMaterial || *type == SdfNodeType::CheckerMaterial || *type == SdfNodeType::MaterialOverride)) {
        payload.material = materialFromJson(value.at("material"));
    }

    const nlohmann::json& editor = value.at("editor");
    SdfGraphNode node{value.at("id").get<SdfGraphNodeId>(), std::move(payload), editor.at("x").get<float>(), editor.at("y").get<float>()};
    node.editorPropertiesCollapsed = editor.at("propertiesCollapsed").get<bool>();
    if (const SdfNodeDefinition* definition = sdfNodeDefinition(*type)) {
        node.inputs = definition->inputs;
        node.outputs = definition->outputs;
    }
    return node;
}

nlohmann::json linkToJson(const SdfGraphLink& link)
{
    return nlohmann::json{
        {"from", {{"node", link.fromNode}, {"socket", link.fromSocket}}},
        {"to", {{"node", link.toNode}, {"socket", link.toSocket}}},
    };
}

SdfGraphLink linkFromJson(const nlohmann::json& value)
{
    return {
        value.at("from").at("node").get<SdfGraphNodeId>(),
        value.at("from").at("socket").get<std::string>(),
        value.at("to").at("node").get<SdfGraphNodeId>(),
        value.at("to").at("socket").get<std::string>(),
    };
}

} // namespace sdf3d::json_graph_serializer
