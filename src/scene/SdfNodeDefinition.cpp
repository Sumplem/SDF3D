#include "sdf3d/scene/SdfNodeDefinition.h"

#include <utility>

namespace sdf3d {
namespace {

SdfGraphSocket inputSocket(std::string name, SdfSocketType type = SdfSocketType::Sdf, bool multiInput = false)
{
    return {std::move(name), type, SdfSocketDirection::Input, multiInput};
}

SdfGraphSocket outputSocket(std::string name, SdfSocketType type = SdfSocketType::Sdf)
{
    return {std::move(name), type, SdfSocketDirection::Output, false};
}

const std::vector<SdfNodeDefinition>& definitions()
{
    static const std::vector<SdfNodeDefinition> items = {
        {SdfNodeType::Sphere, SdfNodeCategory::Primitive, "Sphere", {{"radius", 1.0f, 0.001f, 100.0f, 0.01f}}, {}, {outputSocket("sdf")}},
        {SdfNodeType::Box, SdfNodeCategory::Primitive, "Box", {{"x", 1.0f, 0.001f, 100.0f, 0.01f}, {"y", 1.0f, 0.001f, 100.0f, 0.01f}, {"z", 1.0f, 0.001f, 100.0f, 0.01f}}, {}, {outputSocket("sdf")}},
        {SdfNodeType::Cylinder, SdfNodeCategory::Primitive, "Cylinder", {{"radius", 1.0f, 0.001f, 100.0f, 0.01f}, {"halfHeight", 1.0f, 0.001f, 100.0f, 0.01f}}, {}, {outputSocket("sdf")}},
        {SdfNodeType::Torus, SdfNodeCategory::Primitive, "Torus", {{"majorRadius", 1.0f, 0.001f, 100.0f, 0.01f}, {"minorRadius", 0.25f, 0.001f, 100.0f, 0.01f}}, {}, {outputSocket("sdf")}},
        {SdfNodeType::Plane, SdfNodeCategory::Primitive, "Plane", {{"normalX", 0.0f, -1.0f, 1.0f, 0.01f}, {"normalY", 1.0f, -1.0f, 1.0f, 0.01f}, {"normalZ", 0.0f, -1.0f, 1.0f, 0.01f}, {"offset", 0.0f, -100.0f, 100.0f, 0.01f}}, {}, {outputSocket("sdf")}},

        {SdfNodeType::Union, SdfNodeCategory::Boolean, "Union", {}, {inputSocket("left"), inputSocket("right")}, {outputSocket("sdf")}},
        {SdfNodeType::Subtract, SdfNodeCategory::Boolean, "Subtract", {}, {inputSocket("base"), inputSocket("cutter")}, {outputSocket("sdf")}},
        {SdfNodeType::Intersect, SdfNodeCategory::Boolean, "Intersect", {}, {inputSocket("left"), inputSocket("right")}, {outputSocket("sdf")}},
        {SdfNodeType::SmoothUnion, SdfNodeCategory::Boolean, "Smooth Union", {{"smoothness", 0.25f, 0.001f, 100.0f, 0.01f}}, {inputSocket("left"), inputSocket("right")}, {outputSocket("sdf")}},
        {SdfNodeType::SmoothSubtract, SdfNodeCategory::Boolean, "Smooth Subtract", {{"smoothness", 0.25f, 0.001f, 100.0f, 0.01f}}, {inputSocket("base"), inputSocket("cutter")}, {outputSocket("sdf")}},
        {SdfNodeType::SmoothIntersect, SdfNodeCategory::Boolean, "Smooth Intersect", {{"smoothness", 0.25f, 0.001f, 100.0f, 0.01f}}, {inputSocket("left"), inputSocket("right")}, {outputSocket("sdf")}},

        {SdfNodeType::Translate, SdfNodeCategory::Transform, "Translate", {{"x", 0.0f, -100.0f, 100.0f, 0.01f}, {"y", 0.0f, -100.0f, 100.0f, 0.01f}, {"z", 0.0f, -100.0f, 100.0f, 0.01f}}, {inputSocket("child")}, {outputSocket("sdf")}},
        {SdfNodeType::Rotate, SdfNodeCategory::Transform, "Rotate", {{"xDegrees", 0.0f, -360.0f, 360.0f, 1.0f}, {"yDegrees", 0.0f, -360.0f, 360.0f, 1.0f}, {"zDegrees", 0.0f, -360.0f, 360.0f, 1.0f}}, {inputSocket("child")}, {outputSocket("sdf")}},
        {SdfNodeType::Scale, SdfNodeCategory::Transform, "Scale", {{"scale", 1.0f, 0.001f, 100.0f, 0.01f}}, {inputSocket("child")}, {outputSocket("sdf")}},

        {SdfNodeType::MaterialOverride, SdfNodeCategory::Material, "Material Override", {}, {inputSocket("sdf")}, {outputSocket("sdf")}},

        {SdfNodeType::Output, SdfNodeCategory::Output, "Output", {}, {inputSocket("surface")}, {}},
    };

    return items;
}

} // namespace

const SdfNodeDefinition* sdfNodeDefinition(SdfNodeType type)
{
    for (const SdfNodeDefinition& definition : definitions()) {
        if (definition.type == type) {
            return &definition;
        }
    }

    return nullptr;
}

std::vector<SdfNodeType> sdfNodeTypesForCategory(SdfNodeCategory category)
{
    std::vector<SdfNodeType> types;
    for (const SdfNodeDefinition& definition : definitions()) {
        if (definition.category == category) {
            types.push_back(definition.type);
        }
    }

    return types;
}

SdfNodePtr makeSdfNodeFromDefinition(SdfNodeType type)
{
    const SdfNodeDefinition* definition = sdfNodeDefinition(type);
    if (definition == nullptr) {
        return makeSdfNode(type);
    }

    SdfNodePtr node = makeSdfNode(type, definition->displayName);
    for (const SdfParameterDefinition& parameter : definition->parameters) {
        node->parameters[parameter.name] = parameter.defaultValue;
    }

    return node;
}

} // namespace sdf3d
