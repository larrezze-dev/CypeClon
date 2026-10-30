#include "io/cypesave.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace cypeclon::io {

std::string SchemaVersion::to_string() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

// ==============================================================================
// BinaryWriter Implementation
// ==============================================================================
BinaryWriter::BinaryWriter(SchemaVersion version) : version_(version) {
    buffer_.reserve(1024);
}

void BinaryWriter::write_header() {
    buffer_.insert(buffer_.end(), std::begin(kFileMagic), std::end(kFileMagic));
    write_u32(version_.major);
    write_u32(version_.minor);
    write_u32(version_.patch);
}

void BinaryWriter::write_bool(bool v) {
    write_u8(v ? 1 : 0);
}

void BinaryWriter::write_u8(std::uint8_t v) {
    buffer_.push_back(v);
}

void BinaryWriter::write_u16(std::uint16_t v) {
    const std::uint8_t bytes[2] = {
        static_cast<std::uint8_t>(v & 0xFF),
        static_cast<std::uint8_t>((v >> 8) & 0xFF)
    };
    write_bytes(bytes);
}

void BinaryWriter::write_u32(std::uint32_t v) {
    const std::uint8_t bytes[4] = {
        static_cast<std::uint8_t>(v & 0xFF),
        static_cast<std::uint8_t>((v >> 8) & 0xFF),
        static_cast<std::uint8_t>((v >> 16) & 0xFF),
        static_cast<std::uint8_t>((v >> 24) & 0xFF)
    };
    write_bytes(bytes);
}

void BinaryWriter::write_u64(std::uint64_t v) {
    std::uint8_t bytes[8];
    for (int i = 0; i < 8; ++i) {
        bytes[i] = static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF);
    }
    write_bytes(bytes);
}

void BinaryWriter::write_i32(std::int32_t v) {
    write_u32(static_cast<std::uint32_t>(v));
}

void BinaryWriter::write_i64(std::int64_t v) {
    write_u64(static_cast<std::uint64_t>(v));
}

void BinaryWriter::write_f32(float v) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(float));
    write_u32(bits);
}

void BinaryWriter::write_f64(double v) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(double));
    write_u64(bits);
}

void BinaryWriter::write_bytes(std::span<const std::uint8_t> bytes) {
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
}

void BinaryWriter::write_string_utf8(std::string_view s) {
    write_u32(static_cast<std::uint32_t>(s.size()));
    buffer_.insert(buffer_.end(),
                   reinterpret_cast<const std::uint8_t*>(s.data()),
                   reinterpret_cast<const std::uint8_t*>(s.data()) + s.size());
}

void BinaryWriter::write_wstring(std::wstring_view ws) {
    write_u32(static_cast<std::uint32_t>(ws.size()));
    for (wchar_t ch : ws) {
        write_u16(static_cast<std::uint16_t>(ch));
    }
}

void BinaryWriter::write_vec2d(const core::Vec2d& v) {
    write_f64(v.x);
    write_f64(v.y);
}

void BinaryWriter::write_vec3d(const core::Vec3d& v) {
    write_f64(v.x);
    write_f64(v.y);
    write_f64(v.z);
}

void BinaryWriter::write_array_u32(std::span<const std::uint32_t> arr) {
    write_u32(static_cast<std::uint32_t>(arr.size()));
    for (std::uint32_t v : arr) write_u32(v);
}

void BinaryWriter::write_array_f64(std::span<const double> arr) {
    write_u32(static_cast<std::uint32_t>(arr.size()));
    for (double v : arr) write_f64(v);
}

void BinaryWriter::write_array_vec2d(std::span<const core::Vec2d> arr) {
    write_u32(static_cast<std::uint32_t>(arr.size()));
    for (const auto& p : arr) write_vec2d(p);
}

void BinaryWriter::write_array_vec3d(std::span<const core::Vec3d> arr) {
    write_u32(static_cast<std::uint32_t>(arr.size()));
    for (const auto& p : arr) write_vec3d(p);
}

void BinaryWriter::write_array_strings(std::span<const std::string> arr) {
    write_u32(static_cast<std::uint32_t>(arr.size()));
    for (const auto& s : arr) write_string_utf8(s);
}

std::size_t BinaryWriter::begin_block(std::uint32_t block_tag, std::uint32_t block_version) {
    write_u32(0x4B4C4243u); // Magic 'CBLK'
    write_u32(block_tag);
    write_u32(block_version);
    const std::size_t size_offset = buffer_.size();
    write_u64(0); // Placeholder para el tamaño del bloque
    return size_offset;
}

void BinaryWriter::end_block(std::size_t block_token) {
    if (block_token + 8 > buffer_.size()) {
        throw std::out_of_range("Token de bloque cypesave invalido");
    }
    const std::uint64_t payload_size = static_cast<std::uint64_t>(buffer_.size() - (block_token + 8));
    for (int i = 0; i < 8; ++i) {
        buffer_[block_token + i] = static_cast<std::uint8_t>((payload_size >> (i * 8)) & 0xFF);
    }
}

std::size_t BinaryWriter::begin_section(std::string_view section_name, std::uint32_t section_version) {
    write_u32(0x43455343u); // Magic 'CSEC'
    write_string_utf8(section_name);
    write_u32(section_version);
    const std::size_t size_offset = buffer_.size();
    write_u64(0);
    return size_offset;
}

void BinaryWriter::end_section(std::size_t section_token) {
    end_block(section_token);
}

bool BinaryWriter::save_to_file(const std::filesystem::path& path) const {
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs.write(reinterpret_cast<const char*>(buffer_.data()), static_cast<std::streamsize>(buffer_.size()));
    return ofs.good();
}

// ==============================================================================
// BinaryReader Implementation
// ==============================================================================
BinaryReader::BinaryReader(std::vector<std::uint8_t> data) : data_(std::move(data)) {}

std::optional<BinaryReader> BinaryReader::load_from_file(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) return std::nullopt;
    const auto sz = ifs.tellg();
    if (sz < 0) return std::nullopt;
    ifs.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
    if (sz > 0) {
        ifs.read(reinterpret_cast<char*>(buf.data()), sz);
        if (!ifs) return std::nullopt;
    }
    return BinaryReader(std::move(buf));
}

void BinaryReader::require_bytes(std::size_t count) const {
    if (pos_ + count > data_.size()) {
        throw std::runtime_error("Fin de buffer inesperado en BinaryReader (cypesave)");
    }
}

bool BinaryReader::read_and_verify_header() {
    if (pos_ + 20 > data_.size()) return false;
    if (std::memcmp(data_.data() + pos_, BinaryWriter::kFileMagic, 8) != 0) {
        return false;
    }
    pos_ += 8;
    version_.major = read_u32();
    version_.minor = read_u32();
    version_.patch = read_u32();
    return true;
}

void BinaryReader::set_position(std::size_t new_pos) {
    if (new_pos > data_.size()) {
        throw std::out_of_range("Posicion fuera de rango en cypesave_set_posicion");
    }
    pos_ = new_pos;
}

bool BinaryReader::read_bool() {
    return read_u8() != 0;
}

std::uint8_t BinaryReader::read_u8() {
    require_bytes(1);
    return data_[pos_++];
}

std::uint16_t BinaryReader::read_u16() {
    require_bytes(2);
    const std::uint16_t v = static_cast<std::uint16_t>(data_[pos_]) |
                            (static_cast<std::uint16_t>(data_[pos_ + 1]) << 8);
    pos_ += 2;
    return v;
}

std::uint32_t BinaryReader::read_u32() {
    require_bytes(4);
    const std::uint32_t v = static_cast<std::uint32_t>(data_[pos_]) |
                            (static_cast<std::uint32_t>(data_[pos_ + 1]) << 8) |
                            (static_cast<std::uint32_t>(data_[pos_ + 2]) << 16) |
                            (static_cast<std::uint32_t>(data_[pos_ + 3]) << 24);
    pos_ += 4;
    return v;
}

std::uint64_t BinaryReader::read_u64() {
    require_bytes(8);
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= (static_cast<std::uint64_t>(data_[pos_ + i]) << (i * 8));
    }
    pos_ += 8;
    return v;
}

std::int32_t BinaryReader::read_i32() {
    return static_cast<std::int32_t>(read_u32());
}

std::int64_t BinaryReader::read_i64() {
    return static_cast<std::int64_t>(read_u64());
}

float BinaryReader::read_f32() {
    const std::uint32_t bits = read_u32();
    float v = 0.0f;
    std::memcpy(&v, &bits, sizeof(float));
    return v;
}

double BinaryReader::read_f64() {
    const std::uint64_t bits = read_u64();
    double v = 0.0;
    std::memcpy(&v, &bits, sizeof(double));
    return v;
}

std::vector<std::uint8_t> BinaryReader::read_bytes(std::size_t count) {
    require_bytes(count);
    std::vector<std::uint8_t> out(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                                  data_.begin() + static_cast<std::ptrdiff_t>(pos_ + count));
    pos_ += count;
    return out;
}

std::string BinaryReader::read_string_utf8() {
    const std::uint32_t len = read_u32();
    require_bytes(len);
    std::string s(reinterpret_cast<const char*>(data_.data() + pos_), len);
    pos_ += len;
    return s;
}

std::wstring BinaryReader::read_wstring() {
    const std::uint32_t len = read_u32();
    require_bytes(static_cast<std::size_t>(len) * 2);
    std::wstring ws;
    ws.reserve(len);
    for (std::uint32_t i = 0; i < len; ++i) {
        ws.push_back(static_cast<wchar_t>(read_u16()));
    }
    return ws;
}

core::Vec2d BinaryReader::read_vec2d() {
    const double x = read_f64();
    const double y = read_f64();
    return {x, y};
}

core::Vec3d BinaryReader::read_vec3d() {
    const double x = read_f64();
    const double y = read_f64();
    const double z = read_f64();
    return {x, y, z};
}

std::vector<std::uint32_t> BinaryReader::read_array_u32() {
    const std::uint32_t count = read_u32();
    require_bytes(static_cast<std::size_t>(count) * 4);
    std::vector<std::uint32_t> arr(count);
    for (std::uint32_t i = 0; i < count; ++i) arr[i] = read_u32();
    return arr;
}

std::vector<double> BinaryReader::read_array_f64() {
    const std::uint32_t count = read_u32();
    require_bytes(static_cast<std::size_t>(count) * 8);
    std::vector<double> arr(count);
    for (std::uint32_t i = 0; i < count; ++i) arr[i] = read_f64();
    return arr;
}

std::vector<core::Vec2d> BinaryReader::read_array_vec2d() {
    const std::uint32_t count = read_u32();
    require_bytes(static_cast<std::size_t>(count) * 16);
    std::vector<core::Vec2d> arr(count);
    for (std::uint32_t i = 0; i < count; ++i) arr[i] = read_vec2d();
    return arr;
}

std::vector<core::Vec3d> BinaryReader::read_array_vec3d() {
    const std::uint32_t count = read_u32();
    require_bytes(static_cast<std::size_t>(count) * 24);
    std::vector<core::Vec3d> arr(count);
    for (std::uint32_t i = 0; i < count; ++i) arr[i] = read_vec3d();
    return arr;
}

std::vector<std::string> BinaryReader::read_array_strings() {
    const std::uint32_t count = read_u32();
    std::vector<std::string> arr;
    arr.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) arr.push_back(read_string_utf8());
    return arr;
}

BinaryReader::BlockHeader BinaryReader::begin_block() {
    const std::uint32_t magic = read_u32();
    if (magic != 0x4B4C4243u) {
        throw std::runtime_error("Marca de bloque CBLK invalida en cypesave");
    }
    BlockHeader hdr;
    hdr.tag = read_u32();
    hdr.version = read_u32();
    hdr.payload_size = read_u64();
    hdr.end_offset = pos_ + static_cast<std::size_t>(hdr.payload_size);
    if (hdr.end_offset > data_.size()) {
        throw std::runtime_error("Tamano de bloque corrupto en cypesave");
    }
    return hdr;
}

void BinaryReader::end_block(const BlockHeader& blk) {
    pos_ = blk.end_offset;
}

void BinaryReader::skip_block(const BlockHeader& blk) {
    pos_ = blk.end_offset;
}

BinaryReader::SectionHeader BinaryReader::begin_section() {
    const std::uint32_t magic = read_u32();
    if (magic != 0x43455343u) {
        throw std::runtime_error("Marca de seccion CSEC invalida en cypesave");
    }
    SectionHeader hdr;
    hdr.name = read_string_utf8();
    hdr.version = read_u32();
    hdr.payload_size = read_u64();
    hdr.end_offset = pos_ + static_cast<std::size_t>(hdr.payload_size);
    if (hdr.end_offset > data_.size()) {
        throw std::runtime_error("Tamano de seccion corrupto en cypesave");
    }
    return hdr;
}

void BinaryReader::end_section(const SectionHeader& sec) {
    pos_ = sec.end_offset;
}

void BinaryReader::skip_section(const SectionHeader& sec) {
    pos_ = sec.end_offset;
}

// ==============================================================================
// JsonValue Implementation
// ==============================================================================
bool JsonValue::as_bool(bool default_val) const noexcept {
    if (const auto* p = std::get_if<bool>(&storage_)) return *p;
    return default_val;
}

double JsonValue::as_number(double default_val) const noexcept {
    if (const auto* p = std::get_if<double>(&storage_)) return *p;
    return default_val;
}

std::int64_t JsonValue::as_int64(std::int64_t default_val) const noexcept {
    if (const auto* p = std::get_if<double>(&storage_)) return static_cast<std::int64_t>(*p);
    return default_val;
}

const std::string& JsonValue::as_string() const {
    static const std::string kEmpty;
    if (const auto* p = std::get_if<std::string>(&storage_)) return *p;
    return kEmpty;
}

const JsonValue::Array& JsonValue::as_array() const {
    static const Array kEmpty;
    if (const auto* p = std::get_if<Array>(&storage_)) return *p;
    return kEmpty;
}

const JsonValue::Object& JsonValue::as_object() const {
    static const Object kEmpty;
    if (const auto* p = std::get_if<Object>(&storage_)) return *p;
    return kEmpty;
}

JsonValue::Array& JsonValue::as_array_mut() {
    if (!is_array()) storage_ = Array{};
    return std::get<Array>(storage_);
}

JsonValue::Object& JsonValue::as_object_mut() {
    if (!is_object()) storage_ = Object{};
    return std::get<Object>(storage_);
}

JsonValue& JsonValue::operator[](const std::string& key) {
    return as_object_mut()[key];
}

const JsonValue& JsonValue::get(const std::string& key) const {
    static const JsonValue kNull;
    if (const auto* obj = std::get_if<Object>(&storage_)) {
        auto it = obj->find(key);
        if (it != obj->end()) return it->second;
    }
    return kNull;
}

bool JsonValue::contains(const std::string& key) const noexcept {
    if (const auto* obj = std::get_if<Object>(&storage_)) {
        return obj->find(key) != obj->end();
    }
    return false;
}

namespace {

void escape_json_string(std::string_view sv, std::string& out) {
    out.push_back('"');
    for (char c : sv) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
    out.push_back('"');
}

class SimpleJsonParser {
public:
    explicit SimpleJsonParser(std::string_view text) : text_(text) {}

    std::optional<JsonValue> parse_root() {
        skip_ws();
        auto val = parse_value();
        if (!val) return std::nullopt;
        skip_ws();
        if (pos_ != text_.size()) return std::nullopt;
        return val;
    }

private:
    void skip_ws() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    std::optional<JsonValue> parse_value() {
        skip_ws();
        if (pos_ >= text_.size()) return std::nullopt;
        const char c = text_[pos_];
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') {
            auto s = parse_string();
            if (!s) return std::nullopt;
            return JsonValue(std::move(*s));
        }
        if (c == 't') return parse_literal("true", JsonValue(true));
        if (c == 'f') return parse_literal("false", JsonValue(false));
        if (c == 'n') return parse_literal("null", JsonValue(nullptr));
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            return parse_number();
        }
        return std::nullopt;
    }

    std::optional<JsonValue> parse_literal(std::string_view lit, JsonValue val) {
        if (text_.substr(pos_, lit.size()) == lit) {
            pos_ += lit.size();
            return val;
        }
        return std::nullopt;
    }

    std::optional<std::string> parse_string() {
        if (pos_ >= text_.size() || text_[pos_] != '"') return std::nullopt;
        ++pos_;
        std::string out;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') return out;
            if (c == '\\') {
                if (pos_ >= text_.size()) return std::nullopt;
                const char esc = text_[pos_++];
                switch (esc) {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    default:   out.push_back(esc); break;
                }
            } else {
                out.push_back(c);
            }
        }
        return std::nullopt;
    }

    std::optional<JsonValue> parse_number() {
        const char* start = text_.data() + pos_;
        std::string temp(text_.substr(pos_));
        char* end_ptr = nullptr;
        const double val = std::strtod(temp.c_str(), &end_ptr);
        if (end_ptr == temp.c_str()) return std::nullopt;
        pos_ += static_cast<std::size_t>(end_ptr - temp.c_str());
        (void)start;
        return JsonValue(val);
    }

    std::optional<JsonValue> parse_array() {
        ++pos_; // '['
        JsonValue::Array arr;
        skip_ws();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return JsonValue(std::move(arr));
        }
        while (pos_ < text_.size()) {
            auto elem = parse_value();
            if (!elem) return std::nullopt;
            arr.push_back(std::move(*elem));
            skip_ws();
            if (pos_ < text_.size() && text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < text_.size() && text_[pos_] == ']') {
                ++pos_;
                return JsonValue(std::move(arr));
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    std::optional<JsonValue> parse_object() {
        ++pos_; // '{'
        JsonValue::Object obj;
        skip_ws();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return JsonValue(std::move(obj));
        }
        while (pos_ < text_.size()) {
            skip_ws();
            auto key = parse_string();
            if (!key) return std::nullopt;
            skip_ws();
            if (pos_ >= text_.size() || text_[pos_] != ':') return std::nullopt;
            ++pos_;
            auto val = parse_value();
            if (!val) return std::nullopt;
            obj[*key] = std::move(*val);
            skip_ws();
            if (pos_ < text_.size() && text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            if (pos_ < text_.size() && text_[pos_] == '}') {
                ++pos_;
                return JsonValue(std::move(obj));
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    std::string_view text_;
    std::size_t pos_{0};
};

} // namespace

void JsonValue::dump_impl(std::string& out, int indent, int depth) const {
    const auto pad = [&](int d) {
        if (indent > 0) out.append(static_cast<std::size_t>(d * indent), ' ');
    };
    const auto nl = [&]() {
        if (indent > 0) out.push_back('\n');
    };

    if (is_null()) {
        out += "null";
    } else if (is_bool()) {
        out += as_bool() ? "true" : "false";
    } else if (is_number()) {
        const double d = as_number();
        const double rounded = std::round(d);
        if (std::abs(d - rounded) < 1.0e-12 && std::abs(d) < 1.0e15) {
            out += std::to_string(static_cast<std::int64_t>(rounded));
        } else {
            std::ostringstream oss;
            oss << std::setprecision(12) << d;
            out += oss.str();
        }
    } else if (is_string()) {
        escape_json_string(as_string(), out);
    } else if (is_array()) {
        const auto& arr = as_array();
        if (arr.empty()) {
            out += "[]";
            return;
        }
        out.push_back('[');
        nl();
        for (std::size_t i = 0; i < arr.size(); ++i) {
            pad(depth + 1);
            arr[i].dump_impl(out, indent, depth + 1);
            if (i + 1 < arr.size()) out.push_back(',');
            nl();
        }
        pad(depth);
        out.push_back(']');
    } else if (is_object()) {
        const auto& obj = as_object();
        if (obj.empty()) {
            out += "{}";
            return;
        }
        out.push_back('{');
        nl();
        std::size_t i = 0;
        for (const auto& [k, v] : obj) {
            pad(depth + 1);
            escape_json_string(k, out);
            out += (indent > 0) ? ": " : ":";
            v.dump_impl(out, indent, depth + 1);
            if (++i < obj.size()) out.push_back(',');
            nl();
        }
        pad(depth);
        out.push_back('}');
    }
}

std::string JsonValue::dump(int indent) const {
    std::string out;
    dump_impl(out, indent, 0);
    return out;
}

std::optional<JsonValue> JsonValue::parse(std::string_view json_text) {
    SimpleJsonParser parser(json_text);
    return parser.parse_root();
}

bool JsonValue::save_to_file(const std::filesystem::path& path, int indent) const {
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    const std::string text = dump(indent);
    ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
    return ofs.good();
}

std::optional<JsonValue> JsonValue::load_from_file(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return std::nullopt;
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return parse(oss.str());
}

} // namespace cypeclon::io

