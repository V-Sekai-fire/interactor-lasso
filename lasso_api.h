// SPDX-License-Identifier: MIT
// lasso_api -- the lasso stage's surface in std types. lasso_api.cpp and checks.cpp see godot-lite;
// guest/lasso/main.cpp and the Lean shim (tests/lasso/ffi) see only this header.
#pragma once

#include <string>
#include <vector>

namespace lasso {

// Per target: x, y, z, size, snapping power, visible (0 or 1), snap locked (0 or 1).
constexpr int kStride = 7;

struct Snap {
	int first = -1;
	int second = -1;
	float first_score = 0.0f;
	float second_score = 0.0f;
	bool ok = false;
};

// p_source: the pointer's transform, basis columns x, y, z then the origin (12 values); it points
// along its -z column. p_current: the target snapped last frame, or -1.
Snap snap(const std::vector<double> &p_source, const std::vector<float> &p_targets, int p_current,
		float p_max_increase, float p_increase, bool p_lock);

// The target the joystick (p_dx, p_dy) moves to from p_snapped, seen from p_viewpoint (12 values as
// above); p_snapped when none qualifies, -1 on bad input.
int redirect(int p_snapped, const std::vector<double> &p_viewpoint, const std::vector<float> &p_targets,
		float p_dx, float p_dy);

// LassoPoint objects alive; 0 between calls.
int live_points();

std::string check(const std::string &p_name);
std::string check_all();
std::string check_names();

} // namespace lasso
