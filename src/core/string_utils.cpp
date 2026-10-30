#include "core/string_utils.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace cypeclon::core {

namespace {

bool is_space(unsigned char c) noexcept {
    return std::isspace(c) != 0;
}

unsigned char to_lower_ascii(unsigned char c) noexcept {
    return static_cast<unsigned char>(std::tolower(c));
}

unsigned char to_upper_ascii(unsigned char c) noexcept {
    return static_cast<unsigned char>(std::toupper(c));
}

} // namespace

std::string StringUtils::ltrim(std::string_view sv) {
    std::size_t start = 0;
    while (start < sv.size() && is_space(static_cast<unsigned char>(sv[start]))) {
        ++start;
    }
    return std::string(sv.substr(start));
}

std::string StringUtils::rtrim(std::string_view sv) {
    std::size_t end = sv.size();
    while (end > 0 && is_space(static_cast<unsigned char>(sv[end - 1]))) {
        --end;
    }
    return std::string(sv.substr(0, end));
}

std::string StringUtils::trim(std::string_view sv) {
    return rtrim(ltrim(sv));
}

std::string StringUtils::collapse_whitespace(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    bool in_space = true;
    for (char ch : sv) {
        if (is_space(static_cast<unsigned char>(ch))) {
            if (!in_space) {
                out.push_back(' ');
                in_space = true;
            }
        } else {
            out.push_back(ch);
            in_space = false;
        }
    }
    if (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

std::string StringUtils::to_upper(std::string_view sv) {
    std::string out(sv);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(to_upper_ascii(c));
    });
    return out;
}

std::string StringUtils::to_lower(std::string_view sv) {
    std::string out(sv);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(to_lower_ascii(c));
    });
    return out;
}

std::string StringUtils::strip_accents(std::string_view sv) {
    // Reemplazo rápido de vocales acentuadas UTF-8 comunes en español/portugués/francés
    std::string s(sv);
    const std::pair<std::string_view, std::string_view> replacements[] = {
        {"á", "a"}, {"é", "e"}, {"í", "i"}, {"ó", "o"}, {"ú", "u"}, {"ü", "u"},
        {"Á", "A"}, {"É", "E"}, {"Í", "I"}, {"Ó", "O"}, {"Ú", "U"}, {"Ü", "U"},
        {"à", "a"}, {"è", "e"}, {"ì", "i"}, {"ò", "o"}, {"ù", "u"},
        {"ã", "a"}, {"õ", "o"}, {"â", "a"}, {"ê", "e"}, {"ô", "o"}
    };
    for (const auto& [from, to] : replacements) {
        s = replace_all(s, from, to);
    }
    return s;
}

bool StringUtils::equals_ignore_case(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (to_lower_ascii(static_cast<unsigned char>(a[i])) !=
            to_lower_ascii(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool StringUtils::starts_with_ignore_case(std::string_view text, std::string_view prefix) noexcept {
    if (prefix.size() > text.size()) {
        return false;
    }
    return equals_ignore_case(text.substr(0, prefix.size()), prefix);
}

bool StringUtils::ends_with_ignore_case(std::string_view text, std::string_view suffix) noexcept {
    if (suffix.size() > text.size()) {
        return false;
    }
    return equals_ignore_case(text.substr(text.size() - suffix.size()), suffix);
}

std::vector<std::string> StringUtils::split(std::string_view sv, char delimiter, bool trim_tokens) {
    std::vector<std::string> tokens;
    std::size_t start = 0;
    while (start <= sv.size()) {
        const std::size_t pos = sv.find(delimiter, start);
        std::string_view sub = (pos == std::string_view::npos)
            ? sv.substr(start)
            : sv.substr(start, pos - start);
        tokens.push_back(trim_tokens ? trim(sub) : std::string(sub));
        if (pos == std::string_view::npos) {
            break;
        }
        start = pos + 1;
    }
    return tokens;
}

std::string StringUtils::replace_all(std::string_view text, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return std::string(text);
    }
    std::string result;
    result.reserve(text.size());
    std::size_t pos = 0;
    while (true) {
        const std::size_t match = text.find(from, pos);
        if (match == std::string_view::npos) {
            result.append(text.substr(pos));
            break;
        }
        result.append(text.substr(pos, match - pos));
        result.append(to);
        pos = match + from.size();
    }
    return result;
}

std::string StringUtils::abbreviate_with_ellipsis(std::string_view text, std::size_t max_len) {
    if (text.size() <= max_len) {
        return std::string(text);
    }
    if (max_len <= 3) {
        return std::string(max_len, '.');
    }
    return std::string(text.substr(0, max_len - 3)) + "...";
}

std::string StringUtils::pad_left(std::string_view text, std::size_t width, char fill) {
    if (text.size() >= width) {
        return std::string(text);
    }
    return std::string(width - text.size(), fill) + std::string(text);
}

std::string StringUtils::pad_right(std::string_view text, std::size_t width, char fill) {
    if (text.size() >= width) {
        return std::string(text);
    }
    return std::string(text) + std::string(width - text.size(), fill);
}

std::string StringUtils::pad_center(std::string_view text, std::size_t width, char fill) {
    if (text.size() >= width) {
        return std::string(text);
    }
    const std::size_t total_pad = width - text.size();
    const std::size_t left = total_pad / 2;
    const std::size_t right = total_pad - left;
    return std::string(left, fill) + std::string(text) + std::string(right, fill);
}

std::wstring StringUtils::utf8_to_wide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
#ifdef _WIN32
    const int needed = MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), needed);
    return wide;
#else
    return std::wstring(utf8.begin(), utf8.end());
#endif
}

std::string StringUtils::wide_to_utf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
#ifdef _WIN32
    const int needed = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), needed, nullptr, nullptr);
    return utf8;
#else
    return std::string(wide.begin(), wide.end());
#endif
}

std::string StringUtils::to_hex(std::span<const std::uint8_t> data, bool uppercase) {
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (std::uint8_t b : data) {
        out.push_back(digits[(b >> 4) & 0x0F]);
        out.push_back(digits[b & 0x0F]);
    }
    return out;
}

std::vector<std::uint8_t> StringUtils::from_hex(std::string_view hex) {
    auto nibble = [](char c) -> std::uint8_t {
        if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(10 + (c - 'a'));
        if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(10 + (c - 'A'));
        throw std::invalid_argument("Caracter hexadecimal invalido en cypestrhex");
    };
    if (hex.size() % 2 != 0) {
        throw std::invalid_argument("Longitud hexadecimal impar en cypestrhex");
    }
    std::vector<std::uint8_t> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
    }
    return out;
}

std::string StringUtils::to_base64(std::span<const std::uint8_t> data) {
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 2 < data.size()) {
        const std::uint32_t triple = (static_cast<std::uint32_t>(data[i]) << 16) |
                                     (static_cast<std::uint32_t>(data[i + 1]) << 8) |
                                     static_cast<std::uint32_t>(data[i + 2]);
        out.push_back(kTable[(triple >> 18) & 0x3F]);
        out.push_back(kTable[(triple >> 12) & 0x3F]);
        out.push_back(kTable[(triple >> 6) & 0x3F]);
        out.push_back(kTable[triple & 0x3F]);
        i += 3;
    }
    if (i < data.size()) {
        std::uint32_t triple = static_cast<std::uint32_t>(data[i]) << 16;
        if (i + 1 < data.size()) {
            triple |= static_cast<std::uint32_t>(data[i + 1]) << 8;
        }
        out.push_back(kTable[(triple >> 18) & 0x3F]);
        out.push_back(kTable[(triple >> 12) & 0x3F]);
        out.push_back((i + 1 < data.size()) ? kTable[(triple >> 6) & 0x3F] : '=');
        out.push_back('=');
    }
    return out;
}

std::vector<std::uint8_t> StringUtils::from_base64(std::string_view b64) {
    auto decode_char = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return 26 + (c - 'a');
        if (c >= '0' && c <= '9') return 52 + (c - '0');
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };

    std::vector<std::uint8_t> out;
    out.reserve((b64.size() * 3) / 4);
    std::uint32_t val = 0;
    int bits = -8;
    for (char ch : b64) {
        if (is_space(static_cast<unsigned char>(ch))) {
            continue;
        }
        if (ch == '=') {
            break;
        }
        const int d = decode_char(static_cast<unsigned char>(ch));
        if (d < 0) {
            throw std::invalid_argument("Caracter Base64 invalido en cypestrhex");
        }
        val = (val << 6) | static_cast<std::uint32_t>(d);
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<std::uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

} // namespace cypeclon::core

