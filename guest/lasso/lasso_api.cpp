// SPDX-License-Identifier: MIT
#include "lasso_api.h"

#include "lasso.h"

#include "core/math/basis.h"
#include "core/os/memory.h"

namespace lasso {

namespace {

Transform3D transform_of(const std::vector<double> &p_values) {
	const Vector3 x{ real_t(p_values[0]), real_t(p_values[1]), real_t(p_values[2]) };
	const Vector3 y{ real_t(p_values[3]), real_t(p_values[4]), real_t(p_values[5]) };
	const Vector3 z{ real_t(p_values[6]), real_t(p_values[7]), real_t(p_values[8]) };
	const Vector3 origin{ real_t(p_values[9]), real_t(p_values[10]), real_t(p_values[11]) };
	return Transform3D(Basis(x, y, z), origin);
}

// One database for one call. The database's Array holds every point and every point holds the
// database, so the destructor unregisters each point to break that cycle.
struct Scene {
	Ref<LassoDB> db;
	std::vector<LassoTarget *> targets;
	std::vector<Ref<LassoPoint>> points;

	explicit Scene(const std::vector<float> &p_targets) {
		db.instantiate();
		const size_t count = p_targets.size() / kStride;
		for (size_t i = 0; i < count; i++) {
			const float *t = &p_targets[i * kStride];
			LassoTarget *target = memnew(LassoTarget);
			target->set_global_transform(Transform3D(Basis(), Vector3(t[0], t[1], t[2])));
			target->set_visible(t[5] != 0.0f);
			Ref<LassoPoint> point;
			point.instantiate();
			point->set_size(t[3]);
			point->set_snapping_power(t[4]);
			point->set_snap_locked(t[6] != 0.0f);
			point->register_point(db, target);
			targets.push_back(target);
			points.push_back(point);
		}
	}

	~Scene() {
		for (Ref<LassoPoint> &point : points) {
			point->unregister_point();
		}
		points.clear();
		for (LassoTarget *target : targets) {
			memdelete(target);
		}
	}

	int index_of(const Node *p_node) const {
		for (size_t i = 0; i < targets.size(); i++) {
			if (targets[i] == p_node) {
				return int(i);
			}
		}
		return -1;
	}

	Node *node(int p_index) const {
		return p_index >= 0 && size_t(p_index) < targets.size() ? targets[p_index] : nullptr;
	}
};

bool valid_targets(const std::vector<float> &p_targets) {
	return p_targets.size() % kStride == 0;
}

} // namespace

Snap snap(const std::vector<double> &p_source, const std::vector<float> &p_targets, int p_current,
		float p_max_increase, float p_increase, bool p_lock) {
	Snap out;
	if (p_source.size() != 12 || !valid_targets(p_targets)) {
		return out;
	}
	Scene scene(p_targets);
	const Array top = scene.db->calc_top_two_snapping_power(transform_of(p_source), scene.node(p_current),
			p_max_increase, p_increase, p_lock);
	const Ref<LassoPoint> first = top[0];
	const Ref<LassoPoint> second = top[1];
	if (first.is_valid()) {
		out.first = scene.index_of(first->get_origin());
		out.first_score = first->get_snap_score();
	}
	if (second.is_valid()) {
		out.second = scene.index_of(second->get_origin());
		out.second_score = second->get_snap_score();
	}
	out.ok = true;
	return out;
}

int redirect(int p_snapped, const std::vector<double> &p_viewpoint, const std::vector<float> &p_targets,
		float p_dx, float p_dy) {
	if (p_viewpoint.size() != 12 || !valid_targets(p_targets)) {
		return -1;
	}
	Scene scene(p_targets);
	Node *from = scene.node(p_snapped);
	if (from == nullptr) {
		return -1;
	}
	Node *to = scene.db->calc_top_redirecting_power(from, transform_of(p_viewpoint), Vector2(p_dx, p_dy));
	return scene.index_of(to);
}

int live_points() {
	return LassoPoint::live_count();
}

} // namespace lasso
