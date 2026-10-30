#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cypeclon::gui {

/**
 * @brief Modo de tema visual de CYPE 3D (componen.dll!pnpaleta_set_dark_mode / pnpaleta_get_dark_mode).
 */
enum class ThemeMode : std::uint8_t {
    Claro = 0,
    Oscuro = 1
};

/**
 * @brief Estilo de cinta de opciones Ribbon (CYPE3D_RIBBON_AZUL / Clásico).
 */
enum class RibbonStyle : std::uint8_t {
    RibbonAzul = 0,  ///< CYPE3D_RIBBON_AZUL (por defecto en v2027)
    Clasico = 1
};

struct RgbColor {
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};

    constexpr std::uint32_t to_hex_rgb() const noexcept {
        return (static_cast<std::uint32_t>(r) << 16) |
               (static_cast<std::uint32_t>(g) << 8) |
               static_cast<std::uint32_t>(b);
    }

    constexpr std::uint32_t to_win32_colorref() const noexcept {
        return static_cast<std::uint32_t>(r) |
               (static_cast<std::uint32_t>(g) << 8) |
               (static_cast<std::uint32_t>(b) << 16);
    }

    constexpr bool operator==(const RgbColor& other) const noexcept {
        return r == other.r && g == other.g && b == other.b;
    }
};

struct PaletteColors {
    // Barra de título personalizada de CypeDialogo: RGB(168, 32, 26)
    RgbColor dialogo_barra_titulo_roja{168, 32, 26};
    // Fondo de la pantalla inicial de bienvenida: #737373 (RGB(115, 115, 115))
    RgbColor bienvenida_fondo{115, 115, 115};
    RgbColor bienvenida_tarjeta_fondo{245, 246, 248};
    RgbColor bienvenida_tarjeta_cabecera{224, 228, 234};

    // Colores dependientes de tema (Claro / Oscuro) y estilo Ribbon
    RgbColor app_barra_titulo{28, 78, 128};
    RgbColor ribbon_banda_fondo{240, 242, 245};
    RgbColor ribbon_tab_activa_fondo{240, 242, 245};
    RgbColor ribbon_tab_inactiva_fondo{28, 78, 128};
    RgbColor ribbon_tab_activa_texto{20, 45, 76};
    RgbColor ribbon_tab_inactiva_texto{255, 255, 255};
    RgbColor ribbon_grupo_borde{205, 210, 218};
    RgbColor ribbon_grupo_titulo_texto{95, 102, 112};

    RgbColor panel_dock_fondo{236, 238, 242};
    RgbColor panel_dock_barra_titulo{214, 220, 228};
    RgbColor panel_texto_primario{24, 26, 30};
    RgbColor panel_borde_control{180, 186, 195};

    RgbColor lienzo_cad_fondo{225, 228, 232};
    RgbColor barra_estado_fondo{230, 232, 236};
    RgbColor solapa_inferior_activa{255, 255, 255};
    RgbColor solapa_inferior_inactiva{212, 216, 222};
};

struct IconResourceDescriptor {
    std::string resource_name;   ///< p.ej. "datobra2", "nudo_new2", "nuevocim"
    std::string source_file_tag; ///< p.ej. "ed3mnuob.c (ED3DLIB.DLL)" o "plcpanel.c (PLANOCIM.DLL)"
    std::string subfolder;
    std::uint32_t width_px{24};
    std::uint32_t height_px{24};
    std::uint32_t accent_rgb{0x1C4E80};
};

/**
 * @brief Gestor de paleta gráfica, temas Claro/Oscuro (`pnpaleta_*` en `componen.dll`)
 *        y caché dinámica de iconos (`imagen_nousar_carga_recurso`).
 */
class ThemePalette {
public:
    ThemePalette();

    // Equivalente a componen.dll!pnpaleta_set_dark_mode y pnpaleta_get_dark_mode
    void pnpaleta_set_dark_mode(bool enabled) noexcept;
    bool pnpaleta_get_dark_mode() const noexcept { return mode_ == ThemeMode::Oscuro; }

    ThemeMode theme_mode() const noexcept { return mode_; }
    RibbonStyle ribbon_style() const noexcept { return ribbon_style_; }
    void set_ribbon_style(RibbonStyle style) noexcept;

    // Equivalente a componen.dll!pnpaleta_set_color_fondo_barra_titulo_dialogo
    void pnpaleta_set_color_fondo_barra_titulo_dialogo(RgbColor color) noexcept;
    RgbColor color_barra_titulo_dialogo() const noexcept { return colors_.dialogo_barra_titulo_roja; }

    // Equivalente a componen.dll!pnpaleta_tipo_color_fondo_vistacpl_segun_tema
    RgbColor color_fondo_vista_segun_tema() const noexcept { return colors_.lienzo_cad_fondo; }

    const PaletteColors& colors() const noexcept { return colors_; }

    // Equivalente a componen.dll!imagen_nousar_carga_recurso
    const IconResourceDescriptor& imagen_carga_recurso(
        std::string_view subcarpeta_o_vacio,
        std::string_view source_file_tag,
        std::string_view icon_resource_name
    );

    std::size_t cached_icon_count() const noexcept { return icon_cache_.size(); }
    bool has_cached_icon(std::string_view icon_resource_name) const noexcept;

private:
    void rebuild_palette() noexcept;

    ThemeMode mode_{ThemeMode::Claro};
    RibbonStyle ribbon_style_{RibbonStyle::RibbonAzul};
    PaletteColors colors_{};
    std::unordered_map<std::string, IconResourceDescriptor> icon_cache_;
};

} // namespace cypeclon::gui

