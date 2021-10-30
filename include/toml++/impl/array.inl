//# This file is a part of toml++ and is subject to the the terms of the MIT license.
//# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
//# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT
#pragma once

//# {{
#include "preprocessor.h"
#if !TOML_IMPLEMENTATION
#error This is an implementation-only header.
#endif
//# }}

#include "array.h"
#include "header_start.h"

TOML_ANON_NAMESPACE_START
{
	template <typename T, typename U>
	TOML_INTERNAL_LINKAGE
	bool array_is_homogeneous(T & elements, node_type ntype, U & first_nonmatch) noexcept
	{
		if (elements.empty())
		{
			first_nonmatch = {};
			return false;
		}
		if (ntype == node_type::none)
			ntype = elements[0]->type();
		for (const auto& val : elements)
		{
			if (val->type() != ntype)
			{
				first_nonmatch = val.get();
				return false;
			}
		}
		return true;
	}
}
TOML_ANON_NAMESPACE_END;

TOML_NAMESPACE_START
{
	TOML_EXTERNAL_LINKAGE
	array::array(const array& other) //
		: node(other)
	{
		elems_.reserve(other.elems_.size());
		for (const auto& elem : other)
			elems_.emplace_back(impl::make_node(elem));

#if TOML_LIFETIME_HOOKS
		TOML_ARRAY_CREATED;
#endif
	}

	TOML_EXTERNAL_LINKAGE
	array::array(array && other) noexcept //
		: node(std::move(other)),
		  elems_{ std::move(other.elems_) }
	{
#if TOML_LIFETIME_HOOKS
		TOML_ARRAY_CREATED;
#endif
	}

	TOML_EXTERNAL_LINKAGE
	array& array::operator=(const array& rhs)
	{
		if (&rhs != this)
		{
			node::operator=(rhs);
			elems_.clear();
			elems_.reserve(rhs.elems_.size());
			for (const auto& elem : rhs)
				elems_.emplace_back(impl::make_node(elem));
		}
		return *this;
	}

	TOML_EXTERNAL_LINKAGE
	array& array::operator=(array&& rhs) noexcept
	{
		if (&rhs != this)
		{
			node::operator=(std::move(rhs));
			elems_		  = std::move(rhs.elems_);
		}
		return *this;
	}

	TOML_EXTERNAL_LINKAGE
	void array::preinsertion_resize(size_t idx, size_t count)
	{
		TOML_ASSERT(idx <= elems_.size());
		TOML_ASSERT(count >= 1u);
		const auto old_size			= elems_.size();
		const auto new_size			= old_size + count;
		const auto inserting_at_end = idx == old_size;
		elems_.resize(new_size);
		if (!inserting_at_end)
		{
			for (size_t left = old_size, right = new_size - 1u; left-- > idx; right--)
				elems_[right] = std::move(elems_[left]);
		}
	}

	TOML_EXTERNAL_LINKAGE
	bool array::is_homogeneous(node_type ntype) const noexcept
	{
		if (elems_.empty())
			return false;

		if (ntype == node_type::none)
			ntype = elems_[0]->type();

		for (const auto& val : elems_)
			if (val->type() != ntype)
				return false;

		return true;
	}

	TOML_EXTERNAL_LINKAGE
	bool array::is_homogeneous(node_type ntype, node * &first_nonmatch) noexcept
	{
		return TOML_ANON_NAMESPACE::array_is_homogeneous(elems_, ntype, first_nonmatch);
	}

	TOML_EXTERNAL_LINKAGE
	bool array::is_homogeneous(node_type ntype, const node*& first_nonmatch) const noexcept
	{
		return TOML_ANON_NAMESPACE::array_is_homogeneous(elems_, ntype, first_nonmatch);
	}

	TOML_EXTERNAL_LINKAGE
	bool array::equal(const array& lhs, const array& rhs) noexcept
	{
		if (&lhs == &rhs)
			return true;
		if (lhs.elems_.size() != rhs.elems_.size())
			return false;
		for (size_t i = 0, e = lhs.elems_.size(); i < e; i++)
		{
			const auto lhs_type = lhs.elems_[i]->type();
			const node& rhs_	= *rhs.elems_[i];
			const auto rhs_type = rhs_.type();
			if (lhs_type != rhs_type)
				return false;

			const bool equal = lhs.elems_[i]->visit(
				[&](const auto& lhs_) noexcept
				{ return lhs_ == *reinterpret_cast<std::remove_reference_t<decltype(lhs_)>*>(&rhs_); });
			if (!equal)
				return false;
		}
		return true;
	}

	TOML_EXTERNAL_LINKAGE
	size_t array::total_leaf_count() const noexcept
	{
		size_t leaves{};
		for (size_t i = 0, e = elems_.size(); i < e; i++)
		{
			auto arr = elems_[i]->as_array();
			leaves += arr ? arr->total_leaf_count() : size_t{ 1 };
		}
		return leaves;
	}

	TOML_EXTERNAL_LINKAGE
	void array::flatten_child(array && child, size_t & dest_index) noexcept
	{
		for (size_t i = 0, e = child.size(); i < e; i++)
		{
			auto type = child.elems_[i]->type();
			if (type == node_type::array)
			{
				array& arr = *reinterpret_cast<array*>(child.elems_[i].get());
				if (!arr.empty())
					flatten_child(std::move(arr), dest_index);
			}
			else
				elems_[dest_index++] = std::move(child.elems_[i]);
		}
	}

	TOML_EXTERNAL_LINKAGE
	array& array::flatten()&
	{
		if (elems_.empty())
			return *this;

		bool requires_flattening	 = false;
		size_t size_after_flattening = elems_.size();
		for (size_t i = elems_.size(); i-- > 0u;)
		{
			auto arr = elems_[i]->as_array();
			if (!arr)
				continue;
			size_after_flattening--; // discount the array itself
			const auto leaf_count = arr->total_leaf_count();
			if (leaf_count > 0u)
			{
				requires_flattening = true;
				size_after_flattening += leaf_count;
			}
			else
				elems_.erase(elems_.cbegin() + static_cast<ptrdiff_t>(i));
		}

		if (!requires_flattening)
			return *this;

		elems_.reserve(size_after_flattening);

		size_t i = 0;
		while (i < elems_.size())
		{
			auto arr = elems_[i]->as_array();
			if (!arr)
			{
				i++;
				continue;
			}

			std::unique_ptr<node> arr_storage = std::move(elems_[i]);
			const auto leaf_count			  = arr->total_leaf_count();
			if (leaf_count > 1u)
				preinsertion_resize(i + 1u, leaf_count - 1u);
			flatten_child(std::move(*arr), i); // increments i
		}

		return *this;
	}
}
TOML_NAMESPACE_END;

#include "header_end.h"