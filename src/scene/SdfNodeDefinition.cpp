#include "sdf3d/scene/SdfNodeDefinition.h"

#include "sdf3d/scene/SdfRotationParams.h"
#include "sdf3d/scene/SdfNodeTraits.h"

#include <initializer_list>
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

SdfParameterDefinition floatParameter(std::string name, float defaultValue, float minValue, float maxValue, float step)
{
    return {std::move(name), defaultValue, minValue, maxValue, step};
}

SdfParameterDefinition hiddenFloatParameter(std::string name, float defaultValue, float minValue, float maxValue, float step)
{
    return {std::move(name), defaultValue, minValue, maxValue, step, SdfParameterVisibility::Hidden};
}

SdfParameterDefinition boolParameter(std::string name, float defaultValue)
{
    SdfParameterDefinition parameter;
    parameter.name = std::move(name);
    parameter.defaultValue = defaultValue;
    parameter.type = SdfParameterType::Bool;
    return parameter;
}

SdfParameterDefinition enumParameter(std::string name, int defaultValue, std::initializer_list<SdfParameterEnumValue> values)
{
    SdfParameterDefinition parameter;
    parameter.name = std::move(name);
    parameter.defaultValue = static_cast<float>(defaultValue);
    parameter.type = SdfParameterType::Enum;
    parameter.enumValues = values;
    return parameter;
}

const std::vector<SdfNodeDefinition>& definitions()
{
    static const std::vector<SdfNodeDefinition> items = {
        {SdfNodeType::Sphere, SdfNodeCategory::Primitive, "Sphere", {floatParameter("radius", 1.0f, 0.001f, 100.0f, 0.01f)}, {}, {outputSocket("sdf")}},
        {SdfNodeType::Box, SdfNodeCategory::Primitive, "Box", {floatParameter("x", 1.0f, 0.001f, 100.0f, 0.01f), floatParameter("y", 1.0f, 0.001f, 100.0f, 0.01f), floatParameter("z", 1.0f, 0.001f, 100.0f, 0.01f)}, {}, {outputSocket("sdf")}},
        {SdfNodeType::Cylinder, SdfNodeCategory::Primitive, "Cylinder", {floatParameter("radius", 1.0f, 0.001f, 100.0f, 0.01f), floatParameter("halfHeight", 1.0f, 0.001f, 100.0f, 0.01f)}, {}, {outputSocket("sdf")}},
        {SdfNodeType::Torus, SdfNodeCategory::Primitive, "Torus", {floatParameter("majorRadius", 1.0f, 0.001f, 100.0f, 0.01f), floatParameter("minorRadius", 0.25f, 0.001f, 100.0f, 0.01f)}, {}, {outputSocket("sdf")}},
        {SdfNodeType::Plane, SdfNodeCategory::Primitive, "Plane", {floatParameter("normalX", 0.0f, -1.0f, 1.0f, 0.01f), floatParameter("normalY", 1.0f, -1.0f, 1.0f, 0.01f), floatParameter("normalZ", 0.0f, -1.0f, 1.0f, 0.01f), floatParameter("offset", 0.0f, -100.0f, 100.0f, 0.01f)}, {}, {outputSocket("sdf")}},

        {SdfNodeType::Union, SdfNodeCategory::Boolean, "Union", {}, {inputSocket("inputs", SdfSocketType::Sdf, true)}, {outputSocket("sdf")}},
        {SdfNodeType::Subtract, SdfNodeCategory::Boolean, "Subtract", {}, {inputSocket("base"), inputSocket("cutter")}, {outputSocket("sdf")}},
        {SdfNodeType::Intersect, SdfNodeCategory::Boolean, "Intersect", {}, {inputSocket("inputs", SdfSocketType::Sdf, true)}, {outputSocket("sdf")}},
        {SdfNodeType::SmoothUnion, SdfNodeCategory::Boolean, "Smooth Union", {floatParameter("smoothness", 0.25f, 0.001f, 100.0f, 0.01f)}, {inputSocket("inputs", SdfSocketType::Sdf, true)}, {outputSocket("sdf")}},
        {SdfNodeType::SmoothSubtract, SdfNodeCategory::Boolean, "Smooth Subtract", {floatParameter("smoothness", 0.25f, 0.001f, 100.0f, 0.01f)}, {inputSocket("base"), inputSocket("cutter")}, {outputSocket("sdf")}},
        {SdfNodeType::SmoothIntersect, SdfNodeCategory::Boolean, "Smooth Intersect", {floatParameter("smoothness", 0.25f, 0.001f, 100.0f, 0.01f)}, {inputSocket("inputs", SdfSocketType::Sdf, true)}, {outputSocket("sdf")}},

        {SdfNodeType::Translate, SdfNodeCategory::Transform, "Translate", {floatParameter("x", 0.0f, -100.0f, 100.0f, 0.01f), floatParameter("y", 0.0f, -100.0f, 100.0f, 0.01f), floatParameter("z", 0.0f, -100.0f, 100.0f, 0.01f)}, {inputSocket("child")}, {outputSocket("sdf")}},
        {SdfNodeType::Rotate, SdfNodeCategory::Transform, "Rotate", {
            floatParameter("xDegrees", 0.0f, -360.0f, 360.0f, 1.0f),
            floatParameter("yDegrees", 0.0f, -360.0f, 360.0f, 1.0f),
            floatParameter("zDegrees", 0.0f, -360.0f, 360.0f, 1.0f),
            hiddenFloatParameter(RotateParamQx, 0.0f, -1.0f, 1.0f, 0.01f),
            hiddenFloatParameter(RotateParamQy, 0.0f, -1.0f, 1.0f, 0.01f),
            hiddenFloatParameter(RotateParamQz, 0.0f, -1.0f, 1.0f, 0.01f),
            hiddenFloatParameter(RotateParamQw, 1.0f, -1.0f, 1.0f, 0.01f),
        }, {inputSocket("child")}, {outputSocket("sdf")}},
        {SdfNodeType::Scale, SdfNodeCategory::Transform, "Scale", {floatParameter("x", 1.0f, 0.001f, 100.0f, 0.01f), floatParameter("y", 1.0f, 0.001f, 100.0f, 0.01f), floatParameter("z", 1.0f, 0.001f, 100.0f, 0.01f)}, {inputSocket("child")}, {outputSocket("sdf")}},
        {SdfNodeType::Repeat, SdfNodeCategory::Transform, "Repeat", {floatParameter("x", 2.0f, 0.001f, 100.0f, 0.01f), floatParameter("y", 2.0f, 0.001f, 100.0f, 0.01f), floatParameter("z", 2.0f, 0.001f, 100.0f, 0.01f), boolParameter("repeatX", 1.0f), boolParameter("repeatY", 1.0f), boolParameter("repeatZ", 1.0f)}, {inputSocket("child")}, {outputSocket("sdf")}},
        {SdfNodeType::Mirror, SdfNodeCategory::Transform, "Mirror", {boolParameter("x", 1.0f), boolParameter("y", 0.0f), boolParameter("z", 0.0f)}, {inputSocket("child")}, {outputSocket("sdf")}},
        {SdfNodeType::Twist, SdfNodeCategory::Transform, "Twist", {floatParameter("strength", 1.0f, -20.0f, 20.0f, 0.01f), enumParameter("axis", 1, {{"X", 0}, {"Y", 1}, {"Z", 2}})}, {inputSocket("child")}, {outputSocket("sdf")}},
        {SdfNodeType::Bend, SdfNodeCategory::Transform, "Bend", {floatParameter("strength", 0.5f, -20.0f, 20.0f, 0.01f), enumParameter("axis", 0, {{"X", 0}, {"Y", 1}, {"Z", 2}})}, {inputSocket("child")}, {outputSocket("sdf")}},

        {SdfNodeType::MaterialOverride, SdfNodeCategory::Material, "Material Override", {}, {inputSocket("sdf")}, {outputSocket("sdf")}},

        {SdfNodeType::Group, SdfNodeCategory::Group, "Group", {}, {}, {outputSocket("sdf")}},
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

const SdfParameterDefinition* sdfParameterDefinition(SdfNodeType type, const std::string& name)
{
    const SdfNodeDefinition* definition = sdfNodeDefinition(type);
    if (definition == nullptr) {
        return nullptr;
    }

    for (const SdfParameterDefinition& parameter : definition->parameters) {
        if (parameter.name == name) {
            return &parameter;
        }
    }

    return nullptr;
}

bool isSdfParameterVisible(SdfNodeType type, const std::string& name)
{
    const SdfParameterDefinition* parameter = sdfParameterDefinition(type, name);
    return parameter == nullptr || parameter->visibility == SdfParameterVisibility::Visible;
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
