#pragma once

#include "core/math.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cypeclon::io {

struct SchemaVersion {
    std::uint32_t major{2027};
    std::uint32_t minor{1};
    std::uint32_t patch{0};

    constexpr auto operator<=>(const SchemaVersion&) const noexcept = default;
    std::string to_string() const;
};

/**
 * @brief Escritor binario en memoria y disco equivalente a basicos.dll!cypesave_fichmem_para_escritura.
 *        Soporta cabeceras con versión de esquema, secciones con nombre y bloques con tamaño prefijado
 *        para permitir compatibilidad hacia adelante (cypesave_se_salta_bloque64).
 */
class BinaryWriter {
public:
    static constexpr char kFileMagic[8] = {'C', 'Y', 'P', 'E', '3', 'D', '2', '7'};

    explicit BinaryWriter(SchemaVersion version = {});

    void write_header();

    // Primitivas cypesave_graba_*
    void write_bool(bool v);
    void write_u8(std::uint8_t v);
    void write_u16(std::uint16_t v);
    void write_u32(std::uint32_t v);
    void write_u64(std::uint64_t v);
    void write_i32(std::int32_t v);
    void write_i64(std::int64_t v);
    void write_f32(float v);
    void write_f64(double v);

    void write_bytes(std::span<const std::uint8_t> bytes);
    void write_string_utf8(std::string_view s);
    void write_wstring(std::wstring_view ws);
    void write_vec2d(const core::Vec2d& v);
    void write_vec3d(const core::Vec3d& v);

    // Arrays (cypesave_graba_arr_*)
    void write_array_u32(std::span<const std::uint32_t> arr);
    void write_array_f64(std::span<const double> arr);
    void write_array_vec2d(std::span<const core::Vec2d> arr);
    void write_array_vec3d(std::span<const core::Vec3d> arr);
    void write_array_strings(std::span<const std::string> arr);

    // Bloques y Secciones con salto seguro (cypesave_graba_inicio_bloque64 / fin_bloque64 / seccion)
    std::size_t begin_block(std::uint32_t block_tag = 0, std::uint32_t block_version = 1);
    void end_block(std::size_t block_token);

    std::size_t begin_section(std::string_view section_name, std::uint32_t section_version = 1);
    void end_section(std::size_t section_token);

    const std::vector<std::uint8_t>& buffer() const noexcept { return buffer_; }
    std::vector<std::uint8_t> take_buffer() noexcept { return std::move(buffer_); }
    bool save_to_file(const std::filesystem::path& path) const;

private:
    SchemaVersion version_;
    std::vector<std::uint8_t> buffer_;
};

/**
 * @brief Lector binario con verificación de cabecera, versiones y salto seguro de bloques/secciones
 *        equivalente a basicos.dll!cypesave_fichmem_para_lectura.
 */
class BinaryReader {
public:
    struct BlockHeader {
        std::uint32_t tag{0};
        std::uint32_t version{1};
        std::uint64_t payload_size{0};
        std::size_t end_offset{0};
    };

    struct SectionHeader {
        std::string name;
        std::uint32_t version{1};
        std::uint64_t payload_size{0};
        std::size_t end_offset{0};
    };

    explicit BinaryReader(std::vector<std::uint8_t> data);
    static std::optional<BinaryReader> load_from_file(const std::filesystem::path& path);

    bool read_and_verify_header();
    const SchemaVersion& version() const noexcept { return version_; }

    bool eof() const noexcept { return pos_ >= data_.size(); }
    std::size_t position() const noexcept { return pos_; }
    void set_position(std::size_t new_pos);

    bool read_bool();
    std::uint8_t read_u8();
    std::uint16_t read_u16();
    std::uint32_t read_u32();
    std::uint64_t read_u64();
    std::int32_t read_i32();
    std::int64_t read_i64();
    float read_f32();
    double read_f64();

    std::vector<std::uint8_t> read_bytes(std::size_t count);
    std::string read_string_utf8();
    std::wstring read_wstring();
    core::Vec2d read_vec2d();
    core::Vec3d read_vec3d();

    std::vector<std::uint32_t> read_array_u32();
    std::vector<double> read_array_f64();
    std::vector<core::Vec2d> read_array_vec2d();
    std::vector<core::Vec3d> read_array_vec3d();
    std::vector<std::string> read_array_strings();

    BlockHeader begin_block();
    void end_block(const BlockHeader& blk);
    void skip_block(const BlockHeader& blk);

    SectionHeader begin_section();
    void end_section(const SectionHeader& sec);
    void skip_section(const SectionHeader& sec);

private:
    void require_bytes(std::size_t count) const;

    std::vector<std::uint8_t> data_;
    std::size_t pos_{0};
    SchemaVersion version_{};
};

// ==============================================================================
// Soporte JSON Nativo para Proyectos e Interoperabilidad BIM (JsonValue)
// ==============================================================================
class JsonValue {
public:
    using Array  = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    JsonValue() noexcept : storage_(nullptr) {}
    JsonValue(std::nullptr_t) noexcept : storage_(nullptr) {}
    JsonValue(bool b) noexcept : storage_(b) {}
    JsonValue(int i) noexcept : storage_(static_cast<double>(i)) {}
    JsonValue(std::int64_t i) noexcept : storage_(static_cast<double>(i)) {}
    JsonValue(std::uint32_t u) noexcept : storage_(static_cast<double>(u)) {}
    JsonValue(std::uint64_t u) noexcept : storage_(static_cast<double>(u)) {}
    JsonValue(double d) noexcept : storage_(d) {}
    JsonValue(const char* s) : storage_(std::string(s)) {}
    JsonValue(std::string s) noexcept : storage_(std::move(s)) {}
    JsonValue(Array arr) noexcept : storage_(std::move(arr)) {}
    JsonValue(Object obj) noexcept : storage_(std::move(obj)) {}

    static JsonValue make_object() { return JsonValue(Object{}); }
    static JsonValue make_array()  { return JsonValue(Array{}); }

    bool is_null() const noexcept   { return std::holds_alternative<std::nullptr_t>(storage_); }
    bool is_bool() const noexcept   { return std::holds_alternative<bool>(storage_); }
    bool is_number() const noexcept { return std::holds_alternative<double>(storage_); }
    bool is_string() const noexcept { return std::holds_alternative<std::string>(storage_); }
    bool is_array() const noexcept  { return std::holds_alternative<Array>(storage_); }
    bool is_object() const noexcept { return std::holds_alternative<Object>(storage_); }

    bool as_bool(bool default_val = false) const noexcept;
    double as_number(double default_val = 0.0) const noexcept;
    std::int64_t as_int64(std::int64_t default_val = 0) const noexcept;
    const std::string& as_string() const;
    const Array& as_array() const;
    const Object& as_object() const;
    Array& as_array_mut();
    Object& as_object_mut();

    JsonValue& operator[](const std::string& key);
    const JsonValue& get(const std::string& key) const;
    bool contains(const std::string& key) const noexcept;

    std::string dump(int indent = 2) const;
    static std::optional<JsonValue> parse(std::string_view json_text);

    bool save_to_file(const std::filesystem::path& path, int indent = 2) const;
    static std::optional<JsonValue> load_from_file(const std::filesystem::path& path);

private:
    void dump_impl(std::string& out, int indent, int depth) const;
    Storage storage_;
};

} // namespace cypeclon::io
