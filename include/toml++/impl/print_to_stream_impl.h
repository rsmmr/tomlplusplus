//# This file is a part of toml++ and is subject to the the terms of the MIT license.
//# Copyright (c) Mark Gillard <mark.gillard@outlook.com.au>
//# See https://github.com/marzer/tomlplusplus/blob/master/LICENSE for the full license text.
// SPDX-License-Identifier: MIT
#pragma once
/// \cond

//# {{
#include "preprocessor.h"
#if !TOML_IMPLEMENTATION
#error This is an implementation-only header.
#endif
//# }}

#include "print_to_stream.h"
#include "source_region.h"
#include "date_time.h"
#include "default_formatter.h"
#include "value.h"
#include "array.h"
#include "table.h"
TOML_DISABLE_WARNINGS;
#include <ostream>
#if TOML_INT_CHARCONV || TOML_FLOAT_CHARCONV
#include <charconv>
#endif
#if !TOML_INT_CHARCONV || !TOML_FLOAT_CHARCONV
#include <sstream>
#endif
#if !TOML_INT_CHARCONV
#include <iomanip>
#endif
TOML_ENABLE_WARNINGS;
#include "header_start.h"

TOML_ANON_NAMESPACE_START
{
#if !TOML_HEADER_ONLY
	using namespace toml;
#endif

	template <typename T>
	inline constexpr size_t charconv_buffer_length = 0;

	template <>
	inline constexpr size_t charconv_buffer_length<int8_t> = 4; // strlen("-128")

	template <>
	inline constexpr size_t charconv_buffer_length<int16_t> = 6; // strlen("-32768")

	template <>
	inline constexpr size_t charconv_buffer_length<int32_t> = 11; // strlen("-2147483648")

	template <>
	inline constexpr size_t charconv_buffer_length<int64_t> = 20; // strlen("-9223372036854775808")

	template <>
	inline constexpr size_t charconv_buffer_length<uint8_t> = 3; // strlen("255")

	template <>
	inline constexpr size_t charconv_buffer_length<uint16_t> = 5; // strlen("65535")

	template <>
	inline constexpr size_t charconv_buffer_length<uint32_t> = 10; // strlen("4294967295")

	template <>
	inline constexpr size_t charconv_buffer_length<uint64_t> = 20; // strlen("18446744073709551615")

	template <>
	inline constexpr size_t charconv_buffer_length<float> = 40;

	template <>
	inline constexpr size_t charconv_buffer_length<double> = 60;

	template <typename T>
	TOML_INTERNAL_LINKAGE
	void print_integer_to_stream(std::ostream & stream, T val, value_flags format = {})
	{
		using namespace toml;

		if (!val)
		{
			stream.put('0');
			return;
		}

		int base = 10;
		if (format != value_flags::none && val >= T{})
		{
			switch (format)
			{
				case value_flags::format_as_binary: base = 2; break;
				case value_flags::format_as_octal: base = 8; break;
				case value_flags::format_as_hexadecimal: base = 16; break;
				default: break;
			}
		}

#if TOML_INT_CHARCONV

		char buf[(sizeof(T) * CHAR_BIT)];
		const auto res = std::to_chars(buf, buf + sizeof(buf), val, base);
		const auto len = static_cast<size_t>(res.ptr - buf);
		if (base == 16)
		{
			for (size_t i = 0; i < len; i++)
				if (buf[i] >= 'a')
					buf[i] -= 32;
		}
		impl::print_to_stream(stream, buf, len);

#else

		using unsigned_type = std::conditional_t<(sizeof(T) > sizeof(unsigned)), std::make_unsigned_t<T>, unsigned>;
		using cast_type		= std::conditional_t<std::is_signed_v<T>, std::make_signed_t<unsigned_type>, unsigned_type>;

		if TOML_UNLIKELY(format == value_flags::format_as_binary)
		{
			bool found_one	   = false;
			const auto v	   = static_cast<unsigned_type>(val);
			unsigned_type mask = unsigned_type{ 1 } << (sizeof(unsigned_type) * CHAR_BIT - 1u);
			for (unsigned i = 0; i < sizeof(unsigned_type) * CHAR_BIT; i++)
			{
				if ((v & mask))
				{
					stream.put('1');
					found_one = true;
				}
				else if (found_one)
					stream.put('0');
				mask >>= 1;
			}
		}
		else
		{
			std::ostringstream ss;
			ss.imbue(std::locale::classic());
			ss << std::uppercase << std::setbase(base);
			ss << static_cast<cast_type>(val);
			const auto str = std::move(ss).str();
			impl::print_to_stream(str, stream);
		}

#endif
	}

	template <typename T>
	TOML_INTERNAL_LINKAGE
	void print_floating_point_to_stream(std::ostream & stream, T val, value_flags format = {})
	{
		using namespace toml;

		switch (impl::fpclassify(val))
		{
			case impl::fp_class::neg_inf: impl::print_to_stream(stream, "-inf"sv); break;

			case impl::fp_class::pos_inf: impl::print_to_stream(stream, "inf"sv); break;

			case impl::fp_class::nan: impl::print_to_stream(stream, "nan"sv); break;

			case impl::fp_class::ok:
			{
				static constexpr auto needs_decimal_point = [](auto&& s) noexcept
				{
					for (auto c : s)
						if (c == '.' || c == 'E' || c == 'e')
							return false;
					return true;
				};

#if TOML_FLOAT_CHARCONV

				char buf[charconv_buffer_length<T>];
				const auto res = !!(format & value_flags::format_as_hexadecimal)
								   ? std::to_chars(buf, buf + sizeof(buf), val, std::chars_format::hex)
								   : std::to_chars(buf, buf + sizeof(buf), val);
				const auto str = std::string_view{ buf, static_cast<size_t>(res.ptr - buf) };
				impl::print_to_stream(stream, str);
				if (!(format & value_flags::format_as_hexadecimal) && needs_decimal_point(str))
					toml::impl::print_to_stream(stream, ".0"sv);

#else

				std::ostringstream ss;
				ss.imbue(std::locale::classic());
				ss.precision(std::numeric_limits<T>::digits10 + 1);
				if (!!(format & value_flags::format_as_hexadecimal))
					ss << std::hexfloat;
				ss << val;
				const auto str = std::move(ss).str();
				impl::print_to_stream(stream, str);
				if (!(format & value_flags::format_as_hexadecimal) && needs_decimal_point(str))
					impl::print_to_stream(stream, ".0"sv);

#endif
			}
			break;

			default: TOML_UNREACHABLE;
		}
	}

	template <typename T>
	TOML_INTERNAL_LINKAGE
	void print_integer_leftpad_zeros(std::ostream & stream, T val, size_t min_digits)
	{
		using namespace toml;

#if TOML_INT_CHARCONV

		char buf[charconv_buffer_length<T>];
		const auto res = std::to_chars(buf, buf + sizeof(buf), val);
		const auto len = static_cast<size_t>(res.ptr - buf);
		for (size_t i = len; i < min_digits; i++)
			stream.put('0');
		impl::print_to_stream(stream, buf, static_cast<size_t>(res.ptr - buf));

#else

		std::ostringstream ss;
		ss.imbue(std::locale::classic());
		using cast_type = std::conditional_t<std::is_signed_v<T>, int64_t, uint64_t>;
		ss << std::setfill('0') << std::setw(static_cast<int>(min_digits)) << static_cast<cast_type>(val);
		const auto str = std::move(ss).str();
		impl::print_to_stream(stream, str);

#endif
	}
}
TOML_ANON_NAMESPACE_END;

TOML_IMPL_NAMESPACE_START
{
	TOML_EXTERNAL_LINKAGE
	TOML_ATTR(nonnull)
	void print_to_stream(std::ostream & stream, const char* val, size_t len)
	{
		stream.write(val, static_cast<std::streamsize>(len));
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, std::string_view val)
	{
		stream.write(val.data(), static_cast<std::streamsize>(val.length()));
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const std::string& val)
	{
		stream.write(val.data(), static_cast<std::streamsize>(val.length()));
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, char val)
	{
		stream.put(val);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, int8_t val, value_flags format)
	{
		TOML_ANON_NAMESPACE::print_integer_to_stream(stream, val, format);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, int16_t val, value_flags format)
	{
		TOML_ANON_NAMESPACE::print_integer_to_stream(stream, val, format);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, int32_t val, value_flags format)
	{
		TOML_ANON_NAMESPACE::print_integer_to_stream(stream, val, format);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, int64_t val, value_flags format)
	{
		TOML_ANON_NAMESPACE::print_integer_to_stream(stream, val, format);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, uint8_t val, value_flags format)
	{
		TOML_ANON_NAMESPACE::print_integer_to_stream(stream, val, format);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, uint16_t val, value_flags format)
	{
		TOML_ANON_NAMESPACE::print_integer_to_stream(stream, val, format);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, uint32_t val, value_flags format)
	{
		TOML_ANON_NAMESPACE::print_integer_to_stream(stream, val, format);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, uint64_t val, value_flags format)
	{
		TOML_ANON_NAMESPACE::print_integer_to_stream(stream, val, format);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, float val, value_flags format)
	{
		TOML_ANON_NAMESPACE::print_floating_point_to_stream(stream, val, format);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, double val, value_flags format)
	{
		TOML_ANON_NAMESPACE::print_floating_point_to_stream(stream, val, format);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, bool val)
	{
		print_to_stream(stream, val ? "true"sv : "false"sv);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const toml::date& val)
	{
		print_integer_leftpad_zeros(stream, val.year, 4_sz);
		stream.put('-');
		print_integer_leftpad_zeros(stream, val.month, 2_sz);
		stream.put('-');
		print_integer_leftpad_zeros(stream, val.day, 2_sz);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const toml::time& val)
	{
		print_integer_leftpad_zeros(stream, val.hour, 2_sz);
		stream.put(':');
		print_integer_leftpad_zeros(stream, val.minute, 2_sz);
		stream.put(':');
		print_integer_leftpad_zeros(stream, val.second, 2_sz);
		if (val.nanosecond && val.nanosecond <= 999999999u)
		{
			stream.put('.');
			auto ns		  = val.nanosecond;
			size_t digits = 9_sz;
			while (ns % 10u == 0u)
			{
				ns /= 10u;
				digits--;
			}
			print_integer_leftpad_zeros(stream, ns, digits);
		}
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const toml::time_offset& val)
	{
		if (!val.minutes)
		{
			stream.put('Z');
			return;
		}

		auto mins = static_cast<int>(val.minutes);
		if (mins < 0)
		{
			stream.put('-');
			mins = -mins;
		}
		else
			stream.put('+');
		const auto hours = mins / 60;
		if (hours)
		{
			print_integer_leftpad_zeros(stream, static_cast<unsigned int>(hours), 2_sz);
			mins -= hours * 60;
		}
		else
			print_to_stream(stream, "00"sv);
		stream.put(':');
		print_integer_leftpad_zeros(stream, static_cast<unsigned int>(mins), 2_sz);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const toml::date_time& val)
	{
		print_to_stream(stream, val.date);
		stream.put('T');
		print_to_stream(stream, val.time);
		if (val.offset)
			print_to_stream(stream, *val.offset);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const source_position& val)
	{
		print_to_stream(stream, "line "sv);
		print_to_stream(stream, val.line);
		print_to_stream(stream, ", column "sv);
		print_to_stream(stream, val.column);
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const source_region& val)
	{
		print_to_stream(stream, val.begin);
		if (val.path)
		{
			print_to_stream(stream, " of '"sv);
			print_to_stream(stream, *val.path);
			stream.put('\'');
		}
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const array& arr)
	{
		stream << default_formatter{ arr };
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const table& tbl)
	{
		stream << default_formatter{ tbl };
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const value<std::string>& val)
	{
		stream << default_formatter{ val };
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const value<int64_t>& val)
	{
		stream << default_formatter{ val };
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const value<double>& val)
	{
		stream << default_formatter{ val };
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const value<bool>& val)
	{
		stream << default_formatter{ val };
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const value<date>& val)
	{
		stream << default_formatter{ val };
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const value<time>& val)
	{
		stream << default_formatter{ val };
	}

	TOML_EXTERNAL_LINKAGE
	void print_to_stream(std::ostream & stream, const value<date_time>& val)
	{
		stream << default_formatter{ val };
	}
}
TOML_IMPL_NAMESPACE_END;

#include "header_end.h"
/// \endcond