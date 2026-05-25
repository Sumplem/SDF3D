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

GraphNodeStyle graphNodeInteractionStyle(bool selected, bool hovered, ImU32 titleColor)
{
    GraphNodeStyle style;
    style.bodyColor = selected ? IM_COL32(58, 66, 84, 255) : (hovered ? IM_COL32(48, 52, 62, 255) : IM_COL32(42, 45, 52, 255));
    style.titleColor = titleColor;
    style.borderColor = selected ? IM_COL32(120, 170, 255, 255) : (hovered ? IM_COL32(255, 210, 110, 235) : IM_COL32(78, 82, 92, 255));
    style.borderThickness = selected ? 2.0f : (hovered ? 1.8f : 1.0f);
    return style;
}

ImU32 graphSocketInteractionColor(ImU32 normalColor, ImU32 hoverColor, bool hovered)
{
    return hovered ? hoverColor : normalColor;
}

float graphSocketInteractionRadius(bool hovered)
{
    return hovered ? 8.5f : 6.5f;
}

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

void drawGraphLinkDrag(const GraphCanvasFrame& frame, ImVec2 fixedSocket, bool fixedSocketIsOutput, ImVec2 mouse, ImU32 color)
{
    if (fixedSocketIsOutput) {
        drawGraphBezier(frame, fixedSocket, mouse, color);
        return;
    }
    drawGraphBezier(frame, mouse, fixedSocket, color);
}

bool graphSocketHit(const GraphCanvasFrame& frame, ImVec2 point, ImVec2 socketPosition, float radius)
{
    const float hitRadius = scaleValue(frame, radius);
    return distanceSquared(point, socketPosition) <= hitRadius * hitRadius;
}

void drawGraphSocketDropFeedback(const GraphCanvasFrame& frame, ImVec2 position, ImU32 color)
{
    frame.drawList->AddCircle(position, scaleValue(frame, 10.0f), color, 20, scaleValue(frame, 2.5f));
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
