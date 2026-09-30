// SPDX-License-Identifier: MIT
// The lasso's named checks, each with the control that must come out the other way. The same TU
// runs in lasso.elf and natively in tests/lasso.
#include "lasso_api.h"

#include "lasso.h"

#include "core/os/memory.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace lasso {

namespace {

const double kIdentity[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
const double kDegree = 3.14159265358979323846 / 180.0;

std::vector<double> identity() {
	return std::vector<double>(kIdentity, kIdentity + 12);
}

// A target r metres from the origin, theta degrees off the -z axis, phi degrees around it.
void add_target(std::vector<float> &r_targets, double p_r, double p_theta, double p_phi, bool p_visible = true) {
	const double t = p_theta * kDegree;
	const double f = p_phi * kDegree;
	const float values[kStride] = { float(p_r * std::sin(t) * std::cos(f)), float(p_r * std::sin(t) * std::sin(f)),
		float(-p_r * std::cos(t)), 0.3f, 1.0f, p_visible ? 1.0f : 0.0f, 1.0f };
	r_targets.insert(r_targets.end(), values, values + kStride);
}

void add_point(std::vector<float> &r_targets, float p_x, float p_y, float p_z) {
	const float values[kStride] = { p_x, p_y, p_z, 0.3f, 1.0f, 1.0f, 1.0f };
	r_targets.insert(r_targets.end(), values, values + kStride);
}

std::string fmt(const char *p_format, ...) {
	char buffer[512];
	va_list args;
	va_start(args, p_format);
	vsnprintf(buffer, sizeof buffer, p_format, args);
	va_end(args);
	return buffer;
}

std::string verdict(bool p_pass, const char *p_name, const std::string &p_detail) {
	return std::string(p_pass ? "PASS " : "FAIL ") + p_name + ": " + p_detail;
}

std::string nearest_axis() {
	std::vector<float> t;
	add_target(t, 2.0, 20.0, 0.0);
	add_target(t, 2.0, 5.0, 90.0);
	add_target(t, 2.0, 12.0, 200.0);
	const Snap s = snap(identity(), t, -1, 0.0f, 0.0f, false);
	std::vector<float> swapped;
	add_target(swapped, 2.0, 5.0, 0.0);
	add_target(swapped, 2.0, 20.0, 90.0);
	add_target(swapped, 2.0, 12.0, 200.0);
	const Snap c = snap(identity(), swapped, -1, 0.0f, 0.0f, false);
	const bool pass = s.ok && s.first == 1 && s.second == 2 && c.ok && c.first == 0 && c.second == 2;
	return verdict(pass, "nearest_axis",
			fmt("targets at 2 m, 20/5/12 deg off the axis: first %d second %d (scores %.6g %.6g); control, "
				"the first two swapped: first %d second %d",
					s.first, s.second, s.first_score, s.second_score, c.first, c.second));
}

std::string nearer_wins() {
	std::vector<float> t;
	add_target(t, 3.0, 8.0, 0.0);
	add_target(t, 1.0, 8.0, 180.0);
	const Snap s = snap(identity(), t, -1, 0.0f, 0.0f, false);
	std::vector<float> swapped;
	add_target(swapped, 1.0, 8.0, 0.0);
	add_target(swapped, 3.0, 8.0, 180.0);
	const Snap c = snap(identity(), swapped, -1, 0.0f, 0.0f, false);
	const bool pass = s.ok && s.first == 1 && c.ok && c.first == 0;
	return verdict(pass, "nearer_wins",
			fmt("two targets 8 deg off the axis at 3 m and 1 m: first %d; control, distances swapped: first %d",
					s.first, c.first));
}

std::string snap_lock() {
	std::vector<float> t;
	add_target(t, 2.0, 20.0, 0.0);
	add_target(t, 2.0, 5.0, 90.0);
	const Snap s = snap(identity(), t, 0, 2.0f, 1.0f, true);
	const Snap c = snap(identity(), t, 0, 2.0f, 1.0f, false);
	const bool pass = s.ok && s.first == 0 && s.second == -1 && c.ok && c.first == 1;
	return verdict(pass, "snap_lock",
			fmt("target 0 held and locked: first %d second %d; control, lock off: first %d", s.first, s.second,
					c.first));
}

std::string hidden() {
	std::vector<float> t;
	add_target(t, 2.0, 20.0, 0.0);
	add_target(t, 2.0, 5.0, 90.0, false);
	const Snap s = snap(identity(), t, -1, 0.0f, 0.0f, false);
	std::vector<float> shown;
	add_target(shown, 2.0, 20.0, 0.0);
	add_target(shown, 2.0, 5.0, 90.0, true);
	const Snap c = snap(identity(), shown, -1, 0.0f, 0.0f, false);
	const bool pass = s.ok && s.first == 0 && s.second == -1 && c.ok && c.first == 1;
	return verdict(pass, "hidden",
			fmt("the nearer target hidden: first %d second %d; control, shown: first %d", s.first, s.second, c.first));
}

// A view turned 0.9 rad about y, then 0.5 rad about x, and moved off the origin; the snapped target
// 2 m ahead of it and one neighbour 0.5 m to each side, all in the view's own frame. p_rolled turns
// the view a quarter turn about its forward axis, so its right is the neighbour that was above.
std::vector<int> redirect_cross(bool p_rolled) {
	const double a = 0.9;
	const double b = 0.5;
	const double yaw[3][3] = { { std::cos(a), 0, std::sin(a) }, { 0, 1, 0 }, { -std::sin(a), 0, std::cos(a) } };
	const double pitch[3][3] = { { 1, 0, 0 }, { 0, std::cos(b), -std::sin(b) }, { 0, std::sin(b), std::cos(b) } };
	double r[3][3];
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			r[i][j] = 0;
			for (int k = 0; k < 3; k++) {
				r[i][j] += yaw[i][k] * pitch[k][j];
			}
		}
	}
	const double move[3] = { 3.0, 1.2, -4.0 };
	const double axes[3][3] = { { 0, 1, 0 }, { -1, 0, 0 }, { 0, 0, 1 } };
	std::vector<double> view(12);
	for (int c = 0; c < 3; c++) {
		for (int row = 0; row < 3; row++) {
			view[c * 3 + row] = p_rolled ? r[row][0] * axes[c][0] + r[row][1] * axes[c][1] + r[row][2] * axes[c][2] : r[row][c];
		}
		view[9 + c] = move[c];
	}
	const float local[5][3] = { { 0, 0, -2 }, { 0.5f, 0, -2 }, { -0.5f, 0, -2 }, { 0, 0.5f, -2 }, { 0, -0.5f, -2 } };
	std::vector<float> t;
	for (int i = 0; i < 5; i++) {
		float w[3];
		for (int row = 0; row < 3; row++) {
			w[row] = float(move[row] + r[row][0] * local[i][0] + r[row][1] * local[i][1] + r[row][2] * local[i][2]);
		}
		add_point(t, w[0], w[1], w[2]);
	}
	return { redirect(0, view, t, 1.0f, 0.0f), redirect(0, view, t, -1.0f, 0.0f), redirect(0, view, t, 0.0f, 1.0f),
		redirect(0, view, t, 0.0f, -1.0f), redirect(0, view, t, 0.0f, 0.0f) };
}

std::string redirect_sides() {
	const std::vector<int> s = redirect_cross(false);
	const std::vector<int> c = redirect_cross(true);
	const bool pass = s == std::vector<int>{ 1, 2, 3, 4, 0 } && c == std::vector<int>{ 3, 4, 2, 1, 0 };
	return verdict(pass, "redirect_sides",
			fmt("in a turned view, joystick right/left/up/down/at rest pick %d/%d/%d/%d/%d (want 1/2/3/4/0); "
				"control, the view rolled a quarter turn: %d/%d/%d/%d/%d (want 3/4/2/1/0)",
					s[0], s[1], s[2], s[3], s[4], c[0], c[1], c[2], c[3], c[4]));
}

std::string no_leak() {
	std::vector<float> t;
	add_target(t, 2.0, 20.0, 0.0);
	add_target(t, 2.0, 5.0, 90.0);
	const int before = live_points();
	for (int i = 0; i < 100; i++) {
		snap(identity(), t, -1, 0.0f, 0.0f, false);
		redirect(0, identity(), t, 1.0f, 0.0f);
	}
	const int after = live_points();
	// Control: a point whose last Ref goes without unregister_point stays alive in the database's Array.
	Ref<LassoDB> db;
	db.instantiate();
	LassoTarget *target = memnew(LassoTarget);
	LassoPoint *raw = nullptr;
	{
		Ref<LassoPoint> point;
		point.instantiate();
		raw = point.ptr();
		point->register_point(db, target);
	}
	const int leaked = live_points() - after;
	{
		Ref<LassoPoint> hold(raw);
		hold->unregister_point();
	}
	memdelete(target);
	const int cleaned = live_points() - after;
	const bool pass = before == 0 && after == 0 && leaked == 1 && cleaned == 0;
	return verdict(pass, "no_leak",
			fmt("LassoPoints alive before %d, after 200 calls %d; control, a point dropped without unregistering: "
				"%d alive (want 1), %d once unregistered",
					before, after, leaked, cleaned));
}

std::string bad_input() {
	const std::vector<float> ragged(kStride + 1, 0.0f);
	const Snap s = snap(identity(), ragged, -1, 0.0f, 0.0f, false);
	const Snap short_source = snap(std::vector<double>(11, 0.0), std::vector<float>(), -1, 0.0f, 0.0f, false);
	std::vector<float> t;
	add_target(t, 2.0, 5.0, 0.0);
	const Snap c = snap(identity(), t, -1, 0.0f, 0.0f, false);
	const bool pass = !s.ok && !short_source.ok && redirect(5, identity(), t, 1.0f, 0.0f) == -1 && c.ok && c.first == 0;
	return verdict(pass, "bad_input",
			fmt("a ragged target array, an 11-value source and an out-of-range snapped index are refused; control, "
				"one well-formed target: first %d",
					c.first));
}

struct Named {
	const char *name;
	std::string (*run)();
};

const Named kChecks[] = {
	{ "nearest_axis", nearest_axis },
	{ "nearer_wins", nearer_wins },
	{ "snap_lock", snap_lock },
	{ "hidden", hidden },
	{ "redirect_sides", redirect_sides },
	{ "no_leak", no_leak },
	{ "bad_input", bad_input },
};

} // namespace

std::string check(const std::string &p_name) {
	for (const Named &c : kChecks) {
		if (p_name == c.name) {
			return c.run();
		}
	}
	return "FAIL " + p_name + ": no such check";
}

std::string check_all() {
	std::string out;
	for (const Named &c : kChecks) {
		out += c.run() + "\n";
	}
	return out;
}

std::string check_names() {
	std::string out;
	for (const Named &c : kChecks) {
		out += std::string(out.empty() ? "" : " ") + c.name;
	}
	return out;
}

} // namespace lasso
