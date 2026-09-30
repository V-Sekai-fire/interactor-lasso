// SPDX-License-Identifier: MIT
// lasso.elf -- the lasso stage: which target a pointing cone snaps to, and where a joystick moves the
// snap. This TU sees api.hpp and lasso_api.h only. Every ADD_API_FUNCTION has a no-argument wrapper
// in project/main.gd (AGENTS.md rule 8).
#include <api.hpp>

#include <string>
#include <vector>

#include "lasso_api.h"

static Variant text(const std::string &s) {
	return Variant(String(s));
}

// [first, second, first score, second score]; an empty array on bad input.
static Variant lasso_snap(PackedArray<double> source, PackedArray<float> targets, int current, double max_increase,
		double increase, bool lock) {
	const lasso::Snap s = lasso::snap(source.fetch(), targets.fetch(), current, float(max_increase), float(increase),
			lock);
	std::vector<float> out;
	if (s.ok) {
		out = { float(s.first), float(s.second), s.first_score, s.second_score };
	}
	return Variant(PackedArray<float>(out));
}

static Variant lasso_redirect(int snapped, PackedArray<double> viewpoint, PackedArray<float> targets, double dx,
		double dy) {
	return Variant(int64_t(lasso::redirect(snapped, viewpoint.fetch(), targets.fetch(), float(dx), float(dy))));
}

static Variant lasso_live_points() {
	return Variant(int64_t(lasso::live_points()));
}

static Variant lasso_check(String name) {
	return text(lasso::check(name.utf8()));
}

static Variant lasso_check_all() {
	return text(lasso::check_all());
}

static Variant lasso_check_names() {
	return text(lasso::check_names());
}

int main() {
	ADD_API_FUNCTION(lasso_snap, "PackedFloat32Array",
			"PackedFloat64Array source, PackedFloat32Array targets, int current, float max_increase, float increase, bool lock",
			"The two targets the cone snaps to and their scores (7 floats a target)");
	ADD_API_FUNCTION(lasso_redirect, "int",
			"int snapped, PackedFloat64Array viewpoint, PackedFloat32Array targets, float dx, float dy",
			"The target a joystick moves the snap to");
	ADD_API_FUNCTION(lasso_live_points, "int", "", "LassoPoints alive (0 between calls)");
	ADD_API_FUNCTION(lasso_check, "String", "String name", "One lasso check");
	ADD_API_FUNCTION(lasso_check_all, "String", "", "Every lasso check");
	ADD_API_FUNCTION(lasso_check_names, "String", "", "The lasso check names");
	halt();
}
