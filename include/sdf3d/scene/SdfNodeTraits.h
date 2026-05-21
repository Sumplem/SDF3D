#pragma once

#include "sdf3d/scene/SdfNode.h"

namespace sdf3d {

/// Returns true when a node type emits primitive SDF geometry directly.
inline bool isSdfPrimitiveNode(SdfNodeType type)
{
    return type == SdfNodeType::Sphere
        || type == SdfNodeType::Box
        || type == SdfNodeType::Cylinder
        || type == SdfNodeType::Torus
        || type == SdfNodeType::Plane
        || type == SdfNodeType::Capsule
        || type == SdfNodeType::Cone
        || type == SdfNodeType::RoundBox;
}

/// Returns true when a node type combines two or more SDF inputs.
inline bool isSdfBooleanNode(SdfNodeType type)
{
    return type == SdfNodeType::Union
        || type == SdfNodeType::SmoothUnion
        || type == SdfNodeType::Subtract
        || type == SdfNodeType::SmoothSubtract
        || type == SdfNodeType::Intersect
        || type == SdfNodeType::SmoothIntersect;
}

/// Returns true when a node type transforms its child sample domain.
inline bool isSdfTransformNode(SdfNodeType type)
{
    return type == SdfNodeType::Translate
        || type == SdfNodeType::Rotate
        || type == SdfNodeType::Scale
        || type == SdfNodeType::Repeat
        || type == SdfNodeType::Mirror
        || type == SdfNodeType::Twist
        || type == SdfNodeType::Bend;
}

/// Returns true when a node type forwards a single SDF branch without creating a boolean boundary.
inline bool isSdfPassThroughNode(SdfNodeType type)
{
    return isSdfTransformNode(type) || type == SdfNodeType::MaterialOverride || type == SdfNodeType::Group;
}

/// Returns true when a node type emits reusable material data.
inline bool isSdfMaterialNode(SdfNodeType type)
{
    return type == SdfNodeType::SolidMaterial
        || type == SdfNodeType::CheckerMaterial
        || type == SdfNodeType::ValueNoiseMaterial;
}

/// Returns the packed material type owned by a material source node.
inline SdfMaterialType sdfMaterialTypeForNode(SdfNodeType type)
{
    if (type == SdfNodeType::CheckerMaterial) {
        return SdfMaterialType::Checker;
    }
    if (type == SdfNodeType::ValueNoiseMaterial) {
        return SdfMaterialType::ValueNoise;
    }
    return SdfMaterialType::Solid;
}

/// Returns true when the material source uses secondary color and scale fields.
inline bool isSdfPatternMaterialNode(SdfNodeType type)
{
    return type == SdfNodeType::CheckerMaterial
        || type == SdfNodeType::ValueNoiseMaterial;
}

/// Returns true when a transform belongs to canonical affine wrapper order.
inline bool isSdfAffineTransformNode(SdfNodeType type)
{
    return type == SdfNodeType::Scale
        || type == SdfNodeType::Rotate
        || type == SdfNodeType::Translate;
}

} // namespace sdf3d
