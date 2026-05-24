#include "sdf3d/ui/GraphEditorCore.h"

#include <algorithm>

namespace sdf3d::ui {
namespace {

constexpr float TITLE_HEIGHT = 28.0f;
constexpr float NODE_ROUNDING = 6.0f;
constexpr float TITLE_TEXT_X = 10.0f;
constexpr float TITLE_TEXT_Y = 7.0f;
constexpr float ACTION_TOP = 5.0f;
constexpr float ACTION_RIGHT = 5.0f;
constexpr float ACTION_GAP = 4.0f;

} // namespace

void drawGraphNodeShell(const GraphCanvasFrame& frame, ImVec2 position, ImVec2 size, const std::string& title, const GraphNodeStyle& style)
{
    const ImVec2 nodeEnd = {position.x + size.x, position.y + size.y};
    frame.drawList->AddRectFilled(position, nodeEnd, style.bodyColor, scaleValue(frame, NODE_ROUNDING));
    frame.drawList->AddRectFilled(position, {nodeEnd.x, position.y + scaleValue(frame, TITLE_HEIGHT)}, style.titleColor, scaleValue(frame, NODE_ROUNDING), ImDrawFlags_RoundCornersTop);
    frame.drawList->AddRect(position, nodeEnd, style.borderColor, scaleValue(frame, NODE_ROUNDING), 0, scaleValue(frame, style.borderThickness));
    frame.drawList->AddText(
        ImGui::GetFont(),
        ImGui::GetFontSize() * frame.zoom,
        {position.x + scaleValue(frame, TITLE_TEXT_X), position.y + scaleValue(frame, TITLE_TEXT_Y)},
        style.titleTextColor,
        title.c_str());
}

void drawGraphText(const GraphCanvasFrame& frame, ImVec2 position, ImU32 color, const std::string& text)
{
    frame.drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * frame.zoom, position, color, text.c_str());
}

float distanceSquared(ImVec2 a, ImVec2 b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    return x * x + y * y;
}

bool pointInsideRect(ImVec2 point, ImVec2 rectMin, ImVec2 rectMax)
{
    return point.x >= rectMin.x && point.x <= rectMax.x && point.y >= rectMin.y && point.y <= rectMax.y;
}

void drawGraphBezier(const GraphCanvasFrame& frame, ImVec2 from, ImVec2 to, ImU32 color, float thickness, float handleLength)
{
    const float handle = scaleValue(frame, handleLength);
    frame.drawList->AddBezierCubic(from, {from.x + handle, from.y}, {to.x - handle, to.y}, to, color, scaleValue(frame, thickness));
}

void drawGraphSocket(const GraphCanvasFrame& frame, ImVec2 position, ImU32 color, float radius)
{
    frame.drawList->AddCircleFilled(position, scaleValue(frame, radius), color);
}

float graphNodeActionButtonExtent(const GraphCanvasFrame& frame)
{
    return std::max(16.0f, scaleValue(frame, 18.0f));
}

bool drawGraphNodeTitleActionButton(
    const GraphCanvasFrame& frame,
    ImVec2 nodePosition,
    ImVec2 nodeSize,
    int rightIndex,
    const std::string& label,
    bool enabled)
{
    const float buttonExtent = graphNodeActionButtonExtent(frame);
    const float top = nodePosition.y + scaleValue(frame, ACTION_TOP);
    const float x = nodePosition.x + nodeSize.x - buttonExtent - scaleValue(frame, ACTION_RIGHT) - static_cast<float>(rightIndex) * (buttonExtent + scaleValue(frame, ACTION_GAP));

    ImGui::SetCursorScreenPos({x, top});
    if (!enabled) {
        ImGui::BeginDisabled();
    }
    const bool clicked = ImGui::Button(label.c_str(), {buttonExtent, buttonExtent});
    if (!enabled) {
        ImGui::EndDisabled();
    }
    return enabled && clicked;
}

float graphNodeTitleActionWidth(const GraphCanvasFrame& frame, int buttonCount)
{
    if (buttonCount <= 0) {
        return 0.0f;
    }
    return static_cast<float>(buttonCount) * graphNodeActionButtonExtent(frame)
        + scaleValue(frame, ACTION_RIGHT * 2.0f + ACTION_GAP * static_cast<float>(buttonCount - 1));
}

void drawGraphNodeTitleDragRegion(
    const GraphCanvasFrame& frame,
    ImVec2 nodePosition,
    ImVec2 nodeSize,
    float titleHeight,
    float reservedActionWidth,
    const std::string& id)
{
    ImGui::SetCursorScreenPos(nodePosition);
    ImGui::InvisibleButton(id.c_str(), {std::max(1.0f, nodeSize.x - reservedActionWidth), scaleValue(frame, titleHeight)});
}

} // namespace sdf3d::ui
