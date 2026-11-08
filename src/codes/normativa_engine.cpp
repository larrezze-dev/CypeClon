#include "codes/normativa_engine.hpp"
#include "model/obra.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>

namespace cypeclon::codes {

// ==============================================================================
// 1. P-05.1 — Normativas y Propiedades de Materiales
// ==============================================================================

double PropiedadesAceroArmaduras::longitud_anclaje_basica_mm(
    double diametro_mm,
    double fctd_mpa,
    bool posicion_ii
) const noexcept {
    // Según Código Estructural / Eurocódigo 2 / EHE-08 (`dxancla_long_anclaje_pos_I` / `pos_II`):
    // f_bd = 2.25 * eta_1 * eta_2 * f_ctd, con eta_1 = 1.0 (Pos I) o 0.7 (Pos II)
    const double eta_1 = posicion_ii ? 0.70 : 1.00;
    const double eta_2 = (diametro_mm <= 32.0) ? 1.00 : ((132.0 - diametro_mm) / 100.0);
    const double fbd = std::max(0.50, 2.25 * eta_1 * eta_2 * fctd_mpa);
    const double lb_rqd = (diametro_mm / 4.0) * (fyd_mpa() / fbd);
    const double lb_min = std::max({0.30 * lb_rqd, 10.0 * diametro_mm, 100.0});
    return std::max(lb_rqd, lb_min);
}

const std::vector<NormaEstructuralSpec>& NormativaEngine::catalogo_normas(FamiliaNormativa familia) {
    static const std::vector<NormaEstructuralSpec> kHormigon = {
        {"CE_2021_H", "Código Estructural", "España", FamiliaNormativa::Hormigon, 1.50, 1.15, 1.25, true, "HA-25"},
        {"EC2_GENERICO", "Eurocódigo 2", "Unión Europea", FamiliaNormativa::Hormigon, 1.50, 1.15, 1.25, true, "C25/30"},
        {"EC2_ESPANA", "Eurocódigo 2 (España)", "España", FamiliaNormativa::Hormigon, 1.50, 1.15, 1.25, true, "C25/30"},
        {"EHE_08", "EHE-08", "España", FamiliaNormativa::Hormigon, 1.50, 1.15, 1.25, true, "HA-25"},
        {"ACI_318_19", "ACI 318-19", "USA / Internacional", FamiliaNormativa::Hormigon, 1.33, 1.11, 1.25, false, "C25/30"},
        {"CIRSOC_201", "CIRSOC 201-2005", "Argentina", FamiliaNormativa::Hormigon, 1.33, 1.11, 1.25, false, "C25/30"},
        {"NBR_6118", "ABNT NBR 6118:2014", "Brasil", FamiliaNormativa::Hormigon, 1.40, 1.15, 1.25, true, "C25/30"},
        {"NTC_MEX_2023", "NTC-2023 (México)", "México", FamiliaNormativa::Hormigon, 1.33, 1.15, 1.25, false, "C25/30"}
    };
    static const std::vector<NormaEstructuralSpec> kAceroLaminado = {
        {"CTE_DB_SE_A", "CTE DB SE-A", "España", FamiliaNormativa::AceroLaminado, 1.05, 1.05, 1.25, true, "S275 (UNE-EN 10025-2)"},
        {"CE_2021_A", "Código Estructural", "España", FamiliaNormativa::AceroLaminado, 1.05, 1.05, 1.25, true, "S275 (UNE-EN 10025-2)"},
        {"EC3_GENERICO", "Eurocódigo 3", "Unión Europea", FamiliaNormativa::AceroLaminado, 1.00, 1.00, 1.25, true, "S275 (UNE-EN 10025-2)"},
        {"EC3_ESPANA", "Eurocódigo 3 (España)", "España", FamiliaNormativa::AceroLaminado, 1.05, 1.05, 1.25, true, "S275 (UNE-EN 10025-2)"},
        {"AISC_360_22", "ANSI/AISC 360-22 (LRFD)", "USA / Internacional", FamiliaNormativa::AceroLaminado, 1.11, 1.11, 1.33, true, "A36 (ASTM A36)"},
        {"CIRSOC_301", "CIRSOC 301-2018", "Argentina", FamiliaNormativa::AceroLaminado, 1.11, 1.11, 1.33, false, "S235"},
        {"NBR_8800", "ABNT NBR 8800:2008", "Brasil", FamiliaNormativa::AceroLaminado, 1.10, 1.10, 1.35, true, "A36 (ASTM A36)"}
    };
    static const std::vector<NormaEstructuralSpec> kAceroConformado = {
        {"CE_2021_CF", "Código Estructural", "España", FamiliaNormativa::AceroConformado, 1.05, 1.05, 1.25, true, "S235"},
        {"CTE_DB_SE_A_CF", "CTE DB SE-A", "España", FamiliaNormativa::AceroConformado, 1.05, 1.05, 1.25, true, "S235"},
        {"EC3_1_3", "Eurocódigo 3 (EN 1993-1-3)", "Unión Europea", FamiliaNormativa::AceroConformado, 1.00, 1.00, 1.25, true, "S235"},
        {"AISI_S100_16", "AISI S100-16 (LRFD)", "USA / Internacional", FamiliaNormativa::AceroConformado, 1.11, 1.18, 1.33, false, "S250GD"}
    };
    static const std::vector<NormaEstructuralSpec> kMadera = {
        {"CTE_DB_SE_M", "CTE DB SE-M", "España", FamiliaNormativa::Madera, 1.30, 1.25, 1.30, true, "C24"},
        {"EC5_GENERICO", "Eurocódigo 5", "Unión Europea", FamiliaNormativa::Madera, 1.30, 1.25, 1.30, true, "C24"},
        {"CIRSOC_601", "CIRSOC 601-2016", "Argentina", FamiliaNormativa::Madera, 1.30, 1.25, 1.30, false, "C24"}
    };
    static const std::vector<NormaEstructuralSpec> kAluminio = {
        {"EC9_GENERICO", "Eurocódigo 9", "Unión Europea", FamiliaNormativa::Aluminio, 1.10, 1.10, 1.25, false, "EN AW-5083 - F"},
        {"EC9_ESPANA", "UNE-EN 1999-1-1 (Eurocódigo 9)", "España", FamiliaNormativa::Aluminio, 1.10, 1.10, 1.25, false, "EN AW-6082 - T6"}
    };
    static const std::vector<NormaEstructuralSpec> kCimentacion = {
        {"CTE_DB_SE_C", "CTE DB SE-C", "España", FamiliaNormativa::Cimentacion, 1.50, 1.15, 1.25, false, "C25/30"},
        {"CE_2021_CIM", "Código Estructural", "España", FamiliaNormativa::Cimentacion, 1.50, 1.15, 1.25, false, "HA-25"},
        {"EC7_GENERICO", "Eurocódigo 7", "Unión Europea", FamiliaNormativa::Cimentacion, 1.50, 1.15, 1.25, false, "C25/30"}
    };

    switch (familia) {
        case FamiliaNormativa::Hormigon: return kHormigon;
        case FamiliaNormativa::AceroLaminado: return kAceroLaminado;
        case FamiliaNormativa::AceroConformado: return kAceroConformado;
        case FamiliaNormativa::Madera: return kMadera;
        case FamiliaNormativa::Aluminio: return kAluminio;
        case FamiliaNormativa::Cimentacion: return kCimentacion;
    }
    return kHormigon;
}

const NormaEstructuralSpec* NormativaEngine::buscar_norma(FamiliaNormativa familia, std::string_view id_o_nombre) noexcept {
    const auto& cat = catalogo_normas(familia);
    for (const auto& n : cat) {
        if (n.id == id_o_nombre || n.nombre_visible == id_o_nombre) {
            return &n;
        }
    }
    for (const auto& n : cat) {
        if (n.nombre_visible.find(id_o_nombre) != std::string::npos ||
            id_o_nombre.find(n.nombre_visible) != std::string_view::npos) {
            return &n;
        }
    }
    return cat.empty() ? nullptr : &cat.front();
}

std::string NormativaEngine::construir_resumen_normas(
    std::string_view norma_hormigon,
    std::string_view norma_acero_laminado,
    std::string_view norma_acero_conformado,
    std::string_view norma_madera,
    std::string_view norma_aluminio
) {
    std::ostringstream oss;
    oss << norma_hormigon << ", "
        << norma_acero_laminado << ", "
        << norma_acero_conformado << ", "
        << norma_madera << " y "
        << norma_aluminio;
    return oss.str();
}

const std::vector<PropiedadesHormigon>& NormativaEngine::catalogo_hormigones() {
    static const std::vector<PropiedadesHormigon> kCatalog = {
        {"C20/25", "Eurocódigo 2 / Código Estructural", 20.0, 28.0, 2.21, 29962.0, 12484.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"C25/30", "Eurocódigo 2 / Código Estructural", 25.0, 33.0, 2.56, 31476.0, 13115.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"C30/37", "Eurocódigo 2 / Código Estructural", 30.0, 38.0, 2.90, 32837.0, 13682.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"C35/45", "Eurocódigo 2 / Código Estructural", 35.0, 43.0, 3.21, 34077.0, 14199.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"C40/50", "Eurocódigo 2 / Código Estructural", 40.0, 48.0, 3.51, 35220.0, 14675.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"C45/55", "Eurocódigo 2 / Código Estructural", 45.0, 53.0, 3.80, 36283.0, 15118.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"C50/60", "Eurocódigo 2 / Código Estructural", 50.0, 58.0, 4.07, 37278.0, 15532.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"HA-25", "Código Estructural / EHE-08", 25.0, 33.0, 2.56, 27264.0, 11360.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"HA-30", "Código Estructural / EHE-08", 30.0, 38.0, 2.90, 28577.0, 11907.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"HA-35", "Código Estructural / EHE-08", 35.0, 43.0, 3.21, 29771.0, 12405.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"HA-40", "Código Estructural / EHE-08", 40.0, 48.0, 3.51, 30872.0, 12863.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"HA-45", "Código Estructural / EHE-08", 45.0, 53.0, 3.80, 31896.0, 13290.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035},
        {"HA-50", "Código Estructural / EHE-08", 50.0, 58.0, 4.07, 32856.0, 13690.0, 0.20, 25.0, 1.0e-5, 0.0020, 0.0035}
    };
    return kCatalog;
}

const PropiedadesHormigon* NormativaEngine::buscar_hormigon(std::string_view ref) noexcept {
    const auto& cat = catalogo_hormigones();
    for (const auto& h : cat) {
        if (h.referencia == ref) return &h;
    }
    for (const auto& h : cat) {
        if (ref.find(h.referencia) != std::string_view::npos) return &h;
    }
    return &cat[1]; // C25/30 por defecto
}

const std::vector<PropiedadesAceroEstructural>& NormativaEngine::catalogo_aceros_laminados() {
    static const std::vector<PropiedadesAceroEstructural> kCatalog = {
        {"S235 (UNE-EN 10025-2)", "S235", "UNE-EN 10025-2", false, 235.0, 225.0, 215.0, 360.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5},
        {"S275 (UNE-EN 10025-2)", "S275", "UNE-EN 10025-2", false, 275.0, 265.0, 255.0, 430.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5},
        {"S355 (UNE-EN 10025-2)", "S355", "UNE-EN 10025-2", false, 355.0, 345.0, 335.0, 510.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5},
        {"S450 (UNE-EN 10025-2)", "S450", "UNE-EN 10025-2", false, 440.0, 420.0, 400.0, 550.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5},
        {"A36 (ASTM A36)", "A36", "ASTM A36", false, 250.0, 250.0, 250.0, 400.0, 200000.0, 77200.0, 0.30, 78.5, 1.2e-5},
        {"A572 Gr.50 (ASTM A572)", "A572 Gr.50", "ASTM A572", false, 345.0, 345.0, 345.0, 450.0, 200000.0, 77200.0, 0.30, 78.5, 1.2e-5},
        {"A992 (ASTM A992)", "A992", "ASTM A992", false, 345.0, 345.0, 345.0, 450.0, 200000.0, 77200.0, 0.30, 78.5, 1.2e-5}
    };
    return kCatalog;
}

const std::vector<PropiedadesAceroEstructural>& NormativaEngine::catalogo_aceros_conformados() {
    static const std::vector<PropiedadesAceroEstructural> kCatalog = {
        {"S235", "S235", "UNE-EN 10025-2", true, 235.0, 225.0, 215.0, 360.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5},
        {"S275", "S275", "UNE-EN 10025-2", true, 275.0, 265.0, 255.0, 430.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5},
        {"S355", "S355", "UNE-EN 10025-2", true, 355.0, 345.0, 335.0, 510.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5},
        {"S220GD", "S220GD", "UNE-EN 10346", true, 220.0, 220.0, 220.0, 300.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5},
        {"S250GD", "S250GD", "UNE-EN 10346", true, 250.0, 250.0, 250.0, 330.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5},
        {"S280GD", "S280GD", "UNE-EN 10346", true, 280.0, 280.0, 280.0, 360.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5},
        {"S320GD", "S320GD", "UNE-EN 10346", true, 320.0, 320.0, 320.0, 390.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5},
        {"S350GD", "S350GD", "UNE-EN 10346", true, 350.0, 350.0, 350.0, 420.0, 210000.0, 80769.23, 0.30, 78.5, 1.2e-5}
    };
    return kCatalog;
}

const PropiedadesAceroEstructural* NormativaEngine::buscar_acero_estructural(std::string_view ref) noexcept {
    for (const auto& a : catalogo_aceros_laminados()) {
        if (a.referencia == ref || a.referencia_corta == ref) return &a;
    }
    for (const auto& a : catalogo_aceros_conformados()) {
        if (a.referencia == ref || a.referencia_corta == ref) return &a;
    }
    for (const auto& a : catalogo_aceros_laminados()) {
        if (ref.find(a.referencia_corta) != std::string_view::npos) return &a;
    }
    return &catalogo_aceros_laminados()[1]; // S275 por defecto
}

const std::vector<PropiedadesAceroArmaduras>& NormativaEngine::catalogo_aceros_armaduras() {
    static const std::vector<PropiedadesAceroArmaduras> kCatalog = {
        {"B 400 S", 400.0, 440.0, 200000.0, 78.5, false},
        {"B 400 SD", 400.0, 460.0, 200000.0, 78.5, true},
        {"B 500 S", 500.0, 550.0, 200000.0, 78.5, false},
        {"B 500 SD", 500.0, 575.0, 200000.0, 78.5, true},
        {"S-400", 400.0, 460.0, 200000.0, 78.5, false},
        {"S-500", 500.0, 550.0, 200000.0, 78.5, true},
        {"Grado 60 (ASTM A615)", 420.0, 620.0, 200000.0, 78.5, true}
    };
    return kCatalog;
}

const PropiedadesAceroArmaduras* NormativaEngine::buscar_acero_armaduras(std::string_view ref) noexcept {
    for (const auto& a : catalogo_aceros_armaduras()) {
        if (a.referencia == ref) return &a;
    }
    for (const auto& a : catalogo_aceros_armaduras()) {
        if (ref.find(a.referencia) != std::string_view::npos) return &a;
    }
    return &catalogo_aceros_armaduras()[3]; // B 500 SD
}

const std::vector<PropiedadesMadera>& NormativaEngine::catalogo_maderas() {
    static const std::vector<PropiedadesMadera> kCatalog = {
        {"C18", "Aserrada, procedente de coníferas o chopo (C18)", false, 18.0, 11.0, 18.0, 3.4, 9000.0, 6000.0, 560.0, 320.0, 3.8, 5.0e-6},
        {"C20", "Aserrada, procedente de coníferas o chopo (C20)", false, 20.0, 12.0, 19.0, 3.6, 9500.0, 6400.0, 590.0, 330.0, 3.9, 5.0e-6},
        {"C22", "Aserrada, procedente de coníferas o chopo (C22)", false, 22.0, 13.0, 20.0, 3.8, 10000.0, 6700.0, 630.0, 340.0, 4.1, 5.0e-6},
        {"C24", "Aserrada, procedente de coníferas o chopo (C24)", false, 24.0, 14.5, 21.0, 4.0, 11000.0, 7400.0, 690.0, 350.0, 4.2, 5.0e-6},
        {"C27", "Aserrada, procedente de coníferas o chopo (C27)", false, 27.0, 16.5, 22.0, 4.0, 11500.0, 7700.0, 720.0, 370.0, 4.4, 5.0e-6},
        {"C30", "Aserrada, procedente de coníferas o chopo (C30)", false, 30.0, 18.0, 23.0, 4.0, 12000.0, 8000.0, 750.0, 380.0, 4.6, 5.0e-6},
        {"GL24h", "Madera laminada encolada homogénea (GL24h)", true, 24.0, 19.2, 24.0, 3.5, 11500.0, 9600.0, 650.0, 385.0, 4.2, 5.0e-6},
        {"GL28h", "Madera laminada encolada homogénea (GL28h)", true, 28.0, 22.3, 28.0, 3.5, 12600.0, 10500.0, 650.0, 425.0, 4.6, 5.0e-6},
        {"GL32h", "Madera laminada encolada homogénea (GL32h)", true, 32.0, 25.6, 32.0, 3.5, 14200.0, 11800.0, 650.0, 440.0, 4.9, 5.0e-6}
    };
    return kCatalog;
}

const PropiedadesMadera* NormativaEngine::buscar_madera(std::string_view ref) noexcept {
    for (const auto& m : catalogo_maderas()) {
        if (m.referencia == ref || m.descripcion_completa == ref) return &m;
    }
    for (const auto& m : catalogo_maderas()) {
        if (ref.find(m.referencia) != std::string_view::npos) return &m;
    }
    return &catalogo_maderas()[3]; // C24
}

const std::vector<PropiedadesAluminio>& NormativaEngine::catalogo_aluminios() {
    static const std::vector<PropiedadesAluminio> kCatalog = {
        {"EN AW-5083 - O/H111", "EN AW-5083", "O/H111", 110.0, 270.0, 1.00, 1.00, 70000.0, 27000.0, 0.30, 2700.0, 26.5, 2.3e-5, "B"},
        {"EN AW-5083 - F", "EN AW-5083", "F", 110.0, 270.0, 1.00, 1.00, 70000.0, 27000.0, 0.30, 2700.0, 26.5, 2.3e-5, "B"},
        {"EN AW-6060 - T5", "EN AW-6060", "T5", 120.0, 160.0, 0.50, 0.63, 70000.0, 27000.0, 0.30, 2700.0, 26.5, 2.3e-5, "A"},
        {"EN AW-6060 - T6", "EN AW-6060", "T6", 140.0, 170.0, 0.46, 0.59, 70000.0, 27000.0, 0.30, 2700.0, 26.5, 2.3e-5, "A"},
        {"EN AW-6061 - T6", "EN AW-6061", "T6", 240.0, 260.0, 0.48, 0.58, 70000.0, 27000.0, 0.30, 2700.0, 26.5, 2.3e-5, "A"},
        {"EN AW-6063 - T6", "EN AW-6063", "T6", 160.0, 195.0, 0.41, 0.56, 70000.0, 27000.0, 0.30, 2700.0, 26.5, 2.3e-5, "A"},
        {"EN AW-6082 - T6", "EN AW-6082", "T6", 250.0, 290.0, 0.50, 0.64, 70000.0, 27000.0, 0.30, 2700.0, 26.5, 2.3e-5, "A"}
    };
    return kCatalog;
}

const PropiedadesAluminio* NormativaEngine::buscar_aluminio(std::string_view ref) noexcept {
    for (const auto& al : catalogo_aluminios()) {
        if (al.referencia == ref || al.aleacion == ref) return &al;
    }
    for (const auto& al : catalogo_aluminios()) {
        if (ref.find(al.aleacion) != std::string_view::npos) return &al;
    }
    return &catalogo_aluminios()[1]; // EN AW-5083 - F
}

const std::vector<PropiedadesPernoTornillo>& NormativaEngine::catalogo_pernos_tornillos() {
    static const std::vector<PropiedadesPernoTornillo> kCatalog = {
        {"4.6", "ISO 898-1", 240.0, 400.0, 210000.0, 78.5},
        {"5.6", "ISO 898-1", 300.0, 500.0, 210000.0, 78.5},
        {"6.8", "ISO 898-1", 480.0, 600.0, 210000.0, 78.5},
        {"8.8", "ISO 898-1", 640.0, 800.0, 210000.0, 78.5},
        {"10.9", "ISO 898-1", 900.0, 1000.0, 210000.0, 78.5},
        {"A325", "ASTM F3125", 635.0, 825.0, 200000.0, 78.5},
        {"A490", "ASTM F3125", 895.0, 1035.0, 200000.0, 78.5}
    };
    return kCatalog;
}

const PropiedadesPernoTornillo* NormativaEngine::buscar_perno_tornillo(std::string_view calidad) noexcept {
    for (const auto& p : catalogo_pernos_tornillos()) {
        if (p.calidad == calidad) return &p;
    }
    return &catalogo_pernos_tornillos()[3]; // 8.8
}

const std::vector<DiametroPernoMetrico>& NormativaEngine::catalogo_diametros_pernos() {
    static const std::vector<DiametroPernoMetrico> kDiam = {
        {"M12", 12.0, 113.10, 84.30},
        {"M16", 16.0, 201.06, 157.00},
        {"M20", 20.0, 314.16, 245.00},
        {"M22", 22.0, 380.13, 303.00},
        {"M24", 24.0, 452.39, 353.00},
        {"M27", 27.0, 572.56, 459.00},
        {"M30", 30.0, 706.86, 561.00},
        {"M36", 36.0, 1017.88, 817.00}
    };
    return kDiam;
}

// ==============================================================================
// 2. P-05.2 — Categorías de Uso, Hipótesis y Motor de Combinaciones ELU / ELS
// ==============================================================================

const std::vector<CategoriaUsoSpec>& NormativaEngine::catalogo_categorias_uso() {
    static const std::vector<CategoriaUsoSpec> kUsos = {
        {CodigoCategoriaUso::G1_CubiertaConservacion, "G1", "H",
         "Cubiertas accesibles únicamente para conservación (no concomitante con nieve ni viento)", 0.0, 0.0, 0.0},
        {CodigoCategoriaUso::A_Residencial, "A", "A",
         "Zonas residenciales (viviendas, habitaciones de hospital y hotel)", 0.7, 0.5, 0.3},
        {CodigoCategoriaUso::B_Administrativo, "B", "B",
         "Zonas administrativas y de oficinas", 0.7, 0.5, 0.3},
        {CodigoCategoriaUso::C_PublicaConcurrencia, "C", "C",
         "Zonas de pública concurrencia (salas de reunión, cines, teatros, gimnasios)", 0.7, 0.7, 0.6},
        {CodigoCategoriaUso::D_Comercial, "D", "D",
         "Zonas comerciales (tiendas, centros comerciales y grandes almacenes)", 0.7, 0.7, 0.6},
        {CodigoCategoriaUso::E_Almacenamiento, "E", "E",
         "Zonas de almacenamiento y uso industrial", 1.0, 0.9, 0.8},
        {CodigoCategoriaUso::F_TraficoLigero, "F", "F",
         "Zonas de tráfico y aparcamiento para vehículos ligeros (peso <= 30 kN)", 0.7, 0.7, 0.6},
        {CodigoCategoriaUso::G_TraficoPesado, "G", "G",
         "Zonas de tráfico y aparcamiento para vehículos medios (30 kN < peso <= 160 kN)", 0.7, 0.5, 0.3}
    };
    return kUsos;
}

const CategoriaUsoSpec& NormativaEngine::obtener_categoria_uso(CodigoCategoriaUso codigo) noexcept {
    const auto& cat = catalogo_categorias_uso();
    for (const auto& u : cat) {
        if (u.codigo == codigo) return u;
    }
    return cat.front();
}

std::vector<HipotesisCargaSpec> NormativaEngine::crear_hipotesis_por_defecto(
    bool con_viento,
    bool con_sismo,
    bool con_nieve
) {
    std::vector<HipotesisCargaSpec> list;
    std::uint32_t next_id = 1;

    list.push_back(HipotesisCargaSpec{
        next_id++, "PP", "Peso propio",
        NaturalezaHipotesis::PesoPropio, DisposicionHipotesis::Simultanea,
        CodigoCategoriaUso::G1_CubiertaConservacion, 0, true, true, 1.0, 1.0, 1.0
    });
    list.push_back(HipotesisCargaSpec{
        next_id++, "CM", "Cargas muertas",
        NaturalezaHipotesis::CargasMuertas, DisposicionHipotesis::Simultanea,
        CodigoCategoriaUso::G1_CubiertaConservacion, 0, true, true, 1.0, 1.0, 1.0
    });
    list.push_back(HipotesisCargaSpec{
        next_id++, "Q1", "Sobrecarga de uso",
        NaturalezaHipotesis::SobrecargaUso, DisposicionHipotesis::Compatible,
        CodigoCategoriaUso::A_Residencial, 0, true, true, 0.7, 0.5, 0.3
    });

    if (con_viento) {
        // Grupo de incompatibilidad 1: las direcciones de viento no actúan simultáneamente entre sí
        list.push_back(HipotesisCargaSpec{
            next_id++, "V1", "Viento +X (0°)",
            NaturalezaHipotesis::Viento, DisposicionHipotesis::Incompatible,
            CodigoCategoriaUso::G1_CubiertaConservacion, 1, true, true, 0.6, 0.2, 0.0
        });
        list.push_back(HipotesisCargaSpec{
            next_id++, "V2", "Viento -X (180°)",
            NaturalezaHipotesis::Viento, DisposicionHipotesis::Incompatible,
            CodigoCategoriaUso::G1_CubiertaConservacion, 1, true, true, 0.6, 0.2, 0.0
        });
        list.push_back(HipotesisCargaSpec{
            next_id++, "V3", "Viento +Y (90°)",
            NaturalezaHipotesis::Viento, DisposicionHipotesis::Incompatible,
            CodigoCategoriaUso::G1_CubiertaConservacion, 1, true, true, 0.6, 0.2, 0.0
        });
        list.push_back(HipotesisCargaSpec{
            next_id++, "V4", "Viento -Y (270°)",
            NaturalezaHipotesis::Viento, DisposicionHipotesis::Incompatible,
            CodigoCategoriaUso::G1_CubiertaConservacion, 1, true, true, 0.6, 0.2, 0.0
        });
    }

    if (con_nieve) {
        list.push_back(HipotesisCargaSpec{
            next_id++, "N1", "Nieve",
            NaturalezaHipotesis::Nieve, DisposicionHipotesis::Compatible,
            CodigoCategoriaUso::G1_CubiertaConservacion, 0, true, true, 0.5, 0.2, 0.0
        });
    }

    if (con_sismo) {
        list.push_back(HipotesisCargaSpec{
            next_id++, "SX", "Sismo X",
            NaturalezaHipotesis::Sismo, DisposicionHipotesis::Incompatible,
            CodigoCategoriaUso::G1_CubiertaConservacion, 2, true, true, 1.0, 1.0, 1.0
        });
        list.push_back(HipotesisCargaSpec{
            next_id++, "SY", "Sismo Y",
            NaturalezaHipotesis::Sismo, DisposicionHipotesis::Incompatible,
            CodigoCategoriaUso::G1_CubiertaConservacion, 2, true, true, 1.0, 1.0, 1.0
        });
    }

    return list;
}

double CombinacionGenerada::coeficiente_de(std::uint32_t hipotesis_id) const noexcept {
    for (const auto& t : terminos) {
        if (t.hipotesis_id == hipotesis_id) return t.coeficiente;
    }
    return 0.0;
}

namespace {

std::string formatear_formula_combinacion(const std::vector<TerminoCombinacion>& terminos) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    bool first = true;
    for (const auto& t : terminos) {
        if (std::abs(t.coeficiente) < 1e-6) continue;
        if (!first) {
            oss << (t.coeficiente >= 0.0 ? " + " : " - ");
        } else if (t.coeficiente < 0.0) {
            oss << "-";
        }
        oss << std::abs(t.coeficiente) << "·" << t.codigo_hipotesis;
        first = false;
    }
    return oss.str();
}

bool son_hipotesis_compatibles_entre_si(const HipotesisCargaSpec& a, const HipotesisCargaSpec& b) noexcept {
    if (a.id == b.id) return false;
    if (a.disposicion == DisposicionHipotesis::Incompatible &&
        b.disposicion == DisposicionHipotesis::Incompatible &&
        a.grupo_incompatibilidad != 0 &&
        a.grupo_incompatibilidad == b.grupo_incompatibilidad) {
        return false;
    }
    // Regla CTE DB SE-AE: La categoría G1 (conservación de cubiertas ligeras) no es concomitante con nieve ni con viento
    const bool a_es_g1 = (a.naturaleza == NaturalezaHipotesis::SobrecargaUso &&
                          a.categoria_uso == CodigoCategoriaUso::G1_CubiertaConservacion);
    const bool b_es_g1 = (b.naturaleza == NaturalezaHipotesis::SobrecargaUso &&
                          b.categoria_uso == CodigoCategoriaUso::G1_CubiertaConservacion);
    const bool a_es_clima = (a.naturaleza == NaturalezaHipotesis::Viento || a.naturaleza == NaturalezaHipotesis::Nieve);
    const bool b_es_clima = (b.naturaleza == NaturalezaHipotesis::Viento || b.naturaleza == NaturalezaHipotesis::Nieve);
    if ((a_es_g1 && b_es_clima) || (b_es_g1 && a_es_clima)) {
        return false;
    }
    return true;
}

} // namespace

std::vector<CombinacionGenerada> NormativaEngine::generar_combinaciones(
    const std::vector<HipotesisCargaSpec>& hipotesis,
    const ParametrosGeneracionCombinaciones& params
) {
    std::vector<CombinacionGenerada> resultado;
    std::uint32_t next_comb_id = 1;

    std::vector<HipotesisCargaSpec> permanentes;
    std::vector<HipotesisCargaSpec> variables;
    std::vector<HipotesisCargaSpec> sismicas;
    std::vector<HipotesisCargaSpec> accidentales;

    for (auto h : hipotesis) {
        if (!h.actua) continue;
        if (h.naturaleza == NaturalezaHipotesis::SobrecargaUso) {
            const auto& cat = obtener_categoria_uso(h.categoria_uso);
            h.psi_0 = cat.psi_0;
            h.psi_1 = cat.psi_1;
            h.psi_2 = cat.psi_2;
        } else if (h.naturaleza == NaturalezaHipotesis::Nieve) {
            h.psi_0 = params.altitud_nieve_mayor_1000m ? 0.70 : 0.50;
            h.psi_1 = params.altitud_nieve_mayor_1000m ? 0.50 : 0.20;
            h.psi_2 = params.altitud_nieve_mayor_1000m ? 0.20 : 0.00;
        } else if (h.naturaleza == NaturalezaHipotesis::Viento) {
            h.psi_0 = 0.60;
            h.psi_1 = 0.20;
            h.psi_2 = 0.00;
        }

        if (h.naturaleza == NaturalezaHipotesis::PesoPropio ||
            h.naturaleza == NaturalezaHipotesis::CargasMuertas ||
            h.naturaleza == NaturalezaHipotesis::EmpujesTerreno) {
            permanentes.push_back(h);
        } else if (h.naturaleza == NaturalezaHipotesis::Sismo) {
            sismicas.push_back(h);
        } else if (h.naturaleza == NaturalezaHipotesis::Accidental ||
                   h.naturaleza == NaturalezaHipotesis::Fuego) {
            accidentales.push_back(h);
        } else {
            variables.push_back(h);
        }
    }

    auto agregar_combinacion = [&](SituacionProyecto sit, std::vector<TerminoCombinacion> terms) {
        if (terms.empty()) return;
        CombinacionGenerada c;
        c.id = next_comb_id++;
        c.situacion = sit;
        c.nombre_formula = formatear_formula_combinacion(terms);
        c.terminos = std::move(terms);
        resultado.push_back(std::move(c));
    };

    // 1. E.L.U. Persistentes o Transitorias:
    //    a) Solo permanentes (favorable y desfavorable)
    for (double gamma_g : {params.gamma_g_favorable, params.gamma_g_desfavorable}) {
        std::vector<TerminoCombinacion> terms;
        for (const auto& g : permanentes) {
            terms.push_back({g.id, g.codigo, gamma_g});
        }
        agregar_combinacion(SituacionProyecto::ELU_PersistenteTransitoria, std::move(terms));
    }

    //    b) Con cada acción variable Q_1 como acción dominante (sola y acompañada por las demás compatibles)
    for (double gamma_g : {params.gamma_g_favorable, params.gamma_g_desfavorable}) {
        for (std::size_t i = 0; i < variables.size(); ++i) {
            const auto& q_dom = variables[i];

            // Caso 1: Q_dom actuando sola con las permanentes
            {
                std::vector<TerminoCombinacion> terms;
                for (const auto& g : permanentes) {
                    terms.push_back({g.id, g.codigo, gamma_g});
                }
                terms.push_back({q_dom.id, q_dom.codigo, params.gamma_q_desfavorable});
                agregar_combinacion(SituacionProyecto::ELU_PersistenteTransitoria, std::move(terms));
            }

            // Caso 2: Q_dom acompañada por todas las demás variables compatibles con factor gamma_Q * psi_0
            std::vector<TerminoCombinacion> terms_acomp;
            for (const auto& g : permanentes) {
                terms_acomp.push_back({g.id, g.codigo, gamma_g});
            }
            terms_acomp.push_back({q_dom.id, q_dom.codigo, params.gamma_q_desfavorable});

            bool any_acomp = false;
            std::vector<std::uint32_t> grupos_usados;
            if (q_dom.grupo_incompatibilidad != 0) {
                grupos_usados.push_back(q_dom.grupo_incompatibilidad);
            }

            for (std::size_t j = 0; j < variables.size(); ++j) {
                if (i == j) continue;
                const auto& q_sec = variables[j];
                if (!son_hipotesis_compatibles_entre_si(q_dom, q_sec)) continue;
                if (q_sec.disposicion == DisposicionHipotesis::Incompatible && q_sec.grupo_incompatibilidad != 0) {
                    if (std::find(grupos_usados.begin(), grupos_usados.end(), q_sec.grupo_incompatibilidad) != grupos_usados.end()) {
                        continue;
                    }
                    grupos_usados.push_back(q_sec.grupo_incompatibilidad);
                }
                const double coef = params.gamma_q_desfavorable * q_sec.psi_0;
                if (coef > 1e-6) {
                    terms_acomp.push_back({q_sec.id, q_sec.codigo, coef});
                    any_acomp = true;
                }
            }
            if (any_acomp) {
                agregar_combinacion(SituacionProyecto::ELU_PersistenteTransitoria, std::move(terms_acomp));
            }
        }
    }

    // 2. E.L.U. Sísmicas (Regla direccional 1.0 Ex +- 0.3 Ey y 0.3 Ex +- 1.0 Ey)
    if (params.incluir_combinaciones_sismicas || !sismicas.empty()) {
        auto construir_base_cuasipermanente = [&]() {
            std::vector<TerminoCombinacion> base;
            for (const auto& g : permanentes) {
                base.push_back({g.id, g.codigo, 1.00});
            }
            for (const auto& q : variables) {
                if (q.psi_2 > 1e-6) {
                    base.push_back({q.id, q.codigo, q.psi_2});
                }
            }
            return base;
        };

        if (sismicas.size() >= 2) {
            const auto& sx = sismicas[0];
            const auto& sy = sismicas[1];
            const std::pair<double, double> pares_dir[] = {
                {1.0, 0.3}, {1.0, -0.3}, {-1.0, 0.3}, {-1.0, -0.3},
                {0.3, 1.0}, {0.3, -1.0}, {-0.3, 1.0}, {-0.3, -1.0}
            };
            for (const auto& [cx, cy] : pares_dir) {
                auto terms = construir_base_cuasipermanente();
                terms.push_back({sx.id, sx.codigo, cx});
                terms.push_back({sy.id, sy.codigo, cy});
                agregar_combinacion(SituacionProyecto::ELU_Sismica, std::move(terms));
            }
        } else if (sismicas.size() == 1) {
            for (double signo : {1.0, -1.0}) {
                auto terms = construir_base_cuasipermanente();
                terms.push_back({sismicas[0].id, sismicas[0].codigo, signo});
                agregar_combinacion(SituacionProyecto::ELU_Sismica, std::move(terms));
            }
        }
    }

    // 3. E.L.U. Incendio (`sestlimge_*_accidentales_incendio`)
    if (params.incluir_combinaciones_incendio) {
        for (std::size_t i = 0; i < variables.size(); ++i) {
            std::vector<TerminoCombinacion> terms;
            for (const auto& g : permanentes) {
                terms.push_back({g.id, g.codigo, 1.00});
            }
            if (variables[i].psi_1 > 1e-6) {
                terms.push_back({variables[i].id, variables[i].codigo, variables[i].psi_1});
            }
            for (std::size_t j = 0; j < variables.size(); ++j) {
                if (i == j) continue;
                if (variables[j].psi_2 > 1e-6 && son_hipotesis_compatibles_entre_si(variables[i], variables[j])) {
                    terms.push_back({variables[j].id, variables[j].codigo, variables[j].psi_2});
                }
            }
            agregar_combinacion(SituacionProyecto::ELU_Incendio, std::move(terms));
        }
    }

    // 4. E.L.S. Desplazamientos / Flecha (Característica, Frecuente y Cuasipermanente)
    if (params.incluir_els_desplazamientos) {
        // Cuasipermanente: sum(G) + sum(psi_2 * Q)
        {
            std::vector<TerminoCombinacion> terms;
            for (const auto& g : permanentes) {
                terms.push_back({g.id, g.codigo, 1.00});
            }
            for (const auto& q : variables) {
                if (q.psi_2 > 1e-6) {
                    terms.push_back({q.id, q.codigo, q.psi_2});
                }
            }
            agregar_combinacion(SituacionProyecto::ELS_Cuasipermanente, std::move(terms));
        }

        // Característica y Frecuente para cada acción variable dominante
        for (std::size_t i = 0; i < variables.size(); ++i) {
            const auto& q_dom = variables[i];

            std::vector<TerminoCombinacion> terms_car;
            std::vector<TerminoCombinacion> terms_frec;
            for (const auto& g : permanentes) {
                terms_car.push_back({g.id, g.codigo, 1.00});
                terms_frec.push_back({g.id, g.codigo, 1.00});
            }
            terms_car.push_back({q_dom.id, q_dom.codigo, 1.00});
            if (q_dom.psi_1 > 1e-6) {
                terms_frec.push_back({q_dom.id, q_dom.codigo, q_dom.psi_1});
            }

            std::vector<std::uint32_t> grupos_usados;
            if (q_dom.grupo_incompatibilidad != 0) {
                grupos_usados.push_back(q_dom.grupo_incompatibilidad);
            }
            for (std::size_t j = 0; j < variables.size(); ++j) {
                if (i == j) continue;
                const auto& q_sec = variables[j];
                if (!son_hipotesis_compatibles_entre_si(q_dom, q_sec)) continue;
                if (q_sec.disposicion == DisposicionHipotesis::Incompatible && q_sec.grupo_incompatibilidad != 0) {
                    if (std::find(grupos_usados.begin(), grupos_usados.end(), q_sec.grupo_incompatibilidad) != grupos_usados.end()) {
                        continue;
                    }
                    grupos_usados.push_back(q_sec.grupo_incompatibilidad);
                }
                if (q_sec.psi_0 > 1e-6) {
                    terms_car.push_back({q_sec.id, q_sec.codigo, q_sec.psi_0});
                }
                if (q_sec.psi_2 > 1e-6) {
                    terms_frec.push_back({q_sec.id, q_sec.codigo, q_sec.psi_2});
                }
            }
            agregar_combinacion(SituacionProyecto::ELS_Caracteristica, std::move(terms_car));
            agregar_combinacion(SituacionProyecto::ELS_Frecuente, std::move(terms_frec));
        }
    }

    // 5. Tensiones sobre el terreno (`sestlimci_coeficientes_tension_del_terreno`, Conjunto C / A2: gamma_G = 1.0, gamma_Q = 1.0)
    if (params.incluir_tensiones_terreno) {
        std::vector<TerminoCombinacion> terms_base;
        for (const auto& g : permanentes) {
            terms_base.push_back({g.id, g.codigo, 1.00});
        }
        agregar_combinacion(SituacionProyecto::TensionTerreno_Persistente, terms_base);

        for (const auto& q : variables) {
            auto terms = terms_base;
            terms.push_back({q.id, q.codigo, 1.00});
            agregar_combinacion(SituacionProyecto::TensionTerreno_Persistente, std::move(terms));
        }
    }

    return resultado;
}

// ==============================================================================
// 3. P-05.3 — Acción Sísmica, Viento y Resistencia al Fuego
// ==============================================================================

double ConfiguracionSismo::calcular_aceleracion_diseno_g(double periodo_s, bool direccion_y) const noexcept {
    const double t = std::max(0.0, periodo_s);
    const double q = std::max(1.0, direccion_y ? factor_comportamiento_qy : factor_comportamiento_qx);

    if (norma == NormaSismica::EspectroUsuario && !curva_usuario.empty()) {
        if (t <= curva_usuario.front().periodo_s) return curva_usuario.front().aceleracion_g;
        if (t >= curva_usuario.back().periodo_s) return curva_usuario.back().aceleracion_g;
        for (std::size_t i = 0; i + 1 < curva_usuario.size(); ++i) {
            const auto& p0 = curva_usuario[i];
            const auto& p1 = curva_usuario[i + 1];
            if (t >= p0.periodo_s && t <= p1.periodo_s) {
                const double denom = std::max(1e-9, p1.periodo_s - p0.periodo_s);
                const double u = (t - p0.periodo_s) / denom;
                return p0.aceleracion_g + u * (p1.aceleracion_g - p0.aceleracion_g);
            }
        }
    }

    // Coeficiente de suelo C / S y periodos característicos T_A (T_B), T_B (T_C), T_D
    double s_soil = 1.0;
    double tb = 0.15;
    double tc = 0.50;
    double td = 2.00;

    switch (tipo_terreno) {
        case TipoTerrenoSismico::TipoI_RocaCompacta_A:
            s_soil = 1.00; tb = 0.12; tc = 0.40; td = 2.00; break;
        case TipoTerrenoSismico::TipoII_RocaFracturada_B:
            s_soil = 1.25; tb = 0.15; tc = 0.50; td = 2.00; break;
        case TipoTerrenoSismico::TipoIII_SueloMedio_C:
            s_soil = 1.50; tb = 0.20; tc = 0.65; td = 2.00; break;
        case TipoTerrenoSismico::TipoIV_SueloBlando_D:
            s_soil = 1.80; tb = 0.25; tc = 0.80; td = 2.00; break;
    }

    // Factor de corrección por amortiguamiento nu = (5 / xi)^0.4 (NCSE-02 / EC8)
    const double nu_amort = std::pow(5.0 / std::max(1.0, amortiguamiento_porcentaje), 0.40);
    const double ag_calc = aceleracion_basica_ag_g * factor_importancia_gamma_i * s_soil;
    const double alpha_meseta = 2.50 * nu_amort;
    const double beta_min = 0.20;

    double sd_g = 0.0;
    if (t < tb) {
        const double ratio = (tb > 1e-9) ? (t / tb) : 1.0;
        sd_g = ag_calc * (1.0 + ratio * (alpha_meseta / q - 1.0));
    } else if (t <= tc) {
        sd_g = ag_calc * (alpha_meseta / q);
    } else if (t <= td) {
        sd_g = std::max(beta_min * aceleracion_basica_ag_g, ag_calc * (alpha_meseta / q) * (tc / t));
    } else {
        sd_g = std::max(beta_min * aceleracion_basica_ag_g, ag_calc * (alpha_meseta / q) * (tc * td / (t * t)));
    }
    return sd_g;
}

std::vector<PuntoEspectroSismico> ConfiguracionSismo::generar_curva_espectro(
    std::size_t num_puntos,
    double t_max_s
) const {
    std::vector<PuntoEspectroSismico> curva;
    const std::size_t n = std::max<std::size_t>(2, num_puntos);
    curva.reserve(n);
    const double dt = t_max_s / static_cast<double>(n - 1);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) * dt;
        const double ag = calcular_aceleracion_diseno_g(t, false);
        curva.push_back(PuntoEspectroSismico{t, ag, ag * 9.80665});
    }
    return curva;
}

double ConfiguracionViento::presion_dinamica_basica_kn_m2() const noexcept {
    // q_b = 0.5 * rho_aire * v_b^2, con rho_aire = 1.25 kg/m^3 -> en kN/m^2
    double vb = velocidad_basica_m_s;
    if (norma == NormaViento::CTE_DB_SE_AE) {
        switch (zona_eolica) {
            case ZonaEolicaCte::ZonaA_26ms: return 0.42;
            case ZonaEolicaCte::ZonaB_27ms: return 0.45;
            case ZonaEolicaCte::ZonaC_29ms: return 0.52;
        }
    }
    return 0.5 * 1.25 * vb * vb * 1.0e-3;
}

double ConfiguracionViento::coeficiente_exposicion_ce(double z_m) const noexcept {
    // Fórmula CTE DB SE-AE Anejo D.2:
    // F = k * ln(max(z, Z) / L),  c_e = F * (F + 7 * k)
    double k = 0.19;
    double l_rug = 0.05;
    double z_min = 2.0;

    switch (grado_aspereza) {
        case GradoAsperezaEntorno::GradoI_BordeMar:
            k = 0.156; l_rug = 0.003; z_min = 1.0; break;
        case GradoAsperezaEntorno::GradoII_RuralLlano:
            k = 0.170; l_rug = 0.010; z_min = 1.0; break;
        case GradoAsperezaEntorno::GradoIII_RuralAccidentado:
            k = 0.190; l_rug = 0.050; z_min = 2.0; break;
        case GradoAsperezaEntorno::GradoIV_UrbanaIndustrial:
            k = 0.220; l_rug = 0.300; z_min = 5.0; break;
        case GradoAsperezaEntorno::GradoV_CentroGrandesCiudades:
            k = 0.240; l_rug = 1.000; z_min = 10.0; break;
    }

    const double z_eff = std::max(z_m, z_min);
    const double f_val = k * std::log(z_eff / l_rug);
    return std::max(1.0, f_val * (f_val + 7.0 * k));
}

std::uint32_t ConfiguracionResistenciaFuego::tiempo_requerido_minutos() const noexcept {
    switch (resistencia_requerida) {
        case ClaseResistenciaFuego::R15: return 15;
        case ClaseResistenciaFuego::R30: return 30;
        case ClaseResistenciaFuego::R45: return 45;
        case ClaseResistenciaFuego::R60: return 60;
        case ClaseResistenciaFuego::R90: return 90;
        case ClaseResistenciaFuego::R120: return 120;
        case ClaseResistenciaFuego::R180: return 180;
        case ClaseResistenciaFuego::R240: return 240;
    }
    return 60;
}

std::string ConfiguracionResistenciaFuego::etiqueta_clase() const {
    return "R " + std::to_string(tiempo_requerido_minutos());
}

double ConfiguracionResistenciaFuego::calcular_espesor_proteccion_necesario_mm(
    double masividad_am_v_m_inv,
    double temp_critica_c,
    const RevestimientoIgnifugoSpec& revestimiento
) const noexcept {
    if (tipo_proteccion_acero == TipoRevestimientoIgnifugo::SinProteccion) {
        return 0.0;
    }
    const double t_min = static_cast<double>(tiempo_requerido_minutos());
    const double am_v = std::clamp(masividad_am_v_m_inv, 20.0, 400.0);
    const double t_crit = std::clamp(temp_critica_c, 350.0, 750.0);
    // Temperatura del gas ISO 834 en el instante requerido: theta_g = 20 + 345 * log10(8*t + 1)
    const double theta_g = 20.0 + 345.0 * std::log10(8.0 * t_min + 1.0);
    const double factor_temp = std::max(0.15, (theta_g - t_crit) / std::max(100.0, t_crit - 20.0));
    const double d_p_m = (revestimiento.conductividad_w_mk * (t_min * 60.0) * am_v * factor_temp) /
                         (425.0 * 7850.0 * 0.85);
    const double d_p_mm = d_p_m * 1000.0;
    return std::clamp(d_p_mm, revestimiento.espesor_min_mm, revestimiento.espesor_max_mm);
}

const std::vector<RevestimientoIgnifugoSpec>& NormativaEngine::catalogo_pinturas_intumescentes() {
    static const std::vector<RevestimientoIgnifugoSpec> kPinturas = {
        {"PROMAPAINT_SC4", "Promat", "PROMAPAINT-SC4 (Pintura intumescente base agua)", TipoRevestimientoIgnifugo::PinturaIntumescente, 0.012, 1100.0, 1300.0, 0.25, 3.50},
        {"NULLIFIRE_SC902", "Nullifire", "Nullifire SC902 (Intumescente híbrida rápida)", TipoRevestimientoIgnifugo::PinturaIntumescente, 0.011, 1100.0, 1350.0, 0.30, 4.20},
        {"HEMPACORE_ONE", "Hempel", "Hempacore One 43600 (Intumescente acrílica)", TipoRevestimientoIgnifugo::PinturaIntumescente, 0.013, 1150.0, 1280.0, 0.25, 3.80},
        {"INTERCHAR_212", "International", "Interchar 212 (Epoxi intumescente)", TipoRevestimientoIgnifugo::PinturaIntumescente, 0.014, 1100.0, 1400.0, 0.40, 6.00}
    };
    return kPinturas;
}

const std::vector<RevestimientoIgnifugoSpec>& NormativaEngine::catalogo_morteros_intumescentes() {
    static const std::vector<RevestimientoIgnifugoSpec> kMorteros = {
        {"PERLITA_VERMICULITA", "Genérico / CYPE", "Mortero proyectado de perlita y vermiculita", TipoRevestimientoIgnifugo::MorteroProyeccion, 0.120, 1000.0, 350.0, 10.0, 50.0},
        {"PROMASPRAY_C450", "Promat", "PROMASPRAY-C450 (Mortero lana mineral)", TipoRevestimientoIgnifugo::MorteroProyeccion, 0.095, 1050.0, 280.0, 10.0, 45.0},
        {"TECPYR_800", "Mercor Tecresa", "TECPYR-800 (Mortero ignífugo base yeso/perlita)", TipoRevestimientoIgnifugo::MorteroProyeccion, 0.110, 1000.0, 320.0, 10.0, 45.0}
    };
    return kMorteros;
}

const RevestimientoIgnifugoSpec* NormativaEngine::buscar_revestimiento_ignifugo(std::string_view id) noexcept {
    for (const auto& p : catalogo_pinturas_intumescentes()) {
        if (p.id == id || p.nombre_comercial == id) return &p;
    }
    for (const auto& m : catalogo_morteros_intumescentes()) {
        if (m.id == id || m.nombre_comercial == id) return &m;
    }
    return &catalogo_pinturas_intumescentes().front();
}

// ==============================================================================
// 4. P-05.4 — Motor del Generador de Pórticos (`PorticoGeneratorEngine`)
// ==============================================================================

double PorticoGeneratorEngine::coeficiente_forma_nieve_mu1(double angulo_faldon_deg) noexcept {
    // Según CTE DB SE-AE Anejo E / EN 1991-1-3:
    // 0 <= alpha <= 30° -> mu_1 = 0.8
    // 30° < alpha < 60° -> mu_1 = 0.8 * (60 - alpha) / 30
    // alpha >= 60° -> mu_1 = 0.0
    const double alpha = std::abs(angulo_faldon_deg);
    if (alpha <= 30.0) return 0.80;
    if (alpha >= 60.0) return 0.00;
    return 0.80 * (60.0 - alpha) / 30.0;
}

double PorticoGeneratorEngine::calcular_carga_nieve_faldon_kn_m2(
    const ConfiguracionGeneradorPorticos& cfg,
    double angulo_faldon_deg
) noexcept {
    if (!cfg.con_sobrecarga_nieve) return 0.0;
    const double mu1 = coeficiente_forma_nieve_mu1(angulo_faldon_deg);
    return mu1 * cfg.carga_nieve_horizontal_sk_kn_m2;
}

ResultadoDimensionamientoCorreas PorticoGeneratorEngine::dimensionar_correas_cubierta(
    const ConfiguracionGeneradorPorticos& cfg
) {
    ResultadoDimensionamientoCorreas res;
    res.separacion_m = std::max(0.50, cfg.correas.separacion_cubierta_m);

    const VanoPorticoSpec vano = cfg.vanos.empty() ? VanoPorticoSpec{} : cfg.vanos.front();
    const double semiluz = (vano.tipologia == TipologiaVanoPortico::UnAgua) ? vano.luz_m : (0.5 * vano.luz_m);
    const double dz = (vano.tipologia == TipologiaVanoPortico::UnAgua)
        ? std::abs(vano.altura_pilar_der_m - vano.altura_pilar_izq_m)
        : std::abs(vano.altura_cumbrera_m - vano.altura_pilar_izq_m);
    const double long_faldon = std::hypot(semiluz, dz);
    const double angulo_deg = std::atan2(dz, std::max(0.1, semiluz)) * (180.0 / std::numbers::pi);

    res.num_correas_por_faldon = static_cast<std::uint32_t>(std::ceil(long_faldon / res.separacion_m)) + 1;

    // Cargas de diseño sobre una correa de vano L = separacion_entre_porticos_m
    const double l_vano = std::max(1.0, cfg.separacion_entre_porticos_m);
    const double q_perm = (cfg.peso_cerramiento_cubierta_kn_m2 + 0.08) * res.separacion_m; // kN/m
    const double q_nieve = calcular_carga_nieve_faldon_kn_m2(cfg, angulo_deg) * res.separacion_m;
    const double q_uso = cfg.sobrecarga_cubierta_kn_m2 * res.separacion_m;
    const double q_var = std::max(q_nieve, q_uso);

    const double q_elu = 1.35 * q_perm + 1.50 * q_var; // kN/m
    const double q_els = q_perm + q_var;               // kN/m

    // Coeficientes de momento y flecha según número de vanos continuos
    double coef_m = 1.0 / 8.0;
    double coef_f = 5.0 / 384.0;
    if (cfg.correas.tipo_vano == TipoVanoCorrea::Continuo_2Vanos) {
        coef_m = 1.0 / 8.0;
        coef_f = 2.08 / 384.0;
    } else if (cfg.correas.tipo_vano == TipoVanoCorrea::Continuo_3Vanos) {
        coef_m = 0.100;
        coef_f = 2.60 / 384.0;
    }

    const double m_ed_knm = coef_m * q_elu * l_vano * l_vano;
    const double flecha_adm_mm = (l_vano * 1000.0) / std::max(100.0, cfg.correas.limite_flecha_relativa);

    const auto* acero = NormativaEngine::buscar_acero_estructural(cfg.correas.acero_correa);
    const double fy_mpa = acero ? acero->fy_nominal_mpa : 275.0;
    const double e_mpa = acero ? acero->e_mpa : 210000.0;

    struct SerieCorreaCandidata {
        const char* nombre;
        double wy_cm3;
        double iy_cm4;
        double peso_kg_m;
    };

    static constexpr SerieCorreaCandidata kCandidatasZF[] = {
        {"ZF-140x2.0", 21.8, 152.6, 4.45},
        {"ZF-160x2.0", 27.5, 220.0, 4.95},
        {"ZF-180x2.0", 33.8, 304.2, 5.45},
        {"ZF-200x2.0", 40.8, 408.0, 5.96},
        {"ZF-200x2.5", 50.2, 502.0, 7.42},
        {"ZF-225x2.5", 62.1, 698.6, 8.15},
        {"ZF-250x2.5", 75.4, 942.5, 8.95},
        {"ZF-250x3.0", 89.6, 1120.0, 10.70},
        {"ZF-300x3.0", 124.5, 1867.5, 12.60},
        {"IPE 140", 77.3, 541.2, 12.90},
        {"IPE 160", 109.0, 869.3, 15.80},
        {"IPE 180", 146.0, 1317.0, 18.80}
    };

    const SerieCorreaCandidata* elegida = &kCandidatasZF[4];
    for (const auto& cand : kCandidatasZF) {
        const double m_rd_knm = (cand.wy_cm3 * 1e-3 * fy_mpa) / 1.05;
        const double ratio_m = (m_rd_knm > 1e-9) ? (m_ed_knm / m_rd_knm) * 100.0 : 100.0;

        // Flecha en mm: f = coef_f * q(N/mm) * L(mm)^4 / (E(MPa) * I(mm^4))
        const double l_mm = l_vano * 1000.0;
        const double i_mm4 = cand.iy_cm4 * 1e4;
        const double flecha_mm = (coef_f * q_els * std::pow(l_mm, 4)) / (e_mpa * i_mm4);
        const double ratio_f = (flecha_adm_mm > 1e-9) ? (flecha_mm / flecha_adm_mm) * 100.0 : 100.0;

        if (ratio_m <= 100.0 && ratio_f <= 100.0) {
            elegida = &cand;
            res.aprovechamiento_tension_pct = ratio_m;
            res.aprovechamiento_flecha_pct = ratio_f;
            res.cumple = true;
            break;
        }
        elegida = &cand;
        res.aprovechamiento_tension_pct = ratio_m;
        res.aprovechamiento_flecha_pct = ratio_f;
        res.cumple = false;
    }

    res.perfil_optimo = elegida->nombre;
    const std::uint32_t num_faldones = (vano.tipologia == TipologiaVanoPortico::UnAgua) ? 1u : 2u;
    const double long_total_nave = static_cast<double>(cfg.num_modulos_longitudinales) * l_vano;
    res.peso_total_correas_kg =
        static_cast<double>( std::max<std::size_t>(1, cfg.vanos.size()) * num_faldones * res.num_correas_por_faldon ) *
        long_total_nave * elegida->peso_kg_m;

    return res;
}

ResumenExportacionGeneradorPorticos PorticoGeneratorEngine::exportar_a_obra(
    const ConfiguracionGeneradorPorticos& cfg,
    model::Obra& obra
) {
    ResumenExportacionGeneradorPorticos resumen{};
    resumen.dimensionamiento_correas = dimensionar_correas_cubierta(cfg);

    auto& est = obra.estructura_mut();
    est.nudos.clear();
    est.barras.clear();
    est.panyos.clear();
    est.laminas.clear();
    est.hipotesis.clear();
    est.cargas_superficiales.clear();
    est.cargas_lineales.clear();
    est.cargas_nudos.clear();
    est.next_nudo_id = 1;
    est.next_barra_id = 1;
    est.next_panyo_id = 1;
    est.next_hipotesis_id = 1;
    est.next_carga_sup_id = 1;
    est.next_carga_lin_id = 1;
    est.next_carga_nudo_id = 1;

    // 1. Crear Capas organizadas por familia constructiva (`dcp_*`)
    const std::uint32_t capa_pilares = obra.add_etiqueta_capa("Pilares", 0x0078D7, true, false);
    const std::uint32_t capa_dinteles = obra.add_etiqueta_capa("Dinteles", 0xD83B01, true, false);
    const std::uint32_t capa_vigas_atado = obra.add_etiqueta_capa("Vigas de atado", 0x107C10, true, false);
    const std::uint32_t capa_hastiales = obra.add_etiqueta_capa("Pilares hastiales", 0x8764B8, true, false);

    // 2. Crear Hipótesis de Carga (`cb3hipusr_crea_hipotesis_para_exportacion_generador_porticos`)
    const std::uint32_t hip_pp = obra.add_hipotesis_carga("PP", "Peso propio", 0, 0, 0, 0, true);
    const std::uint32_t hip_cm = obra.add_hipotesis_carga("CM", "Cargas muertas (Cerramiento)", 1, 0, 0, 0, true);
    const std::uint32_t hip_sc = obra.add_hipotesis_carga("Q1", "Sobrecarga de uso en cubierta (G1)", 2, 1, 0, 0, true);
    std::uint32_t hip_nieve = 0;
    if (cfg.con_sobrecarga_nieve) {
        hip_nieve = obra.add_hipotesis_carga("N1", "Nieve (CTE DB SE-AE)", 5, 1, 0, 0, true);
    }

    struct HipotesisVientoDir {
        std::uint32_t id{0};
        std::string codigo;
        double cpe_barlovento_fachada{0.80};
        double cpe_sotavento_fachada{-0.50};
        double cpe_cubierta_barlovento{-0.70};
        double cpe_cubierta_sotavento{-0.40};
        double cpi{0.0};
    };
    std::vector<HipotesisVientoDir> hips_viento;
    if (cfg.viento.activo) {
        if (cfg.edificio_con_huecos_abiertos) {
            hips_viento.push_back({obra.add_hipotesis_carga("V1", "Viento 0° (+X) Presión interior", 3, 2, 0, 1, true), "V1", 0.80, -0.50, -0.60, -0.40, +0.20});
            hips_viento.push_back({obra.add_hipotesis_carga("V2", "Viento 0° (+X) Succión interior", 3, 2, 0, 1, true), "V2", 0.80, -0.50, -0.60, -0.40, -0.30});
            hips_viento.push_back({obra.add_hipotesis_carga("V3", "Viento 180° (-X) Presión interior", 3, 2, 0, 1, true), "V3", -0.50, 0.80, -0.40, -0.60, +0.20});
            hips_viento.push_back({obra.add_hipotesis_carga("V4", "Viento 180° (-X) Succión interior", 3, 2, 0, 1, true), "V4", -0.50, 0.80, -0.40, -0.60, -0.30});
            hips_viento.push_back({obra.add_hipotesis_carga("V5", "Viento 90° (+Y) Presión interior", 3, 2, 0, 1, true), "V5", -0.70, -0.70, -0.80, -0.80, +0.20});
            hips_viento.push_back({obra.add_hipotesis_carga("V6", "Viento 90° (+Y) Succión interior", 3, 2, 0, 1, true), "V6", -0.70, -0.70, -0.80, -0.80, -0.30});
            hips_viento.push_back({obra.add_hipotesis_carga("V7", "Viento 270° (-Y) Presión interior", 3, 2, 0, 1, true), "V7", -0.70, -0.70, -0.80, -0.80, +0.20});
            hips_viento.push_back({obra.add_hipotesis_carga("V8", "Viento 270° (-Y) Succión interior", 3, 2, 0, 1, true), "V8", -0.70, -0.70, -0.80, -0.80, -0.30});
        } else {
            hips_viento.push_back({obra.add_hipotesis_carga("V1", "Viento 0° (+X)", 3, 2, 0, 1, true), "V1", 0.80, -0.50, -0.60, -0.40, 0.0});
            hips_viento.push_back({obra.add_hipotesis_carga("V2", "Viento 180° (-X)", 3, 2, 0, 1, true), "V2", -0.50, 0.80, -0.40, -0.60, 0.0});
            hips_viento.push_back({obra.add_hipotesis_carga("V3", "Viento 90° (+Y)", 3, 2, 0, 1, true), "V3", -0.70, -0.70, -0.80, -0.80, 0.0});
            hips_viento.push_back({obra.add_hipotesis_carga("V4", "Viento 270° (-Y)", 3, 2, 0, 1, true), "V4", -0.70, -0.70, -0.80, -0.80, 0.0});
        }
    }
    (void)hip_pp;

    // 3. Generación de Nudos y Barras de los Pórticos Transversales (`ed3genport_append_barra`)
    const std::vector<VanoPorticoSpec> vanos_efectivos =
        cfg.vanos.empty() ? std::vector<VanoPorticoSpec>{VanoPorticoSpec{}} : cfg.vanos;
    const std::uint32_t num_porticos = std::max<std::uint32_t>(2, cfg.num_modulos_longitudinales + 1);
    const double sep_y = std::max(1.0, cfg.separacion_entre_porticos_m);
    const model::VinculacionExterior vinc_base = cfg.apoyos_empotrados
        ? model::VinculacionExterior::Empotrado
        : model::VinculacionExterior::ArticuladoFijo;

    struct NudosVanoPorPortico {
        std::uint32_t base_izq{0};
        std::uint32_t alero_izq{0};
        std::uint32_t cumbrera{0};
        std::uint32_t alero_der{0};
        std::uint32_t base_der{0};
        std::uint32_t barra_pilar_izq{0};
        std::uint32_t barra_pilar_der{0};
        std::uint32_t barra_dintel_izq{0};
        std::uint32_t barra_dintel_der{0};
    };

    // Matriz [portico_idx][vano_idx]
    std::vector<std::vector<NudosVanoPorPortico>> malla_porticos(
        num_porticos, std::vector<NudosVanoPorPortico>(vanos_efectivos.size())
    );

    for (std::uint32_t ip = 0; ip < num_porticos; ++ip) {
        const double y = static_cast<double>(ip) * sep_y;
        double x_offset = 0.0;

        for (std::size_t iv = 0; iv < vanos_efectivos.size(); ++iv) {
            const auto& v = vanos_efectivos[iv];
            const double x_izq = x_offset;
            const double x_med = x_offset + 0.5 * v.luz_m;
            const double x_der = x_offset + v.luz_m;

            NudosVanoPorPortico nv{};
            if (iv == 0) {
                nv.base_izq = obra.add_nudo({x_izq, y, 0.0}, vinc_base);
                nv.alero_izq = obra.add_nudo({x_izq, y, v.altura_pilar_izq_m}, model::VinculacionExterior::Libre);
                nv.barra_pilar_izq = obra.add_barra(nv.base_izq, nv.alero_izq, v.perfil_pilares, "S275");
                if (auto* b = obra.find_barra_mut(nv.barra_pilar_izq)) {
                    b->tipo_estructural = model::TipoBarraEstructural::ColumnaPilar;
                    b->capa_id = capa_pilares;
                }
            } else {
                // Comparte pilar interior con el vano adosado izquierdo
                nv.base_izq = malla_porticos[ip][iv - 1].base_der;
                nv.alero_izq = malla_porticos[ip][iv - 1].alero_der;
                nv.barra_pilar_izq = malla_porticos[ip][iv - 1].barra_pilar_der;
            }

            nv.base_der = obra.add_nudo({x_der, y, 0.0}, vinc_base);
            nv.alero_der = obra.add_nudo({x_der, y, v.altura_pilar_der_m}, model::VinculacionExterior::Libre);
            nv.barra_pilar_der = obra.add_barra(nv.base_der, nv.alero_der, v.perfil_pilares, "S275");
            if (auto* b = obra.find_barra_mut(nv.barra_pilar_der)) {
                b->tipo_estructural = model::TipoBarraEstructural::ColumnaPilar;
                b->capa_id = capa_pilares;
            }

            if (v.tipologia == TipologiaVanoPortico::UnAgua) {
                nv.cumbrera = nv.alero_der;
                nv.barra_dintel_izq = obra.add_barra(nv.alero_izq, nv.alero_der, v.perfil_dinteles, "S275");
                nv.barra_dintel_der = nv.barra_dintel_izq;
                if (auto* b = obra.find_barra_mut(nv.barra_dintel_izq)) {
                    b->tipo_estructural = model::TipoBarraEstructural::Viga;
                    b->capa_id = capa_dinteles;
                }
            } else {
                nv.cumbrera = obra.add_nudo({x_med, y, v.altura_cumbrera_m}, model::VinculacionExterior::Libre);
                nv.barra_dintel_izq = obra.add_barra(nv.alero_izq, nv.cumbrera, v.perfil_dinteles, "S275");
                nv.barra_dintel_der = obra.add_barra(nv.cumbrera, nv.alero_der, v.perfil_dinteles, "S275");
                if (auto* b1 = obra.find_barra_mut(nv.barra_dintel_izq)) {
                    b1->tipo_estructural = model::TipoBarraEstructural::Viga;
                    b1->capa_id = capa_dinteles;
                }
                if (auto* b2 = obra.find_barra_mut(nv.barra_dintel_der)) {
                    b2->tipo_estructural = model::TipoBarraEstructural::Viga;
                    b2->capa_id = capa_dinteles;
                }

                // Si es celosía, añade cordón inferior y montantes centrales
                if (v.tipologia == TipologiaVanoPortico::Celosia) {
                    const double z_inf = std::min(v.altura_pilar_izq_m, v.altura_pilar_der_m);
                    const std::uint32_t nudo_inf_centro = obra.add_nudo({x_med, y, z_inf}, model::VinculacionExterior::Libre);
                    const std::uint32_t b_inf1 = obra.add_barra(nv.alero_izq, nudo_inf_centro, v.perfil_cordon_inferior, "S275");
                    const std::uint32_t b_inf2 = obra.add_barra(nudo_inf_centro, nv.alero_der, v.perfil_cordon_inferior, "S275");
                    const std::uint32_t b_mont = obra.add_barra(nudo_inf_centro, nv.cumbrera, v.perfil_montantes_diagonales, "S275");
                    if (auto* p = obra.find_barra_mut(b_inf1)) p->capa_id = capa_dinteles;
                    if (auto* p = obra.find_barra_mut(b_inf2)) p->capa_id = capa_dinteles;
                    if (auto* p = obra.find_barra_mut(b_mont)) p->capa_id = capa_dinteles;
                }
            }

            // Pilares hastiales en pórtico frontal (ip == 0) y trasero (ip == num_porticos - 1)
            if (cfg.incluir_pilares_hastiales && (ip == 0 || ip + 1 == num_porticos)) {
                const std::uint32_t n_hast = std::max<std::uint32_t>(1, cfg.num_pilares_hastiales_por_vano);
                for (std::uint32_t ih = 1; ih <= n_hast; ++ih) {
                    const double frac = static_cast<double>(ih) / static_cast<double>(n_hast + 1);
                    const double x_h = x_izq + frac * v.luz_m;
                    double z_top = v.altura_pilar_izq_m;
                    if (v.tipologia == TipologiaVanoPortico::UnAgua) {
                        z_top = v.altura_pilar_izq_m + frac * (v.altura_pilar_der_m - v.altura_pilar_izq_m);
                    } else {
                        const double dist_rel_centro = 1.0 - std::abs(2.0 * frac - 1.0);
                        z_top = v.altura_pilar_izq_m + dist_rel_centro * (v.altura_cumbrera_m - v.altura_pilar_izq_m);
                    }
                    const std::uint32_t n_base_h = obra.add_nudo({x_h, y, 0.0}, model::VinculacionExterior::ArticuladoFijo);
                    const std::uint32_t n_top_h = obra.add_nudo({x_h, y, z_top}, model::VinculacionExterior::Libre);
                    const std::uint32_t b_hast = obra.add_barra(n_base_h, n_top_h, "HEA 180", "S275");
                    if (auto* bh = obra.find_barra_mut(b_hast)) {
                        bh->tipo_estructural = model::TipoBarraEstructural::ColumnaPilar;
                        bh->capa_id = capa_hastiales;
                    }
                }
            }

            malla_porticos[ip][iv] = nv;
            x_offset += v.luz_m;
        }
    }

    // 4. Vigas de atado longitudinales en aleros y cumbreras entre pórticos consecutivos
    for (std::uint32_t ip = 0; ip + 1 < num_porticos; ++ip) {
        for (std::size_t iv = 0; iv < vanos_efectivos.size(); ++iv) {
            const auto& p0 = malla_porticos[ip][iv];
            const auto& p1 = malla_porticos[ip + 1][iv];

            if (iv == 0) {
                const std::uint32_t b_al_izq = obra.add_barra(p0.alero_izq, p1.alero_izq, "IPE 160", "S275");
                if (auto* b = obra.find_barra_mut(b_al_izq)) b->capa_id = capa_vigas_atado;
            }
            const std::uint32_t b_al_der = obra.add_barra(p0.alero_der, p1.alero_der, "IPE 160", "S275");
            if (auto* b = obra.find_barra_mut(b_al_der)) b->capa_id = capa_vigas_atado;

            if (vanos_efectivos[iv].tipologia != TipologiaVanoPortico::UnAgua) {
                const std::uint32_t b_cumb = obra.add_barra(p0.cumbrera, p1.cumbrera, "IPE 160", "S275");
                if (auto* b = obra.find_barra_mut(b_cumb)) b->capa_id = capa_vigas_atado;
            }
        }
    }

    // 5. Paños de Cubierta y Fachadas + Cargas Superficiales y Lineales (`ed3genport_append_panyo`, `ed3genport_append_superficie_carga`)
    const double y_min = 0.0;
    const double y_max = static_cast<double>(num_porticos - 1) * sep_y;
    double x_offset = 0.0;

    for (std::size_t iv = 0; iv < vanos_efectivos.size(); ++iv) {
        const auto& v = vanos_efectivos[iv];
        const double x_izq = x_offset;
        const double x_med = x_offset + 0.5 * v.luz_m;
        const double x_der = x_offset + v.luz_m;

        const double semiluz = (v.tipologia == TipologiaVanoPortico::UnAgua) ? v.luz_m : (0.5 * v.luz_m);
        const double dz = (v.tipologia == TipologiaVanoPortico::UnAgua)
            ? std::abs(v.altura_pilar_der_m - v.altura_pilar_izq_m)
            : std::abs(v.altura_cumbrera_m - v.altura_pilar_izq_m);
        const double angulo_deg = std::atan2(dz, std::max(0.1, semiluz)) * (180.0 / std::numbers::pi);
        const double q_nieve_faldon = calcular_carga_nieve_faldon_kn_m2(cfg, angulo_deg);
        const double qe_cubierta = cfg.viento.presion_estatica_z_kn_m2(v.altura_cumbrera_m);
        const double qe_fachada = cfg.viento.presion_estatica_z_kn_m2(v.altura_pilar_izq_m);

        // Paño de faldón izquierdo de cubierta
        const std::uint32_t panyo_cub_izq = obra.add_panyo(
            {{x_izq, y_min, v.altura_pilar_izq_m},
             {x_med, y_min, v.altura_cumbrera_m},
             {x_med, y_max, v.altura_cumbrera_m},
             {x_izq, y_max, v.altura_pilar_izq_m}},
            {0.0, 1.0, 0.0},
            capa_dinteles
        );

        // Paño de faldón derecho de cubierta (si es a dos aguas o celosía)
        std::uint32_t panyo_cub_der = panyo_cub_izq;
        if (v.tipologia != TipologiaVanoPortico::UnAgua) {
            panyo_cub_der = obra.add_panyo(
                {{x_med, y_min, v.altura_cumbrera_m},
                 {x_der, y_min, v.altura_pilar_der_m},
                 {x_der, y_max, v.altura_pilar_der_m},
                 {x_med, y_max, v.altura_cumbrera_m}},
                {0.0, 1.0, 0.0},
                capa_dinteles
            );
        }

        // Cargas superficiales en los faldones de cubierta
        for (std::uint32_t pid : {panyo_cub_izq, panyo_cub_der}) {
            if (cfg.cerramiento_cubierta) {
                obra.add_carga_superficial_panyo(pid, hip_cm, cfg.peso_cerramiento_cubierta_kn_m2, {0.0, 0.0, -1.0}, false, "Cerramiento cubierta");
            }
            obra.add_carga_superficial_panyo(pid, hip_sc, cfg.sobrecarga_cubierta_kn_m2, {0.0, 0.0, -1.0}, false, "Sobrecarga cubierta G1");
            if (hip_nieve != 0 && q_nieve_faldon > 1e-6) {
                obra.add_carga_superficial_panyo(pid, hip_nieve, q_nieve_faldon, {0.0, 0.0, -1.0}, false, "Nieve en faldón");
            }
            for (const auto& hv : hips_viento) {
                const double cpe = (pid == panyo_cub_izq) ? hv.cpe_cubierta_barlovento : hv.cpe_cubierta_sotavento;
                const double q_viento = qe_cubierta * (cpe - hv.cpi);
                obra.add_carga_superficial_panyo(pid, hv.id, q_viento, {0.0, 0.0, 1.0}, true, "Viento cubierta (" + hv.codigo + ")");
            }
            if (v.tipologia == TipologiaVanoPortico::UnAgua) break;
        }

        // Paños de fachadas laterales en el primer y último vano
        if (cfg.cerramiento_lateral) {
            if (iv == 0) {
                const std::uint32_t panyo_fach_izq = obra.add_panyo(
                    {{x_izq, y_min, 0.0},
                     {x_izq, y_max, 0.0},
                     {x_izq, y_max, v.altura_pilar_izq_m},
                     {x_izq, y_min, v.altura_pilar_izq_m}},
                    {0.0, 1.0, 0.0},
                    capa_pilares
                );
                obra.add_carga_superficial_panyo(panyo_fach_izq, hip_cm, cfg.peso_cerramiento_lateral_kn_m2, {0.0, 0.0, -1.0}, false, "Cerramiento fachada izq");
                for (const auto& hv : hips_viento) {
                    const double q_viento = qe_fachada * (hv.cpe_barlovento_fachada - hv.cpi);
                    obra.add_carga_superficial_panyo(panyo_fach_izq, hv.id, q_viento, {1.0, 0.0, 0.0}, false, "Viento fachada izq (" + hv.codigo + ")");
                }
            }
            if (iv + 1 == vanos_efectivos.size()) {
                const std::uint32_t panyo_fach_der = obra.add_panyo(
                    {{x_der, y_min, 0.0},
                     {x_der, y_max, 0.0},
                     {x_der, y_max, v.altura_pilar_der_m},
                     {x_der, y_min, v.altura_pilar_der_m}},
                    {0.0, 1.0, 0.0},
                    capa_pilares
                );
                obra.add_carga_superficial_panyo(panyo_fach_der, hip_cm, cfg.peso_cerramiento_lateral_kn_m2, {0.0, 0.0, -1.0}, false, "Cerramiento fachada der");
                for (const auto& hv : hips_viento) {
                    const double q_viento = qe_fachada * (hv.cpe_sotavento_fachada - hv.cpi);
                    obra.add_carga_superficial_panyo(panyo_fach_der, hv.id, q_viento, {-1.0, 0.0, 0.0}, false, "Viento fachada der (" + hv.codigo + ")");
                }
            }
        }

        // Cargas lineales equivalentes sobre los dinteles y pilares de cada pórtico (`ed3genport_append_carga_a_ultima_barra`)
        for (std::uint32_t ip = 0; ip < num_porticos; ++ip) {
            const double ancho_tributario = (ip == 0 || ip + 1 == num_porticos) ? (0.5 * sep_y) : sep_y;
            const auto& nv = malla_porticos[ip][iv];

            if (cfg.cerramiento_cubierta) {
                const double q_lin_cm = cfg.peso_cerramiento_cubierta_kn_m2 * ancho_tributario;
                obra.add_carga_lineal_barra(nv.barra_dintel_izq, hip_cm, q_lin_cm, q_lin_cm, {0.0, 0.0, -1.0}, false);
                if (nv.barra_dintel_der != nv.barra_dintel_izq) {
                    obra.add_carga_lineal_barra(nv.barra_dintel_der, hip_cm, q_lin_cm, q_lin_cm, {0.0, 0.0, -1.0}, false);
                }
            }
            const double q_lin_sc = cfg.sobrecarga_cubierta_kn_m2 * ancho_tributario;
            obra.add_carga_lineal_barra(nv.barra_dintel_izq, hip_sc, q_lin_sc, q_lin_sc, {0.0, 0.0, -1.0}, false);
            if (nv.barra_dintel_der != nv.barra_dintel_izq) {
                obra.add_carga_lineal_barra(nv.barra_dintel_der, hip_sc, q_lin_sc, q_lin_sc, {0.0, 0.0, -1.0}, false);
            }

            if (hip_nieve != 0 && q_nieve_faldon > 1e-6) {
                const double q_lin_nv = q_nieve_faldon * ancho_tributario;
                obra.add_carga_lineal_barra(nv.barra_dintel_izq, hip_nieve, q_lin_nv, q_lin_nv, {0.0, 0.0, -1.0}, false);
                if (nv.barra_dintel_der != nv.barra_dintel_izq) {
                    obra.add_carga_lineal_barra(nv.barra_dintel_der, hip_nieve, q_lin_nv, q_lin_nv, {0.0, 0.0, -1.0}, false);
                }
            }
        }

        x_offset += v.luz_m;
    }

    // 6. Calcular resumen de combinaciones generadas
    std::vector<HipotesisCargaSpec> specs;
    for (const auto& h : est.hipotesis) {
        specs.push_back(HipotesisCargaSpec{
            h.id,
            h.codigo,
            h.descripcion,
            static_cast<NaturalezaHipotesis>(h.naturaleza),
            static_cast<DisposicionHipotesis>(h.disposicion),
            static_cast<CodigoCategoriaUso>(h.categoria_uso),
            h.grupo_incompatibilidad,
            h.es_automatica,
            h.actua
        });
    }
    const auto combs = NormativaEngine::generar_combinaciones(specs);

    resumen.nudos_creados = est.nudos.size();
    resumen.barras_creadas = est.barras.size();
    resumen.panyos_creados = est.panyos.size();
    resumen.hipotesis_creadas = est.hipotesis.size();
    resumen.cargas_superficiales_creadas = est.cargas_superficiales.size();
    resumen.cargas_lineales_creadas = est.cargas_lineales.size();
    resumen.combinaciones_generadas = combs.size();

    obra.undo_redo().clear();
    return resumen;
}

} // namespace cypeclon::codes
