#pragma once

#include <functional>

#include "renderer.h"

struct ClipResult {
	bool visible;
	rendering::Point start, end;
};

void bresenham_line(const rendering::Point start, const rendering::Point end, std::function<void(rendering::Point)> set_pixel);
ClipResult clip_line_cohen_sutherland(const rendering::Point start, const rendering::Point end, const rendering::Rect &bounds);
