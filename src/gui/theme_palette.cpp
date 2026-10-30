#include "gui/theme_palette.hpp"

namespace cypeclon::gui {

ThemePalette::ThemePalette() {
    rebuild_palette();
}

void ThemePalette::pnpaleta_set_dark_mode(bool enabled) noexcept {
    mode_ = enabled ? ThemeMode::Oscuro : ThemeMode::Claro;
    rebuild_palette();
}

void ThemePalette::set_ribbon_style(RibbonStyle style) noexcept {
    ribbon_style_ = style;
    rebuild_palette();
}

void ThemePalette::pnpaleta_set_color_fondo_barra_titulo_dialogo(RgbColor color) noexcept {
    colors_.dialogo_barra_titulo_roja = color;
}

const IconResourceDescriptor& ThemePalette::imagen_carga_recurso(
    std::string_view subcarpeta_o_vacio,
    std::string_view source_file_tag,
    std::string_view icon_resource_name
) {
    const std::string key(icon_resource_name);
    auto it = icon_cache_.find(key);
    if (it != icon_cache_.end()) {
        return it->second;
    }

    // Cálculo determinista de color de acento según recurso de icono para renderizado vectorial
    std::uint32_t hash = 2166136261u;
    for (unsigned char c : key) {
        hash ^= c;
        hash *= 16777619u;
    }
    const std::uint8_t r = static_cast<std::uint8_t>(40 + (hash & 0x7F));
    const std::uint8_t g = static_cast<std::uint8_t>(60 + ((hash >> 8) & 0x7F));
    const std::uint8_t b = static_cast<std::uint8_t>(90 + ((hash >> 16) & 0x7F));

    IconResourceDescriptor desc{};
    desc.resource_name = key;
    desc.source_file_tag = std::string(source_file_tag);
    desc.subfolder = std::string(subcarpeta_o_vacio);
    desc.width_px = 24;
    desc.height_px = 24;
    desc.accent_rgb = (static_cast<std::uint32_t>(r) << 16) |
                      (static_cast<std::uint32_t>(g) << 8) |
                      static_cast<std::uint32_t>(b);

    auto [inserted_it, _] = icon_cache_.emplace(key, std::move(desc));
    return inserted_it->second;
}

bool ThemePalette::has_cached_icon(std::string_view icon_resource_name) const noexcept {
    return icon_cache_.find(std::string(icon_resource_name)) != icon_cache_.end();
}

void ThemePalette::rebuild_palette() noexcept {
    // Barra roja de CypeDialogo constante según capturas 02..05 y 14: RGB(168, 32, 26)
    colors_.dialogo_barra_titulo_roja = RgbColor{168, 32, 26};
    // Fondo de bienvenida constante según 01_startup_welcome_screen.png: #737373
    colors_.bienvenida_fondo = RgbColor{115, 115, 115};

    if (mode_ == ThemeMode::Claro) {
        colors_.bienvenida_tarjeta_fondo = RgbColor{245, 246, 248};
        colors_.bienvenida_tarjeta_cabecera = RgbColor{224, 228, 234};

        if (ribbon_style_ == RibbonStyle::RibbonAzul) {
            colors_.app_barra_titulo = RgbColor{28, 78, 128};
            colors_.ribbon_tab_inactiva_fondo = RgbColor{28, 78, 128};
            colors_.ribbon_tab_inactiva_texto = RgbColor{255, 255, 255};
        } else {
            colors_.app_barra_titulo = RgbColor{216, 220, 226};
            colors_.ribbon_tab_inactiva_fondo = RgbColor{216, 220, 226};
            colors_.ribbon_tab_inactiva_texto = RgbColor{30, 34, 40};
        }

        colors_.ribbon_banda_fondo = RgbColor{240, 242, 245};
        colors_.ribbon_tab_activa_fondo = RgbColor{240, 242, 245};
        colors_.ribbon_tab_activa_texto = RgbColor{20, 45, 76};
        colors_.ribbon_grupo_borde = RgbColor{205, 210, 218};
        colors_.ribbon_grupo_titulo_texto = RgbColor{95, 102, 112};

        colors_.panel_dock_fondo = RgbColor{236, 238, 242};
        colors_.panel_dock_barra_titulo = RgbColor{214, 220, 228};
        colors_.panel_texto_primario = RgbColor{24, 26, 30};
        colors_.panel_borde_control = RgbColor{180, 186, 195};

        colors_.lienzo_cad_fondo = RgbColor{225, 228, 232};
        colors_.barra_estado_fondo = RgbColor{230, 232, 236};
        colors_.solapa_inferior_activa = RgbColor{255, 255, 255};
        colors_.solapa_inferior_inactiva = RgbColor{212, 216, 222};
    } else {
        // Modo Oscuro (pnpaleta_set_dark_mode(true))
        colors_.bienvenida_tarjeta_fondo = RgbColor{45, 48, 54};
        colors_.bienvenida_tarjeta_cabecera = RgbColor{58, 62, 70};

        if (ribbon_style_ == RibbonStyle::RibbonAzul) {
            colors_.app_barra_titulo = RgbColor{18, 48, 82};
            colors_.ribbon_tab_inactiva_fondo = RgbColor{18, 48, 82};
            colors_.ribbon_tab_inactiva_texto = RgbColor{230, 235, 242};
        } else {
            colors_.app_barra_titulo = RgbColor{36, 39, 44};
            colors_.ribbon_tab_inactiva_fondo = RgbColor{36, 39, 44};
            colors_.ribbon_tab_inactiva_texto = RgbColor{210, 214, 220};
        }

        colors_.ribbon_banda_fondo = RgbColor{42, 45, 50};
        colors_.ribbon_tab_activa_fondo = RgbColor{42, 45, 50};
        colors_.ribbon_tab_activa_texto = RgbColor{110, 175, 245};
        colors_.ribbon_grupo_borde = RgbColor{68, 73, 82};
        colors_.ribbon_grupo_titulo_texto = RgbColor{160, 168, 178};

        colors_.panel_dock_fondo = RgbColor{38, 41, 46};
        colors_.panel_dock_barra_titulo = RgbColor{52, 56, 64};
        colors_.panel_texto_primario = RgbColor{232, 235, 240};
        colors_.panel_borde_control = RgbColor{78, 84, 94};

        colors_.lienzo_cad_fondo = RgbColor{24, 27, 32};
        colors_.barra_estado_fondo = RgbColor{32, 35, 40};
        colors_.solapa_inferior_activa = RgbColor{52, 56, 64};
        colors_.solapa_inferior_inactiva = RgbColor{34, 37, 42};
    }
}

} // namespace cypeclon::gui

