#pragma once

#include "gui/docking_system.hpp"
#include "gui/theme_palette.hpp"
#include "model/obra.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace cypeclon::gui {

struct WelcomeButtonSpec {
    std::string id;
    std::string label;
    RectPx rect;
};

struct RecentProjectEntry {
    std::size_t index{1};
    std::string display_path;
    std::string description;
    RectPx rect;
};

/**
 * @brief Modelo de la Pantalla de Bienvenida (`01_startup_welcome_screen.png`) mostrada cuando
 *        no hay obra abierta (`!ebabase_hay_obra()`).
 */
class WelcomeScreenModel {
public:
    WelcomeScreenModel();

    static constexpr std::string_view kNoWorkStatusMessage =
        "Para utilizar el programa, debe tener una obra abierta. Puede crear una obra nueva o abrir una existente.";

    RgbColor background_color() const noexcept { return RgbColor{115, 115, 115}; } // #737373

    const RectPx& columna_archivo_rect() const noexcept { return col_archivo_rect_; }
    const RectPx& columna_ultimos_ficheros_rect() const noexcept { return col_ultimos_rect_; }
    const RectPx& columna_ayuda_rect() const noexcept { return col_ayuda_rect_; }

    const std::vector<WelcomeButtonSpec>& botones_archivo() const noexcept { return botones_archivo_; }
    const std::vector<RecentProjectEntry>& ultimos_ficheros() const noexcept { return ultimos_ficheros_; }
    const WelcomeButtonSpec& boton_mas_ficheros() const noexcept { return boton_mas_ficheros_; }
    const std::vector<std::string>& enlaces_ayuda() const noexcept { return enlaces_ayuda_; }
    const WelcomeButtonSpec& boton_gestion_licencias() const noexcept { return boton_gestion_licencias_; }

    void add_recent_file(std::string_view path, std::string_view description = "");

private:
    RectPx col_archivo_rect_{560, 402, 139, 283};
    RectPx col_ultimos_rect_{721, 402, 404, 283};
    RectPx col_ayuda_rect_{1147, 402, 213, 283};

    std::vector<WelcomeButtonSpec> botones_archivo_;
    std::vector<RecentProjectEntry> ultimos_ficheros_;
    WelcomeButtonSpec boton_mas_ficheros_{"ultimos_mas", "Más...", {902, 672, 43, 27}};
    std::vector<std::string> enlaces_ayuda_;
    WelcomeButtonSpec boton_gestion_licencias_{"ayuda_licencias", "Gestión de licencias", {1231, 716, 136, 28}};
};

/**
 * @brief Modo de creación en el asistente de Nueva Obra (`03_dialog_wizard_nueva_obra.png`, `e3dcomun`, `imporstr`, `bimcenter`, `ed3genport`).
 */
enum class ModoCreacionNuevaObra : std::uint8_t {
    ObraVacia = 0,                ///< "Obra vacía" (por defecto)
    IntroduccionAutomaticaIFC = 1,///< "Introducción automática IFC" (BIMserver.center / bimcenter.dll)
    ImportacionDxfDwgTxt = 2,     ///< Importación de ficheros DXF/DWG/TXT (imporstr.dll)
    GeneradorPorticos = 3         ///< Generador de Pórticos integrado (ed3genport)
};

struct WireframeSegment3d {
    core::Vec3d a;
    core::Vec3d b;
};

/**
 * @brief 1. Diálogo modal `Nueva obra` (`02_dialog_nueva_obra.png`, `CypeDialogo`, `696, 378, 537x285`).
 */
class DialogoNuevaObra {
public:
    DialogoNuevaObra();

    static constexpr std::string_view kWindowClass = "CypeDialogo";
    static constexpr RgbColor kTitleBarRed{168, 32, 26};

    const RectPx& dialog_rect() const noexcept { return dialog_rect_; }
    const RectPx& grupo_nombre_rect() const noexcept { return grupo_nombre_rect_; }
    const RectPx& grupo_descripcion_rect() const noexcept { return grupo_descripcion_rect_; }
    const RectPx& edit_clave_rect() const noexcept { return edit_clave_rect_; }
    const RectPx& edit_descripcion_rect() const noexcept { return edit_descripcion_rect_; }
    const RectPx& boton_examinar_rect() const noexcept { return boton_examinar_rect_; }
    const RectPx& boton_aceptar_rect() const noexcept { return boton_aceptar_rect_; }
    const RectPx& boton_cancelar_rect() const noexcept { return boton_cancelar_rect_; }

    const std::string& directorio() const noexcept { return directorio_; }
    void set_directorio(std::string dir);

    const std::string& nombre_fichero() const noexcept { return nombre_fichero_; }
    void set_nombre_fichero(std::string clave);

    const std::string& descripcion() const noexcept { return descripcion_; }
    void set_descripcion(std::string desc) { descripcion_ = std::move(desc); }

    std::filesystem::path full_ed3_path() const;
    bool requires_overwrite_confirmation() const;
    static constexpr std::string_view overwrite_prompt_message() noexcept {
        return "El fichero ya existe. ¿Desea sobreescribirlo?";
    }

    bool accept(model::Obra& obra) const;

private:
    RectPx dialog_rect_{696, 378, 537, 285};
    RectPx grupo_nombre_rect_{717, 425, 495, 105};
    RectPx boton_examinar_rect_{1125, 451, 72, 27};
    RectPx edit_clave_rect_{841, 492, 328, 23};
    RectPx grupo_descripcion_rect_{717, 538, 495, 64};
    RectPx edit_descripcion_rect_{732, 564, 465, 23};
    RectPx boton_aceptar_rect_{707, 625, 75, 27};
    RectPx boton_cancelar_rect_{1152, 625, 70, 27};

    std::string directorio_{"D:\\CYPE Ingenieros\\Proyectos\\CYPE 3D\\"};
    std::string nombre_fichero_{"Obra1"};
    std::string descripcion_{"Nave industrial metálica"};
};

/**
 * @brief 2. Asistente `Nueva obra` (`03_dialog_wizard_nueva_obra.png`, `CypeDialogo`, `624, 292, 682x457`).
 */
class AsistenteNuevaObra {
public:
    AsistenteNuevaObra();

    static constexpr std::string_view kWindowClass = "CypeDialogo";
    static constexpr RgbColor kTitleBarRed{168, 32, 26};

    const RectPx& dialog_rect() const noexcept { return dialog_rect_; }
    const RectPx& radio_obra_vacia_rect() const noexcept { return radio_obra_vacia_rect_; }
    const RectPx& radio_ifc_rect() const noexcept { return radio_ifc_rect_; }
    const RectPx& preview_canvas_rect() const noexcept { return preview_canvas_rect_; }
    const RectPx& boton_aceptar_rect() const noexcept { return boton_aceptar_rect_; }
    const RectPx& boton_cancelar_rect() const noexcept { return boton_cancelar_rect_; }

    ModoCreacionNuevaObra modo_seleccionado() const noexcept { return modo_; }
    void set_modo_seleccionado(ModoCreacionNuevaObra modo) noexcept { modo_ = modo; }

    const std::string& ruta_fichero_importacion() const noexcept { return ruta_importacion_; }
    void set_ruta_fichero_importacion(std::string path) { ruta_importacion_ = std::move(path); }

    const std::string& proyecto_bimserver_id() const noexcept { return proyecto_bimserver_id_; }
    void set_proyecto_bimserver_id(std::string id) { proyecto_bimserver_id_ = std::move(id); }

    // Previsualización vectorial 3D en CypeAreaDibujo (629, 384, 672x320)
    const std::vector<WireframeSegment3d>& preview_wireframe_3d() const noexcept { return preview_segments_; }

    // Aplica la inicialización geométrica según el modo seleccionado (Vacía / IFC / DXF / Generador de Pórticos)
    bool apply_creation_mode(model::Obra& obra) const;

private:
    void build_preview_wireframe();

    RectPx dialog_rect_{624, 292, 682, 457};
    RectPx radio_obra_vacia_rect_{640, 334, 80, 16};
    RectPx radio_ifc_rect_{640, 356, 178, 16};
    RectPx preview_canvas_rect_{629, 384, 672, 320};
    RectPx boton_aceptar_rect_{635, 711, 75, 27};
    RectPx boton_cancelar_rect_{1225, 711, 70, 27};

    ModoCreacionNuevaObra modo_{ModoCreacionNuevaObra::ObraVacia};
    std::string ruta_importacion_;
    std::string proyecto_bimserver_id_;
    std::vector<WireframeSegment3d> preview_segments_;
};

/**
 * @brief Subdiálogo `Estados límite` (`05_dialog_step5.png`, `358x438`).
 */
class DialogoEstadosLimite {
public:
    DialogoEstadosLimite() = default;

    const RectPx& dialog_rect() const noexcept { return dialog_rect_; }
    void load_from_obra(const model::Obra& obra);
    void apply_to_obra(model::Obra& obra) const;

    model::ConfiguracionEstadosLimite& config_mut() noexcept { return config_; }
    const model::ConfiguracionEstadosLimite& config() const noexcept { return config_; }

private:
    RectPx dialog_rect_{781, 297, 358, 438};
    model::ConfiguracionEstadosLimite config_{};
};

enum class PestanaPilotesTerreno : std::uint8_t {
    PilotesHormigonArmado = 0,
    Micropilotes = 1
};

/**
 * @brief 3. Diálogo `Obra vacía` / `Datos generales` (`04_dialog_step4.png` y `14_dialog_datos_generales.png`, `848x774`).
 */
class DialogoDatosGenerales {
public:
    DialogoDatosGenerales();

    static constexpr std::string_view kWindowClass = "CypeDialogo";
    static constexpr RgbColor kTitleBarRed{168, 32, 26};

    const RectPx& dialog_rect() const noexcept { return dialog_rect_; }
    const RectPx& boton_normas_rect() const noexcept { return boton_normas_rect_; }
    const RectPx& tab_pilotes_rect() const noexcept { return tab_pilotes_rect_; }

    bool modo_asistente_inicial() const noexcept { return modo_asistente_; }
    void set_modo_asistente_inicial(bool es_asistente) noexcept { modo_asistente_ = es_asistente; }

    PestanaPilotesTerreno pestana_terreno_activa() const noexcept { return pestana_terreno_; }
    void set_pestana_terreno_activa(PestanaPilotesTerreno tab) noexcept { pestana_terreno_ = tab; }

    void load_from_obra(const model::Obra& obra);
    void apply_to_obra(model::Obra& obra) const;

    model::DatosGeneralesObra& draft_mut() noexcept { return draft_; }
    const model::DatosGeneralesObra& draft() const noexcept { return draft_; }

    DialogoEstadosLimite& dialogo_estados_limite() noexcept { return dlg_estados_limite_; }
    const DialogoEstadosLimite& dialogo_estados_limite() const noexcept { return dlg_estados_limite_; }

    const std::vector<std::string>& botones_acciones() const noexcept { return botones_acciones_; }
    const std::vector<std::string>& botones_opciones() const noexcept { return botones_opciones_; }
    const std::vector<std::string>& catalogo_aceros_laminados() const noexcept { return cat_acero_laminado_; }
    const std::vector<std::string>& catalogo_aceros_conformados() const noexcept { return cat_acero_conformado_; }
    const std::vector<std::string>& catalogo_hormigones() const noexcept { return cat_hormigon_; }

private:
    RectPx dialog_rect_{536, 129, 848, 774};
    RectPx boton_normas_rect_{603, 176, 770, 27};
    RectPx tab_pilotes_rect_{900, 610, 479, 247};

    bool modo_asistente_{false};
    PestanaPilotesTerreno pestana_terreno_{PestanaPilotesTerreno::PilotesHormigonArmado};
    model::DatosGeneralesObra draft_{};
    DialogoEstadosLimite dlg_estados_limite_{};

    std::vector<std::string> botones_acciones_;
    std::vector<std::string> botones_opciones_;
    std::vector<std::string> cat_acero_laminado_;
    std::vector<std::string> cat_acero_conformado_;
    std::vector<std::string> cat_hormigon_;
};

} // namespace cypeclon::gui

