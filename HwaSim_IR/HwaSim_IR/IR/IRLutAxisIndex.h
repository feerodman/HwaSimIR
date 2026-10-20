#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

// Immutable prefix tree built once after CSV validation. A query visits only
// the two bracketing branches per axis. Missing/duplicate leaves fail closed.
class IRLutAxisIndex
{
public:
	struct Node
	{
		std::map<double, Node> children;
		std::vector<size_t> rows;
	};
	struct WeightedRow { size_t row; double weight; };
	struct Error
	{
		std::string reason;
		size_t axis = 0;
		double value = 0.0, minimum = 0.0, maximum = 0.0;
	};
	void add(const std::vector<double>& axes, size_t row)
	{
		Node* node = &m_root;
		for (double value : axes) node = &node->children[value];
		node->rows.push_back(row);
	}
	bool query(const std::vector<double>& axes, const std::vector<double>& tolerances,
		std::vector<WeightedRow>& rows, Error& error) const
	{
		rows.clear();
		return visit(m_root, axes, tolerances, 0, 1.0, rows, error);
	}
private:
	static bool visit(const Node& node, const std::vector<double>& axes,
		const std::vector<double>& tolerances, size_t axis, double weight,
		std::vector<WeightedRow>& rows, Error& error)
	{
		if (axis == axes.size())
		{
			if (node.rows.size() != 1)
			{
				error.reason = "cell_missing_or_duplicate";
				error.axis = axis;
				return false;
			}
			WeightedRow item = {node.rows.front(), weight};
			rows.push_back(item);
			return true;
		}
		const auto& children = node.children;
		const double requested = axes[axis];
		error.axis = axis;
		error.value = requested;
		if (children.empty()) { error.reason = "cell_missing"; return false; }
		error.minimum = children.begin()->first;
		error.maximum = children.rbegin()->first;
		auto high = children.lower_bound(requested);
		auto low = high;
		if (low == children.end() || (low != children.begin() && low->first > requested)) --low;
		if (high == children.end()) high = low;
		// Legacy coordinate noise tolerance is capped by the local node spacing.
		// Fine meshes can never inherit a metre-wide snap interval.
		double tolerance = tolerances[axis];
		auto nearest = low;
		if (std::abs(high->first-requested) < std::abs(low->first-requested)) nearest = high;
		auto next = nearest; ++next;
		if (next != children.end()) tolerance = std::min(tolerance, .01*(next->first-nearest->first));
		if (nearest != children.begin())
		{
			auto previous = nearest; --previous;
			tolerance = std::min(tolerance, .01*(nearest->first-previous->first));
		}
		if (std::abs(nearest->first-requested) <= tolerance)
			return visit(nearest->second, axes, tolerances, axis+1, weight, rows, error);
		if (requested < error.minimum || requested > error.maximum)
		{
			error.reason = "out_of_range";
			return false;
		}
		if (low == high) { error.reason = "cell_missing"; return false; }
		const double t = (requested-low->first)/(high->first-low->first);
		return visit(low->second, axes, tolerances, axis+1, weight*(1-t), rows, error) &&
			visit(high->second, axes, tolerances, axis+1, weight*t, rows, error);
	}
	Node m_root;
};
