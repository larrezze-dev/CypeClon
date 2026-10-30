#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::core {

/**
 * @brief Utilidades de cadenas ANSI/UTF-8/UTF-16, codificación Hex y Base64 equivalentes a cypestr.dll.
 */
class StringUtils {
public:
    static std::string ltrim(std::string_view sv);
    static std::string rtrim(std::string_view sv);
    static std::string trim(std::string_view sv);
    static std::string collapse_whitespace(std::string_view sv);

    static std::string to_upper(std::string_view sv);
    static std::string to_lower(std::string_view sv);
    static std::string strip_accents(std::string_view sv);

    static bool equals_ignore_case(std::string_view a, std::string_view b) noexcept;
    static bool starts_with_ignore_case(std::string_view text, std::string_view prefix) noexcept;
    static bool ends_with_ignore_case(std::string_view text, std::string_view suffix) noexcept;

    static std::vector<std::string> split(std::string_view sv, char delimiter, bool trim_tokens = false);
    static std::string replace_all(std::string_view text, std::string_view from, std::string_view to);
    static std::string abbreviate_with_ellipsis(std::string_view text, std::size_t max_len);

    static std::string pad_left(std::string_view text, std::size_t width, char fill = ' ');
    static std::string pad_right(std::string_view text, std::size_t width, char fill = ' ');
    static std::string pad_center(std::string_view text, std::size_t width, char fill = ' ');

    // Conversiones UTF-8 <-> UTF-16 (widestr_* en cypestr.dll)
    static std::wstring utf8_to_wide(std::string_view utf8);
    static std::string wide_to_utf8(std::wstring_view wide);

    // Codificación Hexadecimal y Base64 (cypestrhex_* en cypestr.dll)
    static std::string to_hex(std::span<const std::uint8_t> data, bool uppercase = false);
    static std::vector<std::uint8_t> from_hex(std::string_view hex);

    static std::string to_base64(std::span<const std::uint8_t> data);
    static std::vector<std::uint8_t> from_base64(std::string_view b64);
};

} // namespace cypeclon::core

