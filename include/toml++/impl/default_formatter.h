//# This file is a part of toml++ and is subject to the the terms of the MIT license.
//# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
//# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT
#pragma once

#include "formatter.h"
#include "std_vector.h"
#include "header_start.h"

TOML_NAMESPACE_START
{
	/// \brief	A wrapper for printing TOML objects out to a stream as formatted TOML.
	///
	/// \remarks You generally don't need to create an instance of this class explicitly; the stream
	/// 		 operators of the TOML node types already print themselves out using this formatter.
	///
	/// \detail \cpp
	/// auto tbl = toml::table{
	///		{ "description", "This is some TOML, yo." },
	///		{ "fruit", toml::array{ "apple", "orange", "pear" } },
	///		{ "numbers", toml::array{ 1, 2, 3, 4, 5 } },
	///		{ "table", toml::table{ { "foo", "bar" } } }
	/// };
	///
	/// // these two lines are equivalent:
	///	std::cout << toml::default_formatter{ tbl } << "\n";
	///	std::cout << tbl << "\n";
	///
	/// \ecpp
	///
	/// \out
	/// description = "This is some TOML, yo."
	/// fruit = ["apple", "orange", "pear"]
	/// numbers = [1, 2, 3, 4, 5]
	///
	/// [table]
	/// foo = "bar"
	/// \eout
	class default_formatter : impl::formatter
	{
	  private:
		/// \cond

		using base = impl::formatter;
		std::vector<std::string_view> key_path_;
		bool pending_table_separator_ = false;

		static constexpr size_t line_wrap_cols = 120;

		TOML_NODISCARD
		TOML_API
		static size_t count_inline_columns(const node&) noexcept;

		TOML_NODISCARD
		TOML_API
		static bool forces_multiline(const node&, size_t = 0) noexcept;

		TOML_API
		void print_pending_table_separator();

		TOML_API
		void print_key_segment(std::string_view);

		TOML_API
		void print_key_path();

		TOML_API
		void print_inline(const toml::table&);

		TOML_API
		void print(const toml::array&);

		TOML_API
		void print(const toml::table&);

		TOML_API
		void print();

		static constexpr impl::formatter_constants constants = { "inf"sv, "-inf"sv, "nan"sv };
		static constexpr format_flags mandatory_flags		 = format_flags::none;
		static constexpr format_flags ignored_flags			 = format_flags::none;

		/// \endcond

	  public:
		/// \brief	The default flags for a default_formatter.
		static constexpr format_flags default_flags = format_flags::allow_literal_strings	 //
													| format_flags::allow_multi_line_strings //
													| format_flags::allow_value_format_flags //
													| format_flags::indentation;

		/// \brief	Constructs a default formatter and binds it to a TOML object.
		///
		/// \param 	source	The source TOML object.
		/// \param 	flags 	Format option flags.
		TOML_NODISCARD_CTOR
		explicit default_formatter(const toml::node& source, format_flags flags = default_flags) noexcept
			: base{ &source, nullptr, constants, { (flags | mandatory_flags) & ~ignored_flags, "    "sv } }
		{}

#if defined(DOXYGEN) || (TOML_PARSER && !TOML_EXCEPTIONS)

		/// \brief	Constructs a default TOML formatter and binds it to a toml::parse_result.
		///
		/// \availability This constructor is only available when exceptions are disabled.
		///
		/// \attention Formatting a failed parse result will simply dump the error message out as-is.
		///		This will not be valid TOML, but at least gives you something to log or show up in diagnostics:
		/// \cpp
		/// std::cout << toml::default_formatter{ toml::parse("a = 'b'"sv) } // ok
		///           << "\n\n"
		///           << toml::default_formatter{ toml::parse("a = "sv) } // malformed
		///           << "\n";
		/// \ecpp
		/// \out
		/// a = 'b'
		///
		/// Error while parsing key-value pair: encountered end-of-file
		///         (error occurred at line 1, column 5)
		/// \eout
		/// Use the library with exceptions if you want to avoid this scenario.
		///
		/// \param 	result	The parse result.
		/// \param 	flags 	Format option flags.
		TOML_NODISCARD_CTOR
		explicit default_formatter(const toml::parse_result& result, format_flags flags = default_flags) noexcept
			: base{ nullptr, &result, constants, { (flags | mandatory_flags) & ~ignored_flags, "    "sv } }
		{}

#endif

		/// \brief	Prints the bound TOML object out to the stream as formatted TOML.
		friend std::ostream& operator<<(std::ostream& lhs, default_formatter& rhs)
		{
			rhs.attach(lhs);
			rhs.key_path_.clear();
			rhs.print();
			rhs.detach();
			return lhs;
		}

		/// \brief	Prints the bound TOML object out to the stream as formatted TOML (rvalue overload).
		friend std::ostream& operator<<(std::ostream& lhs, default_formatter&& rhs)
		{
			return lhs << rhs; // as lvalue
		}
	};
}
TOML_NAMESPACE_END;

#include "header_end.h"
