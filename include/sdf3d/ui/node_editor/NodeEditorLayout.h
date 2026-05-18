#pragma once

#include "sdf3d/scene/SdfGraph.h"

namespace sdf3d::node_editor {

struct CanvasFrame;

bool autoLayoutGraph(SdfGraph& graph, const CanvasFrame& frame, bool selectedOnly);

} // namespace sdf3d::node_editor
