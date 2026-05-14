#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace sdf3d {

/// Canonical SDF node taxonomy for Phase 1 and planned Phase 2 extensions.
enum class SdfNodeType {
    Sphere,
    Box,
    Cylinder,
    Torus,
    Plane,
    Capsule,
    Cone,
    RoundBox,
    Union,
    SmoothUnion,
    Subtract,
    SmoothSubtract,
    Intersect,
    SmoothIntersect,
    Translate,
    Rotate,
    Scale,
    Repeat,
    Mirror,
    Twist,
    Bend,
    MaterialOverride,
};

/// Per-node material parameters used by the raymarch shader.
struct SdfMaterial {
    glm::vec3 albedo = {0.8f, 0.8f, 0.8f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float emission = 0.0f;
};

/// Expression-tree node for SDF primitives, boolean ops, domain ops, and material overrides.
struct SdfNode {
    explicit SdfNode(SdfNodeType nodeType, std::string nodeName = {})
        : type(nodeType)
        , name(std::move(nodeName))
    {
    }

    SdfNodeType type;
    std::string name;
    std::unordered_map<std::string, float> parameters;
    std::vector<std::shared_ptr<SdfNode>> children;
    SdfMaterial material;
};

using SdfNodePtr = std::shared_ptr<SdfNode>;

/// Creates a node with shared ownership for use in the scene AST.
inline SdfNodePtr makeSdfNode(SdfNodeType type, std::string name = {})
{
    return std::make_shared<SdfNode>(type, std::move(name));
}

/// Deep-copies an SDF node subtree.
inline SdfNodePtr cloneSdfNodeTree(const SdfNodePtr& node)
{
    if (!node) {
        return nullptr;
    }

    SdfNodePtr clone = makeSdfNode(node->type, node->name);
    clone->parameters = node->parameters;
    clone->material = node->material;
    clone->children.reserve(node->children.size());

    // AGENT: Duplicate operates on arbitrary AST subtrees, so cloning recurses
    // through shared_ptr children instead of relying on node-type constructors.
    for (const SdfNodePtr& child : node->children) {
        clone->children.push_back(cloneSdfNodeTree(child));
    }

    return clone;
}

/// Creates a default unit sphere primitive.
inline SdfNodePtr makeSphereNode(std::string name = "Sphere")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::Sphere, std::move(name));
    // AGENT: A radius parameter keeps the compiler generic enough for live
    // property editing without introducing a primitive-specific subclass.
    node->parameters["radius"] = 1.0f;
    return node;
}

/// Creates a box primitive with half-extents along each axis.
inline SdfNodePtr makeBoxNode(glm::vec3 halfExtents = {1.0f, 1.0f, 1.0f}, std::string name = "Box")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::Box, std::move(name));
    // AGENT: Box extents use separate float parameters so the generic
    // Properties panel can expose them without a vector-specific parameter type.
    node->parameters["x"] = halfExtents.x;
    node->parameters["y"] = halfExtents.y;
    node->parameters["z"] = halfExtents.z;
    return node;
}

/// Creates a capped cylinder primitive aligned to the Y axis.
inline SdfNodePtr makeCylinderNode(float radius = 1.0f, float halfHeight = 1.0f, std::string name = "Cylinder")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::Cylinder, std::move(name));
    node->parameters["radius"] = radius;
    node->parameters["halfHeight"] = halfHeight;
    return node;
}

/// Creates a torus primitive around the Y axis.
inline SdfNodePtr makeTorusNode(float majorRadius = 1.0f, float minorRadius = 0.25f, std::string name = "Torus")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::Torus, std::move(name));
    node->parameters["majorRadius"] = majorRadius;
    node->parameters["minorRadius"] = minorRadius;
    return node;
}

/// Creates a plane primitive with normal and signed offset.
inline SdfNodePtr makePlaneNode(glm::vec3 normal = {0.0f, 1.0f, 0.0f}, float offset = 0.0f, std::string name = "Plane")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::Plane, std::move(name));
    node->parameters["normalX"] = normal.x;
    node->parameters["normalY"] = normal.y;
    node->parameters["normalZ"] = normal.z;
    node->parameters["offset"] = offset;
    return node;
}

/// Creates a union node from a list of child SDF expressions.
inline SdfNodePtr makeUnionNode(std::vector<SdfNodePtr> children, std::string name = "Union")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::Union, std::move(name));
    node->children = std::move(children);
    return node;
}

/// Creates a smooth union node from a list of child SDF expressions.
inline SdfNodePtr makeSmoothUnionNode(std::vector<SdfNodePtr> children, float smoothness = 0.25f, std::string name = "Smooth Union")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::SmoothUnion, std::move(name));
    node->parameters["smoothness"] = smoothness;
    node->children = std::move(children);
    return node;
}

/// Creates a subtract node that removes the second child from the first.
inline SdfNodePtr makeSubtractNode(SdfNodePtr base, SdfNodePtr cutter, std::string name = "Subtract")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::Subtract, std::move(name));
    node->children.push_back(std::move(base));
    node->children.push_back(std::move(cutter));
    return node;
}

/// Creates a smooth subtract node that removes the second child from the first.
inline SdfNodePtr makeSmoothSubtractNode(SdfNodePtr base, SdfNodePtr cutter, float smoothness = 0.25f, std::string name = "Smooth Subtract")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::SmoothSubtract, std::move(name));
    node->parameters["smoothness"] = smoothness;
    node->children.push_back(std::move(base));
    node->children.push_back(std::move(cutter));
    return node;
}

/// Creates an intersect node from a list of child SDF expressions.
inline SdfNodePtr makeIntersectNode(std::vector<SdfNodePtr> children, std::string name = "Intersect")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::Intersect, std::move(name));
    node->children = std::move(children);
    return node;
}

/// Creates a smooth intersect node from a list of child SDF expressions.
inline SdfNodePtr makeSmoothIntersectNode(std::vector<SdfNodePtr> children, float smoothness = 0.25f, std::string name = "Smooth Intersect")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::SmoothIntersect, std::move(name));
    node->parameters["smoothness"] = smoothness;
    node->children = std::move(children);
    return node;
}

/// Creates a translate domain operation node.
inline SdfNodePtr makeTranslateNode(SdfNodePtr child, glm::vec3 offset, std::string name = "Translate")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::Translate, std::move(name));
    node->parameters["x"] = offset.x;
    node->parameters["y"] = offset.y;
    node->parameters["z"] = offset.z;
    node->children.push_back(std::move(child));
    return node;
}

/// Creates a rotate domain operation node using Euler angles in degrees.
inline SdfNodePtr makeRotateNode(SdfNodePtr child, glm::vec3 degrees, std::string name = "Rotate")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::Rotate, std::move(name));
    // AGENT: Euler degrees keep M3 editable with the float-only parameter map;
    // a quaternion UI can be layered on later without changing serialized keys.
    node->parameters["xDegrees"] = degrees.x;
    node->parameters["yDegrees"] = degrees.y;
    node->parameters["zDegrees"] = degrees.z;
    node->children.push_back(std::move(child));
    return node;
}

/// Creates a uniform scale domain operation node.
inline SdfNodePtr makeScaleNode(SdfNodePtr child, float scale, std::string name = "Scale")
{
    SdfNodePtr node = makeSdfNode(SdfNodeType::Scale, std::move(name));
    node->parameters["scale"] = scale;
    node->children.push_back(std::move(child));
    return node;
}

} // namespace sdf3d
