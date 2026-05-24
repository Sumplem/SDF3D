#pragma once

#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/ui/GraphCanvas.h"

namespace sdf3d::node_editor {

using CanvasFrame = ui::GraphCanvasFrame;

bool autoLayoutGraph(SdfGraph& graph, const CanvasFrame& frame, bool selectedOnly);

} // namespace sdf3d::node_editor
