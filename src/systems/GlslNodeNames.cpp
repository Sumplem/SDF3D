#include "sdf3d/systems/GlslNodeNames.h"

namespace sdf3d {

std::string glslNodeTypeName(SdfNodeType type)
{
    switch (type) {
    case SdfNodeType::Sphere:
        return "Sphere";
    case SdfNodeType::SphereInstances:
        return "SphereInstances";
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
    case SdfNodeType::ValueNoiseMaterial:
        return "ValueNoiseMaterial";
    case SdfNodeType::MaterialOverride:
        return "MaterialOverride";
    case SdfNodeType::Group:
        return "Group";
    case SdfNodeType::Output:
        return "Output";
    }

    return "Unknown";
}

} // namespace sdf3d
