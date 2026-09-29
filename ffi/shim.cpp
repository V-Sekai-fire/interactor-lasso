// SPDX-License-Identifier: MIT
// Lean bindings for guest/lasso/lasso_api.h. Transforms and targets cross as FloatArrays (doubles),
// in the layout lasso_api.h documents.
#include "lasso_api.h"

#include <lean/lean.h>

#include <string>
#include <vector>

namespace {

std::vector<double> doubles(b_lean_obj_arg p_array) {
	const double *values = lean_float_array_cptr(p_array);
	return std::vector<double>(values, values + lean_sarray_size(p_array));
}

std::vector<float> floats(b_lean_obj_arg p_array) {
	const double *values = lean_float_array_cptr(p_array);
	const size_t n = lean_sarray_size(p_array);
	std::vector<float> out(n);
	for (size_t i = 0; i < n; i++) {
		out[i] = float(values[i]);
	}
	return out;
}

lean_obj_res float_array(const std::vector<double> &p_values) {
	lean_object *out = lean_alloc_sarray(sizeof(double), p_values.size(), p_values.size());
	double *values = lean_float_array_cptr(out);
	for (size_t i = 0; i < p_values.size(); i++) {
		values[i] = p_values[i];
	}
	return out;
}

} // namespace

extern "C" {

// [first, second, first score, second score], or empty on bad input.
lean_obj_res lasso_lean_snap(b_lean_obj_arg p_source, b_lean_obj_arg p_targets, double p_current,
		double p_max_increase, double p_increase, uint8_t p_lock) {
	const lasso::Snap s = lasso::snap(doubles(p_source), floats(p_targets), int(p_current), float(p_max_increase),
			float(p_increase), p_lock != 0);
	if (!s.ok) {
		return float_array({});
	}
	return float_array({ double(s.first), double(s.second), double(s.first_score), double(s.second_score) });
}

double lasso_lean_redirect(double p_snapped, b_lean_obj_arg p_viewpoint, b_lean_obj_arg p_targets, double p_dx,
		double p_dy) {
	return double(lasso::redirect(int(p_snapped), doubles(p_viewpoint), floats(p_targets), float(p_dx), float(p_dy)));
}

lean_obj_res lasso_lean_live_points(lean_obj_arg) {
	return lean_io_result_mk_ok(lean_box_uint32(uint32_t(lasso::live_points())));
}

lean_obj_res lasso_lean_check_all(lean_obj_arg) {
	return lean_io_result_mk_ok(lean_mk_string(lasso::check_all().c_str()));
}

} // extern "C"
