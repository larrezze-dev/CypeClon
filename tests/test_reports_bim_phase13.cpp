#include "gui/app_window.hpp"
#include "io/reports_drawings_bim_engine.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void check_p13(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::cerr << "[FAIL Phase 13] " << file << ":" << line << " -> " << expr << "\n";
        std::exit(1);
    }
}

#define TEST_P13_ASSERT(expr) check_p13((expr), #expr, __FILE__, __LINE__)

} // namespace

void run_reports_bim_phase13_tests() {
    using namespace cypeclon;
    using namespace cypeclon::io;

    // 1. Construir una obra completa con barras de acero laminado, conformado y madera, lámina, uniones, placas y cimentación
    model::Obra obra;
    obra.datos_generales_mut().clave = "NAVE_BIM_2027";
    obra.datos_generales_mut().descripcion = "Nave Industrial con Cimentación y Uniones";

    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, model::VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({6.0, 0.0, 0.0}, model::VinculacionExterior::Empotrado);
    const auto n3 = obra.add_nudo({0.0, 0.0, 4.0}, model::VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({6.0, 0.0, 4.0}, model::VinculacionExterior::Libre);

    obra.add_barra(n1, n3, "HEB 200", "S275");
    obra.add_barra(n2, n4, "HEB 200", "S275");
    obra.add_barra(n3, n4, "IPE 240", "S275");
    obra.add_barra(n3, n4, "ZF 200x2.5", "S235");
    obra.add_barra(n1, n2, "RECT 140x240", "C24");
    obra.add_lamina({{0.0, 0.0, 0.0}, {6.0, 0.0, 0.0}, {6.0, 4.0, 0.0}, {0.0, 4.0, 0.0}}, 0.20, "C25/30");
    obra.add_carga_puntual_nudo(n3, 1, {12.0, 0.0, -180.0});
    obra.add_carga_puntual_nudo(n4, 1, {12.0, 0.0, -180.0});

    const auto solver_res = solver::SolverEngine::analyze_linear_static(obra);
    const auto design_res = codes::MemberDesignEngine::comprobar_todas_las_barras(obra, solver_res);
    const auto baseplates = connections::ConnectionsEngine::generar_todas_las_placas_anclaje(obra);
    const auto unions = connections::ConnectionsEngine::detectar_y_generar_uniones_obra(obra, solver_res);
    const auto found_model = foundation::FoundationEngine::generate_automatic_foundation(obra, &solver_res, false, true);

    // 2. P-14.1 (A-032): Medición de estructura (mppzperf.dll) y Listados de Obra en 6 formatos
    const auto takeoff = ReportsDrawingsBimEngine::compute_structure_takeoff(obra, &baseplates);
    TEST_P13_ASSERT(!takeoff.partidas_acero_laminado.empty());
    TEST_P13_ASSERT(!takeoff.partidas_acero_conformado.empty());
    TEST_P13_ASSERT(!takeoff.partidas_madera.empty());
    TEST_P13_ASSERT(takeoff.longitud_total_barras_m > 20.0);
    TEST_P13_ASSERT(takeoff.superficie_total_pintura_m2 > 5.0);
    TEST_P13_ASSERT(takeoff.peso_total_acero_laminado_kg > 200.0);
    TEST_P13_ASSERT(takeoff.peso_total_placas_y_pernos_kg > 10.0);

    const auto doc = ReportsDrawingsBimEngine::generate_project_report(
        obra, &solver_res, &design_res, &baseplates, &unions, &found_model);
    TEST_P13_ASSERT(doc.capitulos.size() >= 9);

    const auto txt_out = ReportsDrawingsBimEngine::export_report(doc, FormatoExportacionListado::TXT);
    const auto html_out = ReportsDrawingsBimEngine::export_report(doc, FormatoExportacionListado::HTML);
    const auto rtf_out = ReportsDrawingsBimEngine::export_report(doc, FormatoExportacionListado::RTF);
    const auto docx_out = ReportsDrawingsBimEngine::export_report(doc, FormatoExportacionListado::DOCX_XML);
    const auto pdf_out = ReportsDrawingsBimEngine::export_report(doc, FormatoExportacionListado::PDF);
    const auto csv_out = ReportsDrawingsBimEngine::export_report(doc, FormatoExportacionListado::CSV);

    TEST_P13_ASSERT(txt_out.find("LISTADO DE OBRA CYPE 3D") != std::string::npos);
    TEST_P13_ASSERT(html_out.find("<!DOCTYPE html>") != std::string::npos);
    TEST_P13_ASSERT(rtf_out.find("{\\rtf1") != std::string::npos);
    TEST_P13_ASSERT(docx_out.find("<w:document") != std::string::npos);
    TEST_P13_ASSERT(pdf_out.find("%PDF-1.4") != std::string::npos);
    TEST_P13_ASSERT(csv_out.find("HEB 200") != std::string::npos);

    // 3. P-14.2 (A-033): Planos 2D/3D, Capas DXF (`undxf.dll`) y Detalles Constructivos (`det_*.dll`)
    const auto layers = ReportsDrawingsBimEngine::build_standard_dxf_drawing_layers();
    TEST_P13_ASSERT(layers.size() >= 15);

    const auto details = ReportsDrawingsBimEngine::build_constructive_details_catalog();
    TEST_P13_ASSERT(details.size() >= 5);

    const auto sheet = ReportsDrawingsBimEngine::compose_drawing_sheet(
        obra,
        TipoPlanoObra::Estructura3D,
        FormatoHojaPlano::ISO_A1,
        100.0,
        &baseplates,
        &unions,
        &found_model,
        {"EAC001", "CSZ002"});
    TEST_P13_ASSERT(sheet.ancho_hoja_mm == 841.0 && sheet.alto_hoja_mm == 594.0);
    TEST_P13_ASSERT(sheet.detalles_insertados.size() == 2);
    TEST_P13_ASSERT(!sheet.lineas.empty() && !sheet.textos.empty());

    const auto dxf_sheet = ReportsDrawingsBimEngine::export_drawing_to_dxf(sheet);
    const auto svg_sheet = ReportsDrawingsBimEngine::export_drawing_to_svg_dwg_xml(sheet);
    const auto pdf_sheet = ReportsDrawingsBimEngine::export_drawing_to_pdf(sheet);
    TEST_P13_ASSERT(dxf_sheet.find("SECTION") != std::string::npos && dxf_sheet.find("DETALLES_CONSTRUCTIVOS") != std::string::npos);
    TEST_P13_ASSERT(svg_sheet.find("<svg") != std::string::npos);
    TEST_P13_ASSERT(pdf_sheet.find("%PDF-1.4") != std::string::npos);

    // 4. P-14.3 (A-034): Importación DXF/TXT (`imporstr.dll`) y Exportación IFC2x3/IFC4, glTF 2.0, CIS/2, Tekla y TecnoMETAL
    model::Obra obra_imp_dxf;
    const std::string sample_dxf =
        "0\nSECTION\n2\nENTITIES\n"
        "0\nLINE\n8\nESTRUCTURA\n10\n0.0\n20\n0.0\n30\n0.0\n11\n0.0\n21\n0.0\n31\n4.5\n"
        "0\nLINE\n8\nESTRUCTURA\n10\n0.0\n20\n0.0\n30\n4.5\n11\n8.0\n21\n0.0\n31\n4.5\n"
        "0\nENDSEC\n0\nEOF\n";
    const auto imp_dxf_res = ReportsDrawingsBimEngine::import_geometry_from_dxf(obra_imp_dxf, sample_dxf, "IPE 220", "S275");
    TEST_P13_ASSERT(imp_dxf_res.exito && imp_dxf_res.nudos_importados == 3 && imp_dxf_res.barras_importadas == 2);

    model::Obra obra_imp_txt;
    const std::string sample_txt =
        "# Nudos y barras por coordenadas\n"
        "0.0 0.0 0.0  0.0 0.0 3.5\n"
        "0.0 0.0 3.5  5.0 0.0 3.5\n";
    const auto imp_txt_res = ReportsDrawingsBimEngine::import_geometry_from_text(obra_imp_txt, sample_txt, "HEA 180", "S275");
    TEST_P13_ASSERT(imp_txt_res.exito && imp_txt_res.barras_importadas == 2);

    const auto ifc2x3 = ReportsDrawingsBimEngine::export_to_ifc(obra, VersionEsquemaIfc::IFC2x3_TC1, &found_model, &baseplates);
    const auto ifc4 = ReportsDrawingsBimEngine::export_to_ifc(obra, VersionEsquemaIfc::IFC4_ADD2_TC1, &found_model, &baseplates);
    const auto gltf = ReportsDrawingsBimEngine::export_to_gltf_json(obra, &found_model);
    const auto cis2 = ReportsDrawingsBimEngine::export_to_cis2_step(obra, &unions, &baseplates);
    const auto tekla = ReportsDrawingsBimEngine::export_to_tekla(obra, &unions, &baseplates);
    const auto tecnometal = ReportsDrawingsBimEngine::export_to_tecnometal(obra, &unions, &baseplates);

    TEST_P13_ASSERT(ifc2x3.find("FILE_SCHEMA(('IFC2X3'))") != std::string::npos);
    TEST_P13_ASSERT(ifc4.find("FILE_SCHEMA(('IFC4'))") != std::string::npos);
    TEST_P13_ASSERT(ifc4.find("IFCCOLUMN") != std::string::npos && ifc4.find("IFCFOOTING") != std::string::npos);
    TEST_P13_ASSERT(gltf.find("\"version\": \"2.0\"") != std::string::npos);
    TEST_P13_ASSERT(cis2.find("STRUCTURAL_FRAME_SCHEMA") != std::string::npos);
    TEST_P13_ASSERT(tekla.find("<TeklaStructuresModel") != std::string::npos);
    TEST_P13_ASSERT(tecnometal.find("ctmmodelo_append_pilar") != std::string::npos);

    // 5. Verificar comandos de exportación e informes en AppWindow
    gui::AppWindow app;
    app.obra_mut() = obra;
    TEST_P13_ASSERT(app.execute_command("Exportacion_A_Tekla"));
    TEST_P13_ASSERT(app.last_exported_payload().find("ctkmodelo_append_pilar") != std::string::npos);
    TEST_P13_ASSERT(app.execute_command("Exportacion_A_CIS2"));
    TEST_P13_ASSERT(app.last_exported_payload().find("STRUCTURAL_FRAME_SCHEMA") != std::string::npos);
    TEST_P13_ASSERT(app.execute_command("Exportacion_A_CSV"));
    TEST_P13_ASSERT(!app.last_exported_payload().empty());
    TEST_P13_ASSERT(app.execute_command("exportar_bim"));
    TEST_P13_ASSERT(app.last_exported_payload().find("IFC4") != std::string::npos);
    TEST_P13_ASSERT(app.execute_command("Ayuda_MemoriaCalculo"));
    TEST_P13_ASSERT(!app.last_report_document().capitulos.empty());
}
