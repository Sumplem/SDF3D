#pragma once

#include "sdf3d/scene/SdfGraph.h"

#include <nlohmann/json.hpp>

namespace sdf3d::json_graph_serializer {

nlohmann::json socketToJson(const SdfGraphSocket& socket);
SdfGraphSocket socketFromJson(const nlohmann::json& value);
nlohmann::json materialToJson(const SdfMaterial& material);
SdfMaterial materialFromJson(const nlohmann::json& value);
nlohmann::json materialDefinitionToJson(const MaterialDefinition& material);
MaterialDefinition materialDefinitionFromJson(const nlohmann::json& value);
nlohmann::json nodeToJson(const SdfGraphNode& node);
SdfGraphNode nodeFromJson(const nlohmann::json& value);
nlohmann::json linkToJson(const SdfGraphLink& link);
SdfGraphLink linkFromJson(const nlohmann::json& value);

} // namespace sdf3d::json_graph_serializer
