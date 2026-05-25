#pragma once

#include "sdf3d/scene/MaterialRegistry.h"
#include "sdf3d/scene/SdfCompiler.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>

namespace sdf3d {

inline std::string materialGraphFloat(float value)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(6) << value;
    return out.str();
}

inline std::string materialGraphVec3(const glm::vec3& value)
{
    return "vec3(" + materialGraphFloat(value.x) + ", " + materialGraphFloat(value.y) + ", " + materialGraphFloat(value.z) + ")";
}

inline std::string materialGraphClampVec3(const std::string& expression)
{
    return "clamp(" + expression + ", vec3(0.0), vec3(1.0))";
}

class MaterialGraphCompiler {
public:
    std::string emitMaterialFunction(const MaterialDefinition& material, SdfCompileResult& result) const
    {
        const std::string functionName = functionNameFor(material.id);
        result.materialFunctionByRegistryId[material.id] = functionName;

        const MaterialGraph& graph = material.graph;
        const MaterialGraphLink* outputLink = inputLink(graph, graph.outputNode(), "material");
        std::unordered_set<MaterialGraphNodeId> visiting;
        const std::string expression = outputLink != nullptr
            ? materialExpr(graph, outputLink->fromNode, material.material, result, visiting)
            : fallbackMaterialExpr(material.material);

        std::ostringstream glsl;
        glsl << "SdfMaterialSample " << functionName << "(vec3 p)\n";
        glsl << "{\n";
        glsl << "    return " << expression << ";\n";
        glsl << "}\n\n";
        return glsl.str();
    }

    static std::string functionNameFor(MaterialId id)
    {
        return "sdf3d_material_" + std::to_string(id);
    }

private:
    static const MaterialGraphLink* inputLink(const MaterialGraph& graph, MaterialGraphNodeId node, const std::string& socket)
    {
        for (const MaterialGraphLink& link : graph.links()) {
            if (link.toNode == node && link.toSocket == socket) {
                return &link;
            }
        }
        return nullptr;
    }

    static std::string sampleExpr(const SdfMaterial& material)
    {
        std::ostringstream out;
        out << "SdfMaterialSample(" << materialGraphVec3(material.albedo) << ", "
            << materialGraphFloat(std::clamp(material.roughness, 0.02f, 1.0f)) << ", "
            << materialGraphFloat(std::clamp(material.metallic, 0.0f, 1.0f)) << ", "
            << materialGraphFloat(std::max(material.emission, 0.0f)) << ")";
        return out.str();
    }

    static std::string fallbackMaterialExpr(const SdfMaterial& material)
    {
        return sampleExpr(material);
    }

    static std::string linkedColorOrDefault(
        const MaterialGraph& graph,
        MaterialGraphNodeId node,
        const std::string& socket,
        const glm::vec3& fallback,
        const SdfMaterial& material,
        SdfCompileResult& result,
        std::unordered_set<MaterialGraphNodeId>& visiting)
    {
        const MaterialGraphLink* link = inputLink(graph, node, socket);
        return link == nullptr ? materialGraphVec3(fallback) : colorExpr(graph, link->fromNode, material, result, visiting);
    }

    static std::string linkedFloatOrDefault(
        const MaterialGraph& graph,
        MaterialGraphNodeId node,
        const std::string& socket,
        float fallback,
        const SdfMaterial& material,
        SdfCompileResult& result,
        std::unordered_set<MaterialGraphNodeId>& visiting)
    {
        const MaterialGraphLink* link = inputLink(graph, node, socket);
        return link == nullptr ? materialGraphFloat(fallback) : floatExpr(graph, link->fromNode, material, result, visiting);
    }

    static std::string checkerFactorExpr(
        const MaterialGraph& graph,
        MaterialGraphNodeId id,
        const MaterialGraphNode& node,
        const SdfMaterial& material,
        SdfCompileResult& result,
        std::unordered_set<MaterialGraphNodeId>& visiting)
    {
        const std::string scale = linkedFloatOrDefault(graph, id, "scale", node.value, material, result, visiting);
        const std::string scaled = "max(" + scale + ", 0.0001)";
        return "mod(floor(p.x * " + scaled + ") + floor(p.y * " + scaled + ") + floor(p.z * " + scaled + "), 2.0)";
    }

    static std::string colorExpr(
        const MaterialGraph& graph,
        MaterialGraphNodeId id,
        const SdfMaterial& material,
        SdfCompileResult& result,
        std::unordered_set<MaterialGraphNodeId>& visiting)
    {
        const MaterialGraphNode* node = graph.node(id);
        if (node == nullptr || !visiting.insert(id).second) {
            result.errors.push_back("Invalid material graph color expression.");
            return materialGraphVec3(material.albedo);
        }

        std::string expression;
        switch (node->type) {
        case MaterialGraphNodeType::ColorConstant:
            expression = materialGraphVec3(node->color);
            break;
        case MaterialGraphNodeType::MixColor: {
            const std::string a = linkedColorOrDefault(graph, id, "a", node->color, material, result, visiting);
            const std::string b = linkedColorOrDefault(graph, id, "b", node->secondaryColor, material, result, visiting);
            const std::string factor = linkedFloatOrDefault(graph, id, "factor", node->value, material, result, visiting);
            expression = "mix(" + a + ", " + b + ", clamp(" + factor + ", 0.0, 1.0))";
            break;
        }
        case MaterialGraphNodeType::MultiplyColor: {
            const std::string a = linkedColorOrDefault(graph, id, "a", node->color, material, result, visiting);
            const std::string b = linkedColorOrDefault(graph, id, "b", node->secondaryColor, material, result, visiting);
            const std::string factor = linkedFloatOrDefault(graph, id, "factor", node->value, material, result, visiting);
            expression = "mix(" + a + ", (" + a + " * " + b + "), clamp(" + factor + ", 0.0, 1.0))";
            break;
        }
        case MaterialGraphNodeType::ColorRamp: {
            const std::string factor = linkedFloatOrDefault(graph, id, "factor", node->value, material, result, visiting);
            expression = "mix(" + materialGraphVec3(node->color) + ", " + materialGraphVec3(node->secondaryColor) + ", clamp(" + factor + ", 0.0, 1.0))";
            break;
        }
        case MaterialGraphNodeType::AddColor: {
            const std::string a = linkedColorOrDefault(graph, id, "a", node->color, material, result, visiting);
            const std::string b = linkedColorOrDefault(graph, id, "b", node->secondaryColor, material, result, visiting);
            expression = materialGraphClampVec3("(" + a + " + " + b + ")");
            break;
        }
        case MaterialGraphNodeType::SubtractColor: {
            const std::string a = linkedColorOrDefault(graph, id, "a", node->color, material, result, visiting);
            const std::string b = linkedColorOrDefault(graph, id, "b", node->secondaryColor, material, result, visiting);
            expression = materialGraphClampVec3("(" + a + " - " + b + ")");
            break;
        }
        case MaterialGraphNodeType::CheckerPattern: {
            const std::string a = linkedColorOrDefault(graph, id, "a", node->color, material, result, visiting);
            const std::string b = linkedColorOrDefault(graph, id, "b", node->secondaryColor, material, result, visiting);
            expression = "mix(" + a + ", " + b + ", " + checkerFactorExpr(graph, id, *node, material, result, visiting) + ")";
            break;
        }
        case MaterialGraphNodeType::ValueNoisePattern: {
            const std::string a = linkedColorOrDefault(graph, id, "a", node->color, material, result, visiting);
            const std::string b = linkedColorOrDefault(graph, id, "b", node->secondaryColor, material, result, visiting);
            const std::string scale = linkedFloatOrDefault(graph, id, "scale", node->value, material, result, visiting);
            expression = "mix(" + a + ", " + b + ", sdf3d_valueNoise3d(p * max(" + scale + ", 0.0001)))";
            break;
        }
        default:
            result.errors.push_back("Material graph node does not output color.");
            expression = materialGraphVec3(material.albedo);
            break;
        }

        visiting.erase(id);
        return expression;
    }

    static std::string floatExpr(
        const MaterialGraph& graph,
        MaterialGraphNodeId id,
        const SdfMaterial& material,
        SdfCompileResult& result,
        std::unordered_set<MaterialGraphNodeId>& visiting)
    {
        const MaterialGraphNode* node = graph.node(id);
        if (node == nullptr || !visiting.insert(id).second) {
            result.errors.push_back("Invalid material graph float expression.");
            return "0.0";
        }

        std::string expression;
        if (node->type == MaterialGraphNodeType::FloatConstant) {
            expression = materialGraphFloat(node->value);
        } else if (node->type == MaterialGraphNodeType::CheckerPattern) {
            expression = checkerFactorExpr(graph, id, *node, material, result, visiting);
        } else if (node->type == MaterialGraphNodeType::ValueNoise) {
            const std::string scale = linkedFloatOrDefault(graph, id, "scale", node->value, material, result, visiting);
            expression = "sdf3d_valueNoise3d(p * max(" + scale + ", 0.0001))";
        } else if (node->type == MaterialGraphNodeType::PowerFloat) {
            const std::string base = linkedFloatOrDefault(graph, id, "base", node->value, material, result, visiting);
            const std::string exponent = linkedFloatOrDefault(graph, id, "exponent", node->secondaryValue, material, result, visiting);
            expression = "pow(max(" + base + ", 0.0), " + exponent + ")";
        } else if (node->type == MaterialGraphNodeType::ClampFloat) {
            const std::string value = linkedFloatOrDefault(graph, id, "value", node->value, material, result, visiting);
            const std::string minValue = linkedFloatOrDefault(graph, id, "min", node->secondaryValue, material, result, visiting);
            const std::string maxValue = linkedFloatOrDefault(graph, id, "max", node->tertiaryValue, material, result, visiting);
            expression = "clamp(" + value + ", " + minValue + ", " + maxValue + ")";
        } else {
            result.errors.push_back("Material graph node does not output float.");
            expression = materialGraphFloat(material.roughness);
        }

        visiting.erase(id);
        return expression;
    }

    static std::string materialExpr(
        const MaterialGraph& graph,
        MaterialGraphNodeId id,
        const SdfMaterial& material,
        SdfCompileResult& result,
        std::unordered_set<MaterialGraphNodeId>& visiting)
    {
        const MaterialGraphNode* node = graph.node(id);
        if (node == nullptr || !visiting.insert(id).second) {
            result.errors.push_back("Invalid material graph material expression.");
            return fallbackMaterialExpr(material);
        }

        std::string expression;
        if (node->type == MaterialGraphNodeType::PbrMaterial) {
            const std::string albedo = linkedColorOrDefault(graph, id, "albedo", node->color, material, result, visiting);
            const std::string roughness = linkedFloatOrDefault(graph, id, "roughness", node->roughness, material, result, visiting);
            const std::string metallic = linkedFloatOrDefault(graph, id, "metallic", node->metallic, material, result, visiting);
            const std::string emission = linkedFloatOrDefault(graph, id, "emission", node->emission, material, result, visiting);
            expression = "SdfMaterialSample(" + albedo + ", clamp(" + roughness + ", 0.02, 1.0), clamp(" + metallic + ", 0.0, 1.0), max(" + emission + ", 0.0))";
        } else {
            result.errors.push_back("Material graph node does not output material.");
            expression = fallbackMaterialExpr(material);
        }

        visiting.erase(id);
        return expression;
    }
};

} // namespace sdf3d
