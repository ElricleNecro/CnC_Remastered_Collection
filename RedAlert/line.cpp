#include "line.h"

#include <cmath>
#include <format>
#include <stdexcept>

void bresenham_line(const rendering::Point start, const rendering::Point end, std::function<void(rendering::Point)> set_pixel) {
	rendering::Point delta = { std::abs(end.x - start.x), std::abs(end.y - start.y) };
	rendering::Point dir = { (end.x > start.x) ? 1 : -1, (end.y > start.y) ? 1 : -1 };

	rendering::Point p = start;
	if (delta.x > delta.y) {
		int error = delta.x / 2;

		while (p.x != end.x) {
			set_pixel(p);

			p.x += dir.x;
			error -= delta.y;
			if (error < 0) {
				p.y += dir.y;
				error += delta.x;
			}
		}
	} else {
		int error = delta.y / 2;

		while (p.y != end.y) {
			set_pixel(p);

			p.y += dir.y;
			error -= delta.x;
			if (error < 0) {
				p.x += dir.x;
				error += delta.y;
			}
		}
	}

	set_pixel(end);
}

static constexpr int ABOVE = 0b1;
static constexpr int BELOW = 0b10;
static constexpr int LEFT = 0b100;
static constexpr int RIGHT = 0b1000;

// TODO: bounding box is currently right exclusive, maybe a source of bug in the future:
constexpr uint8_t compute_code(const rendering::Point p, const rendering::Rect &bounds) {
	uint8_t code = 0b0;

	if (p.y > (bounds.y + bounds.h))
		code |= ABOVE;
	if (p.y < bounds.y)
		code |= BELOW;
	if (p.x < bounds.x)
		code |= LEFT;
	if (p.x > (bounds.x + bounds.w))
		code |= RIGHT;

	return code;
}

rendering::Point find_intersection(const rendering::Point p0,
				   const rendering::Point p1,
				   const uint8_t code,
				   const int xmin,
				   const int xmax,
				   const int ymin,
				   const int ymax) {
	if (code & ABOVE) {
		const int x_new = p0.x + (p1.x - p0.x) * (ymax - p0.y) / (p1.y - p0.y);
		return { x_new, ymax };
	}

	if (code & BELOW) {
		const int x_new = p0.x + (p1.x - p0.x) * (ymin - p0.y) / (p1.y - p0.y);
		return { x_new, ymin };
	}

	if (code & LEFT) {
		const int y_new = p0.y + (p1.y - p0.y) * (xmin - p0.x) / (p1.x - p0.x);
		return { xmin, y_new };
	}

	if (code & RIGHT) {
		const int y_new = p0.y + (p1.y - p0.y) * (xmax - p0.x) / (p1.x - p0.x);
		return { xmax, y_new };
	}

	throw std::domain_error(std::format("Unknown clipping code: {}", code));
}

ClipResult clip_line_cohen_sutherland(const rendering::Point start, const rendering::Point end, const rendering::Rect &bounds) {
	rendering::Point p0 = start, p1 = end;

	const int xmin = bounds.x, xmax = bounds.x + bounds.w;
	const int ymin = bounds.y, ymax = bounds.y + bounds.h;

	while (true) {
		const uint8_t code0 = compute_code(p0, bounds);
		const uint8_t code1 = compute_code(p1, bounds);

		if (code0 == 0 && code1 == 0)
			return { true, p0, p1 };

		if ((code0 & code1) != 0)
			return { false, p0, p1 };

		if (code0 != 0) {
			p0 = find_intersection(p0, p1, code0, xmin, xmax, ymin, ymax);
		} else {
			p1 = find_intersection(p0, p1, code1, xmin, xmax, ymin, ymax);
		}
	}
}
