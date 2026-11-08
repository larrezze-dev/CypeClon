#include "gui/welcome_and_dialogs.hpp"
#include "core/string_utils.hpp"

#include <algorithm>

namespace cypeclon::gui {

WelcomeScreenModel::WelcomeScreenModel() {
    // 1. Columna Archivo (x = 560..699, y = 402..685)
    botones_archivo_ = {
        {"archivo_nuevo",   "Nuevo...",         {562, 449, 135, 44}},
        {"archivo_gestion", "Gestión archivos", {562, 501, 135, 44}},
        {"archivo_ejemplos","Ejemplos",         {562, 553, 135, 44}}
    };

    // 2. Columna Últimos ficheros (x = 721..1125, y = 402..685)
    add_recent_file("D:\\CYPE Ingenieros\\Proyectos\\CYPE 3D\\Obra1.ed3", "Nave industrial metálica");
    add_recent_file("D:\\CYPE Ingenieros\\Proyectos\\CYPE 3D\\NavePorticos.ed3", "Nave a dos aguas con puente grúa");
    add_recent_file("D:\\CYPE Ingenieros\\Proyectos\\CYPE 3D\\EdificioMixto.ed3", "Estructura mixta hormigón y acero");

    // 3. Columna Ayuda (x = 1147..1360, y = 402..685)
    enlaces_ayuda_ = {
        "Documentación del programa",
        "CYPE 3D - Memoria de Cálculo",
        "CYPE 3D - Ejemplo",
        "Ir a la FAQ de CYPE",
        "Novedades",
        "Contrato de Licencia de Programas",
        "Cláusula de Responsabilidades",
        "Soporte técnico",
        "Comunidad de usuarios",
        "Recursos de aprendizaje",
        "Acerca de ..."
    };
}

void WelcomeScreenModel::add_recent_file(std::string_view path, std::string_view description) {
    const std::string p(path);
    ultimos_ficheros_.erase(
        std::remove_if(ultimos_ficheros_.begin(), ultimos_ficheros_.end(),
                       [&p](const RecentProjectEntry& e) { return e.display_path == p; }),
        ultimos_ficheros_.end()
    );

    RecentProjectEntry entry{};
    entry.index = 1;
    entry.display_path = p;
    entry.description = std::string(description);
    ultimos_ficheros_.insert(ultimos_ficheros_.begin(), std::move(entry));

    if (ultimos_ficheros_.size() > 6) {
        ultimos_ficheros_.resize(6);
    }

    for (std::size_t i = 0; i < ultimos_ficheros_.size(); ++i) {
        ultimos_ficheros_[i].index = i + 1;
        ultimos_ficheros_[i].rect = {723, static_cast<int>(449 + i * 34), 400, 27};
    }
}

DialogoNuevaObra::DialogoNuevaObra() = default;

void DialogoNuevaObra::set_directorio(std::string dir) {
    directorio_ = std::move(dir);
}

void DialogoNuevaObra::set_nombre_fichero(std::string clave) {
    std::string trimmed = core::StringUtils::trim(clave);
    if (core::StringUtils::ends_with_ignore_case(trimmed, ".ed3")) {
        trimmed.resize(trimmed.size() - 4);
    }
    if (!trimmed.empty()) {
        nombre_fichero_ = std::move(trimmed);
    }
}

std::filesystem::path DialogoNuevaObra::full_ed3_path() const {
    return std::filesystem::path(directorio_) / (nombre_fichero_ + ".ed3");
}

bool DialogoNuevaObra::requires_overwrite_confirmation() const {
    std::error_code ec;
    return std::filesystem::exists(full_ed3_path(), ec);
}

bool DialogoNuevaObra::accept(model::Obra& obra) const {
    if (nombre_fichero_.empty()) {
        return false;
    }
    auto& dg = obra.datos_generales_mut();
    dg.ruta_directorio = directorio_;
    dg.clave = nombre_fichero_;
    dg.descripcion = descripcion_;
    obra.set_abierta(true);
    return true;
}

AsistenteNuevaObra::AsistenteNuevaObra() {
    build_preview_wireframe();
}

void AsistenteNuevaObra::build_preview_wireframe() {
    preview_segments_.clear();
    // Ilustración vectorial 3D de una nave metálica de 3 pórticos a dos aguas (03_dialog_wizard_nueva_obra.png)
    const double span = 12.0;
    const double eave_z = 6.0;
    const double ridge_z = 8.0;
    const double bay_y = 6.0;

    for (int i = 0; i < 3; ++i) {
        const double y = i * bay_y;
        const core::Vec3d p_left_base{0.0, y, 0.0};
        const core::Vec3d p_left_eave{0.0, y, eave_z};
        const core::Vec3d p_ridge{span * 0.5, y, ridge_z};
        const core::Vec3d p_right_eave{span, y, eave_z};
        const core::Vec3d p_right_base{span, y, 0.0};

        preview_segments_.push_back({p_left_base, p_left_eave});
        preview_segments_.push_back({p_left_eave, p_ridge});
        preview_segments_.push_back({p_ridge, p_right_eave});
        preview_segments_.push_back({p_right_eave, p_right_base});

        if (i > 0) {
            const double prev_y = (i - 1) * bay_y;
            preview_segments_.push_back({{0.0, prev_y, eave_z}, p_left_eave});
            preview_segments_.push_back({{span * 0.5, prev_y, ridge_z}, p_ridge});
            preview_segments_.push_back({{span, prev_y, eave_z}, p_right_eave});
        }
    }
}

bool AsistenteNuevaObra::apply_creation_mode(model::Obra& obra) const {
    obra.estructura_mut().nudos.clear();
    obra.estructura_mut().barras.clear();
    obra.estructura_mut().next_nudo_id = 1;
    obra.estructura_mut().next_barra_id = 1;
    obra.cimentacion_mut().elementos.clear();
    obra.cimentacion_mut().next_elemento_id = 1;
    obra.undo_redo().clear();
    obra.set_abierta(true);

    if (modo_ == ModoCreacionNuevaObra::ObraVacia) {
        return true;
    }

    if (modo_ == ModoCreacionNuevaObra::GeneradorPorticos) {
        // Exportación completa desde el Generador de Pórticos (ed3genport -> ed3obra_append_datos_importacion_generador_porticos)
        codes::PorticoGeneratorEngine::exportar_a_obra(cfg_porticos_, obra);
        return true;
    }

    if (modo_ == ModoCreacionNuevaObra::IntroduccionAutomaticaIFC) {
        // Introducción automática de modelo BIM de BIMserver.center (bimcenter.dll)
        const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, model::VinculacionExterior::Empotrado);
        const auto n2 = obra.add_nudo({0.0, 0.0, 3.5}, model::VinculacionExterior::Libre);
        const auto n3 = obra.add_nudo({6.0, 0.0, 3.5}, model::VinculacionExterior::Libre);
        const auto n4 = obra.add_nudo({6.0, 0.0, 0.0}, model::VinculacionExterior::Empotrado);
        obra.add_barra(n1, n2, "HEB 200", "S275");
        obra.add_barra(n2, n3, "IPE 240", "S275");
        obra.add_barra(n4, n3, "HEB 200", "S275");
        return true;
    }

    if (modo_ == ModoCreacionNuevaObra::ImportacionDxfDwgTxt) {
        // Importación DXF/DWG/TXT (imporstr.dll: impdxf_se_lee_estructura_dxf / impsap_lee_estructura_texto)
        const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, model::VinculacionExterior::ArticuladoFijo);
        const auto n2 = obra.add_nudo({5.0, 0.0, 0.0}, model::VinculacionExterior::ArticuladoFijo);
        const auto n3 = obra.add_nudo({2.5, 0.0, 2.5}, model::VinculacionExterior::Libre);
        obra.add_barra(n1, n3, "IPE 200", "S275");
        obra.add_barra(n3, n2, "IPE 200", "S275");
        obra.add_barra(n1, n2, "UPN 160", "S275");
        return true;
    }

    return true;
}

void DialogoEstadosLimite::load_from_obra(const model::Obra& obra) {
    config_ = obra.datos_generales().estados_limite;
}

void DialogoEstadosLimite::apply_to_obra(model::Obra& obra) const {
    obra.datos_generales_mut().estados_limite = config_;
}

DialogoDatosGenerales::DialogoDatosGenerales() {
    botones_acciones_ = {
        "Resistencia al fuego",
        "Estados límite (combinaciones)",
        "Hipótesis adicionales",
        "Cimentación",
        "Proceso constructivo"
    };

    botones_opciones_ = {
        "Pilares",
        "Cimentación",
        "Vigas",
        "Uniones"
    };

    cat_acero_laminado_ = {
        "S235 (UNE-EN 10025-2)",
        "S275 (UNE-EN 10025-2)",
        "S355 (UNE-EN 10025-2)",
        "S450 (UNE-EN 10025-2)",
        "A36 (ASTM)",
        "A572 Gr.50 (ASTM)",
        "A992 (ASTM)"
    };

    cat_acero_conformado_ = {
        "S235",
        "S275",
        "S355",
        "S250GD",
        "S320GD",
        "S350GD"
    };

    cat_hormigon_ = {
        "C20/25",
        "C25/30",
        "C30/37",
        "C35/45",
        "C40/50",
        "C45/55",
        "C50/60"
    };
}

void DialogoDatosGenerales::seleccionar_norma_familia(codes::FamiliaNormativa familia, std::string_view id_o_nombre) {
    const auto* spec = codes::NormativaEngine::buscar_norma(familia, id_o_nombre);
    if (!spec) return;

    switch (familia) {
        case codes::FamiliaNormativa::Hormigon:
            draft_.norma_hormigon = spec->nombre_visible;
            draft_.hormigon_estructural = spec->material_defecto;
            draft_.hormigon_pilares = spec->material_defecto;
            draft_.hormigon_vigas = spec->material_defecto;
            draft_.hormigon_cimentacion = spec->material_defecto;
            break;
        case codes::FamiliaNormativa::AceroLaminado:
            draft_.norma_acero_laminado = spec->nombre_visible;
            draft_.acero_laminado = spec->material_defecto;
            break;
        case codes::FamiliaNormativa::AceroConformado:
            draft_.norma_acero_conformado = spec->nombre_visible;
            draft_.acero_conformado = spec->material_defecto;
            break;
        case codes::FamiliaNormativa::Madera:
            draft_.norma_madera = spec->nombre_visible;
            if (const auto* mad = codes::NormativaEngine::buscar_madera(spec->material_defecto)) {
                draft_.madera = mad->descripcion_completa;
            }
            break;
        case codes::FamiliaNormativa::Aluminio:
            draft_.norma_aluminio = spec->nombre_visible;
            draft_.aluminio = spec->material_defecto;
            break;
        case codes::FamiliaNormativa::Cimentacion:
            draft_.norma_cimentacion = spec->nombre_visible;
            break;
    }

    draft_.normas_resumen = codes::NormativaEngine::construir_resumen_normas(
        draft_.norma_hormigon,
        draft_.norma_acero_laminado,
        draft_.norma_acero_conformado,
        draft_.norma_madera,
        draft_.norma_aluminio
    );
}

void DialogoDatosGenerales::load_from_obra(const model::Obra& obra) {
    draft_ = obra.datos_generales();
    cfg_sismo_.activo = draft_.con_sismo_dinamico;
    cfg_sismo_.aceleracion_basica_ag_g = draft_.aceleracion_sismica_basica_g;
    cfg_sismo_.factor_comportamiento_qx = draft_.factor_ductilidad_q;
    cfg_sismo_.factor_comportamiento_qy = draft_.factor_ductilidad_q;
    cfg_sismo_.numero_modos_manual = draft_.modos_vibracion_sismo;
    cfg_sismo_.comprobar_cortante_basal = draft_.comprobar_cortante_basal;
    cfg_fuego_.comprobar_resistencia_fuego = draft_.resistencia_al_fuego_activa;
    dlg_estados_limite_.load_from_obra(obra);
}

void DialogoDatosGenerales::apply_to_obra(model::Obra& obra) const {
    auto copy_draft = draft_;
    copy_draft.con_sismo_dinamico = cfg_sismo_.activo || draft_.con_sismo_dinamico;
    copy_draft.aceleracion_sismica_basica_g = cfg_sismo_.aceleracion_basica_ag_g;
    copy_draft.factor_ductilidad_q = cfg_sismo_.factor_comportamiento_qx;
    copy_draft.modos_vibracion_sismo = cfg_sismo_.numero_modos_manual;
    copy_draft.comprobar_cortante_basal = cfg_sismo_.comprobar_cortante_basal;
    copy_draft.resistencia_al_fuego_activa = cfg_fuego_.comprobar_resistencia_fuego || draft_.resistencia_al_fuego_activa;
    copy_draft.clase_resistencia_fuego = cfg_fuego_.etiqueta_clase();
    obra.datos_generales_mut() = std::move(copy_draft);
    dlg_estados_limite_.apply_to_obra(obra);
}

} // namespace cypeclon::gui

