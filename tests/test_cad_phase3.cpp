#include "cad/cad_engine.hpp"
#include "gui/app_window.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>

namespace {

void check_cad(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        std::cerr << "[FAIL] " << file << ":" << line << " -> " << expr << "\n";
        std::exit(1);
    }
}

#define CAD_ASSERT(expr) check_cad((expr), #expr, __FILE__, __LINE__)

void test_mdi_view_manager() {
    using namespace cypeclon::cad;
    using namespace cypeclon::model;

    MdiViewManager mgr;
    CAD_ASSERT(mgr.views().size() == 1);
    const std::uint32_t v3d_id = mgr.active_view_id();
    CAD_ASSERT(mgr.active_view() != nullptr);
    CAD_ASSERT(mgr.active_view()->type == MdiViewType::Vista3dGeneral);

    // No permitir cerrar la única ventana MDI activa
    CAD_ASSERT(!mgr.close_view(v3d_id));

    // Crear vistas 2D ortogonales (XY, XZ, YZ), plano por 3 puntos, pórtico y planta de nivel
    const auto v_xy = mgr.create_view_orthogonal_2d(MdiViewType::Plano2dXY, 3.5, "Planta Z=3.5");
    const auto v_xz = mgr.create_view_orthogonal_2d(MdiViewType::Plano2dXZ, 0.0, "Pórtico Frontal Y=0");
    const auto v_3p = mgr.create_view_inclined_3pts({0.0, 0.0, 0.0}, {6.0, 0.0, 0.0}, {0.0, 4.0, 2.0}, "Faldón Cubierta");
    const auto v_port = mgr.create_view_portico({0.0, 0.0, 0.0}, {8.0, 0.0, 0.0}, "Pórtico 1");

    NivelPlanta niv{1, "Forjado 1", 3.50, 3.50, 1, true};
    const auto v_niv = mgr.create_view_planta_nivel(niv);
    CAD_ASSERT(mgr.views().size() == 6);
    CAD_ASSERT(mgr.active_view_id() == v_niv);

    // Verificar pertenencia de puntos al plano de trabajo de la vista XY (Z = 3.5 m)
    const auto* win_xy = mgr.find_view(v_xy);
    CAD_ASSERT(win_xy != nullptr);
    CAD_ASSERT(win_xy->contains_point_in_plane({2.0, 5.0, 3.50}));
    CAD_ASSERT(!win_xy->contains_point_in_plane({2.0, 5.0, 0.00}));

    // Verificar plano inclinado por 3 puntos
    const auto* win_3p = mgr.find_view(v_3p);
    CAD_ASSERT(win_3p != nullptr);
    CAD_ASSERT(win_3p->contains_point_in_plane({3.0, 2.0, 1.0}));

    // Renombrado y navegación Anterior / Siguiente
    CAD_ASSERT(mgr.rename_view(v_port, "Pórtico Principal A"));
    CAD_ASSERT(mgr.find_view(v_port)->title == "Pórtico Principal A");

    CAD_ASSERT(mgr.activate_view(v3d_id));
    CAD_ASSERT(mgr.activate_next());
    CAD_ASSERT(mgr.active_view_id() == v_xy);
    CAD_ASSERT(mgr.activate_previous());
    CAD_ASSERT(mgr.active_view_id() == v3d_id);

    // Algoritmos de disposición de subventanas MDI dentro de cype_mdi_frame (390, 180, 1530x814)
    mgr.set_mdi_frame_rect({390, 180, 1530, 814});

    mgr.arrange_cascade();
    CAD_ASSERT(mgr.views()[0].rect.x == 390 && mgr.views()[0].rect.y == 180);
    CAD_ASSERT(mgr.views()[1].rect.x == 390 + 28 && mgr.views()[1].rect.y == 180 + 28);

    mgr.arrange_tile_horizontal();
    CAD_ASSERT(mgr.views().front().rect.width == 1530);
    CAD_ASSERT(mgr.views().front().rect.y == 180);
    CAD_ASSERT(mgr.views().back().rect.y + mgr.views().back().rect.height == 180 + 814);

    mgr.arrange_tile_vertical();
    CAD_ASSERT(mgr.views().front().rect.height == 814);
    CAD_ASSERT(mgr.views().front().rect.x == 390);
    CAD_ASSERT(mgr.views().back().rect.x + mgr.views().back().rect.width == 390 + 1530);

    mgr.arrange_order_grid();
    CAD_ASSERT(mgr.views()[0].rect.width > 0 && mgr.views()[0].rect.height > 0);

    // Cerrar subventana y verificar actualización de vista activa
    CAD_ASSERT(mgr.close_view(v_xz));
    CAD_ASSERT(mgr.views().size() == 5);
}

void test_camera3d_and_rays() {
    using namespace cypeclon::cad;
    using namespace cypeclon::core;

    Camera3d cam;
    cam.set_preset_view("XY");
    cam.set_target({5.0, 4.0, 0.0});
    cam.set_ortho_half_height(10.0);

    constexpr int vp_w = 1000;
    constexpr int vp_h = 800;

    // En vista XY centrada en (5, 4, 0), el centro de pantalla debe proyectarse a (500, 400)
    const Vec3d center_scr = cam.world_to_screen({5.0, 4.0, 0.0}, vp_w, vp_h);
    CAD_ASSERT(std::abs(center_scr.x - 500.0) < 1e-4);
    CAD_ASSERT(std::abs(center_scr.y - 400.0) < 1e-4);

    // Roundtrip world_to_screen <-> unproject
    const Vec3d test_pt{8.25, 1.75, 0.0};
    const Vec3d scr = cam.world_to_screen(test_pt, vp_w, vp_h);
    const Vec3d unproj = cam.unproject(scr.x, scr.y, scr.z, vp_w, vp_h);
    CAD_ASSERT((unproj - test_pt).norm() < 1e-5);

    // Intersección rayo-plano Z=0
    const Plane3d pl_xy = Plane3d::from_point_and_normal({0.0, 0.0, 0.0}, {0.0, 0.0, 1.0});
    const auto hit_plane = cam.screen_to_plane(scr.x, scr.y, vp_w, vp_h, pl_xy);
    CAD_ASSERT(hit_plane.has_value());
    CAD_ASSERT((*hit_plane - test_pt).norm() < 1e-4);

    // Probar órbita esférica, encuadre (pan), zoom rueda, zoom ventana y zoom extensión
    cam.set_preset_view("Isometrica");
    const Vec3d pos_before = cam.position();
    cam.orbit(0.35, 0.15);
    CAD_ASSERT((cam.position() - pos_before).norm() > 0.5);

    const double h_before = cam.ortho_half_height();
    cam.zoom_wheel(2.0);
    CAD_ASSERT(std::abs(cam.ortho_half_height() - 0.5 * h_before) < 1e-6);

    BBox3d scene_box{{0.0, 0.0, 0.0}, {12.0, 8.0, 6.0}};
    cam.zoom_extents(scene_box, vp_w, vp_h);
    CAD_ASSERT((cam.target() - Vec3d{6.0, 4.0, 3.0}).norm() < 1e-6);

    // Proyección en perspectiva cónica
    cam.set_projection_mode(ProjectionMode::Perspective);
    CAD_ASSERT(!cam.is_orthographic());
    const Ray3d persp_ray = cam.screen_to_ray(500.0, 400.0, vp_w, vp_h);
    CAD_ASSERT((persp_ray.origin - cam.position()).norm() < 1e-6);
    CAD_ASSERT(std::abs(persp_ray.direction.norm() - 1.0) < 1e-6);
}

void test_picker_snaps_and_dimensions() {
    using namespace cypeclon::cad;
    using namespace cypeclon::model;
    using namespace cypeclon::core;

    Obra obra;
    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({10.0, 0.0, 0.0}, VinculacionExterior::ArticuladoFijo);
    const auto n3 = obra.add_nudo({5.0, -5.0, 0.0}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({5.0,  5.0, 0.0}, VinculacionExterior::Libre);

    const auto b1 = obra.add_barra(n1, n2, "IPE 240", "S275"); // Segmento horizontal de (0,0,0) a (10,0,0)
    const auto b2 = obra.add_barra(n3, n4, "HEB 200", "S275"); // Segmento vertical de (5,-5,0) a (5,5,0) -> corta en (5,0,0)
    CAD_ASSERT(b1 != b2);

    Camera3d cam;
    cam.set_preset_view("XY");
    cam.set_target({5.0, 0.0, 0.0});
    cam.set_ortho_half_height(10.0);
    constexpr int vp_w = 1000;
    constexpr int vp_h = 1000;

    CadPicker picker;
    picker.rebuild_index(obra);

    // 1. Picking de nudo n1 en (0, 0, 0)
    const Vec3d s_n1 = cam.world_to_screen({0.0, 0.0, 0.0}, vp_w, vp_h);
    const auto pick_n1 = picker.pick_entity(obra, s_n1.x + 2.0, s_n1.y - 1.0, cam, vp_w, vp_h, 8.0);
    CAD_ASSERT(pick_n1.has_value());
    CAD_ASSERT(pick_n1->kind == EntityKind::Nudo && pick_n1->entity_id == n1);

    // 2. Picking de barra b1 cerca de (2.5, 0, 0) lejos de los nudos
    const Vec3d s_b1 = cam.world_to_screen({2.5, 0.0, 0.0}, vp_w, vp_h);
    const auto pick_b1 = picker.pick_entity(obra, s_b1.x, s_b1.y + 3.0, cam, vp_w, vp_h, 8.0);
    CAD_ASSERT(pick_b1.has_value());
    CAD_ASSERT(pick_b1->kind == EntityKind::Barra && pick_b1->entity_id == b1);

    // 3. Selección por caja: Ventana (izquierda a derecha) vs Captura (derecha a izquierda)
    // Rectángulo alrededor de x in [-1, 3], y in [-1, 1]: contiene n1 pero sólo parte de b1
    const Vec3d box_a = cam.world_to_screen({-1.0,  1.0, 0.0}, vp_w, vp_h);
    const Vec3d box_b = cam.world_to_screen({ 3.0, -1.0, 0.0}, vp_w, vp_h);

    // Modo Ventana (box_a.x < box_b.x): sólo selecciona n1 (b1 no está íntegramente dentro)
    const auto sel_win = picker.select_box(obra, box_a.x, box_a.y, box_b.x, box_b.y, cam, vp_w, vp_h, SelectionBoxMode::AutoByDragDirection);
    CAD_ASSERT(sel_win.size() == 1 && sel_win[0].kind == EntityKind::Nudo && sel_win[0].entity_id == n1);

    // Modo Captura (arrastre derecha -> izquierda: box_b.x a box_a.x): selecciona tanto n1 como b1
    const auto sel_cross = picker.select_box(obra, box_b.x, box_a.y, box_a.x, box_b.y, cam, vp_w, vp_h, SelectionBoxMode::AutoByDragDirection);
    CAD_ASSERT(sel_cross.size() == 2);

    // 4. Motor de Capturas (SnapEngine): Extremo, Intersección, Punto medio, Perpendicular, Más cercano, Polar, Rejilla y DXF
    SnapEngine snaps;
    const Plane3d work_plane = Plane3d::from_point_and_normal({0.0, 0.0, 0.0}, {0.0, 0.0, 1.0});

    // 4a. Extremo en (10, 0, 0)
    const Vec3d s_end = cam.world_to_screen({10.0, 0.0, 0.0}, vp_w, vp_h);
    const auto sr_end = snaps.find_snap(obra, s_end.x + 3.0, s_end.y, cam, vp_w, vp_h, work_plane);
    CAD_ASSERT(sr_end.hit && sr_end.type == SnapType::Extremo);
    CAD_ASSERT((sr_end.world_pos - Vec3d{10.0, 0.0, 0.0}).norm() < 1e-5);

    // 4b. Intersección entre b1 y b2 en (5, 0, 0)
    const Vec3d s_int = cam.world_to_screen({5.0, 0.0, 0.0}, vp_w, vp_h);
    const auto sr_int = snaps.find_snap(obra, s_int.x + 2.0, s_int.y - 2.0, cam, vp_w, vp_h, work_plane);
    CAD_ASSERT(sr_int.hit && sr_int.type == SnapType::Interseccion);
    CAD_ASSERT((sr_int.world_pos - Vec3d{5.0, 0.0, 0.0}).norm() < 1e-4);

    // 4c. Punto medio de una barra aislada (o desactivando intersección para probar punto medio de b1 en (5,0,0))
    snaps.config().snap_interseccion = false;
    const auto sr_mid = snaps.find_snap(obra, s_int.x + 1.0, s_int.y, cam, vp_w, vp_h, work_plane);
    CAD_ASSERT(sr_mid.hit && sr_mid.type == SnapType::PuntoMedio);
    CAD_ASSERT((sr_mid.world_pos - Vec3d{5.0, 0.0, 0.0}).norm() < 1e-4);
    snaps.config().snap_interseccion = true;

    // 4d. Perpendicular desde punto de referencia (3, 4, 0) hacia barra b1 -> pie en (3, 0, 0)
    const Vec3d s_perp = cam.world_to_screen({3.0, 0.0, 0.0}, vp_w, vp_h);
    const auto sr_perp = snaps.find_snap(obra, s_perp.x + 2.0, s_perp.y, cam, vp_w, vp_h, work_plane, Vec3d{3.0, 4.0, 0.0});
    CAD_ASSERT(sr_perp.hit && sr_perp.type == SnapType::Perpendicular);
    CAD_ASSERT((sr_perp.world_pos - Vec3d{3.0, 0.0, 0.0}).norm() < 1e-4);

    // 4e. Más cercano sobre b1 en (7.3, 0, 0)
    snaps.config().snap_mas_cercano = true;
    const Vec3d s_near = cam.world_to_screen({7.3, 0.0, 0.0}, vp_w, vp_h);
    const auto sr_near = snaps.find_snap(obra, s_near.x, s_near.y + 4.0, cam, vp_w, vp_h, work_plane);
    CAD_ASSERT(sr_near.hit && sr_near.type == SnapType::MasCercano);
    CAD_ASSERT(std::abs(sr_near.world_pos.x - 7.3) < 0.05 && std::abs(sr_near.world_pos.y) < 1e-5);
    snaps.config().snap_mas_cercano = false;

    // 4f. Plantilla DXF con transformación (offset + escala)
    DxfTemplate dxf;
    dxf.set_transformation({2.0, 3.0, 0.0}, 2.0, 0.0);
    dxf.add_point("EJES_DXF", {1.5, 1.0, 0.0}); // Punto transformado: (2 + 3 = 5, 3 + 2 = 5, 0) -> usemos (4, 4, 0) local -> (10, 11, 0)
    dxf.add_point("EJES_DXF", {-3.0, 2.0, 0.0}); // Transformado: (2 - 6 = -4, 3 + 4 = 7, 0)
    const Vec3d s_dxf = cam.world_to_screen({-4.0, 7.0, 0.0}, vp_w, vp_h);
    const auto sr_dxf = snaps.find_snap(obra, s_dxf.x + 1.0, s_dxf.y, cam, vp_w, vp_h, work_plane, std::nullopt, &dxf);
    CAD_ASSERT(sr_dxf.hit && sr_dxf.type == SnapType::PlantillaDxf);
    CAD_ASSERT((sr_dxf.world_pos - Vec3d{-4.0, 7.0, 0.0}).norm() < 1e-4);

    // 4g. Rastreo polar (45 grados desde origen (0,0,0) cerca de (4, 4, 0))
    snaps.config().snap_rastreo_polar = true;
    snaps.config().snap_rejilla = false;
    const Vec3d s_pol = cam.world_to_screen({4.0, 4.05, 0.0}, vp_w, vp_h);
    const auto sr_pol = snaps.find_snap(obra, s_pol.x, s_pol.y, cam, vp_w, vp_h, work_plane, Vec3d{0.0, 0.0, 0.0});
    CAD_ASSERT(sr_pol.hit && sr_pol.type == SnapType::RastreoPolar);
    CAD_ASSERT(std::abs(sr_pol.world_pos.x - sr_pol.world_pos.y) < 1e-4);
    snaps.config().snap_rastreo_polar = false;
    snaps.config().snap_rejilla = true;

    // 4h. Rejilla en (-2.0, -3.0, 0.0)
    const Vec3d s_grd = cam.world_to_screen({-2.02, -2.98, 0.0}, vp_w, vp_h);
    const auto sr_grd = snaps.find_snap(obra, s_grd.x, s_grd.y, cam, vp_w, vp_h, work_plane);
    CAD_ASSERT(sr_grd.hit && sr_grd.type == SnapType::Rejilla);
    CAD_ASSERT((sr_grd.world_pos - Vec3d{-2.0, -3.0, 0.0}).norm() < 1e-4);

    // 5. Sistema de Acotación 3D (`DimensionSystem` / `cyplanobj`)
    DimensionSystem dims;
    const auto dim_id = dims.add_dimension({0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}, 1.20, {0.0, 0.0, 1.0}, cypeclon::units::UnitSystem::SI);
    const auto* d = dims.find_dimension(dim_id);
    CAD_ASSERT(d != nullptr);
    CAD_ASSERT(std::abs(d->measured_length_m - 10.0) < 1e-9);
    CAD_ASSERT(d->formatted_text == "10.00 m");
    CAD_ASSERT((d->dim_line_start - Vec3d{0.0, 1.20, 0.0}).norm() < 1e-6);
    CAD_ASSERT((d->dim_line_end - Vec3d{10.0, 1.20, 0.0}).norm() < 1e-6);

    dims.hide_all();
    CAD_ASSERT(!dims.global_visibility() && !dims.find_dimension(dim_id)->visible);
    dims.show_all();
    CAD_ASSERT(dims.global_visibility() && dims.find_dimension(dim_id)->visible);
    CAD_ASSERT(dims.remove_dimension(dim_id));
    CAD_ASSERT(dims.dimensions().empty());
}

void test_layers_profiles_extrusion_and_hlr() {
    using namespace cypeclon::cad;
    using namespace cypeclon::model;
    using namespace cypeclon::core;

    // 1. Verificar generación paramétrica de secciones 2D (`CrossSectionProfile2d`)
    const auto p_ipe = CrossSectionProfile2d::from_profile_name("IPE 200");
    CAD_ASSERT(p_ipe.kind == ProfileShapeKind::DoubleT && p_ipe.outer_contour.size() == 12);
    CAD_ASSERT(std::abs(p_ipe.height_z_m - 0.200) < 1e-6);

    const auto p_heb = CrossSectionProfile2d::from_profile_name("HEB 300");
    CAD_ASSERT(p_heb.kind == ProfileShapeKind::DoubleT && std::abs(p_heb.width_y_m - 0.300) < 1e-6);

    const auto p_upn = CrossSectionProfile2d::from_profile_name("UPN 160");
    CAD_ASSERT(p_upn.kind == ProfileShapeKind::ChannelUPN && p_upn.outer_contour.size() == 8);

    const auto p_ang = CrossSectionProfile2d::from_profile_name("L 80x80x8");
    CAD_ASSERT(p_ang.kind == ProfileShapeKind::AngleL && p_ang.outer_contour.size() == 6);

    const auto p_shs = CrossSectionProfile2d::from_profile_name("SHS 100x100x5");
    CAD_ASSERT(p_shs.kind == ProfileShapeKind::HollowRect && p_shs.inner_hole_contour.size() == 4);

    const auto p_chs = CrossSectionProfile2d::from_profile_name("CHS 114.3x4");
    CAD_ASSERT(p_chs.kind == ProfileShapeKind::HollowCirc && p_chs.outer_contour.size() == 12);

    const auto p_rect = CrossSectionProfile2d::from_profile_name("RECT 30x50");
    CAD_ASSERT(p_rect.kind == ProfileShapeKind::SolidRect && std::abs(p_rect.height_z_m - 0.50) < 1e-6);

    // 2. Modelo con múltiples capas y categorías estructurales (`Pilares`, `Vigas`, `Tirantes`, `Laminas`, `Panyos`)
    Obra obra;
    const auto capa_pilares = obra.add_etiqueta_capa("Pilares", 0xFF0000, true, false);
    const auto capa_oculta  = obra.add_etiqueta_capa("Auxiliar Oculta", 0x888888, false, false);

    const auto n1 = obra.add_nudo({0.0, 0.0, 0.0}, VinculacionExterior::Empotrado);
    const auto n2 = obra.add_nudo({0.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n3 = obra.add_nudo({6.0, 0.0, 4.0}, VinculacionExterior::Libre);
    const auto n4 = obra.add_nudo({6.0, 0.0, 0.0}, VinculacionExterior::ArticuladoFijo);

    const auto b_pilar = obra.add_barra(n1, n2, "HEB 300", "S275");
    obra.find_barra_mut(b_pilar)->tipo_estructural = TipoBarraEstructural::ColumnaPilar;
    obra.find_barra_mut(b_pilar)->capa_id = capa_pilares;
    obra.find_barra_mut(b_pilar)->desp_y_m = 0.05; // Enrase excéntrico

    const auto b_viga = obra.add_barra(n2, n3, "IPE 300", "S275");
    obra.find_barra_mut(b_viga)->tipo_estructural = TipoBarraEstructural::Viga;

    const auto b_tirante = obra.add_barra(n1, n3, "L 60x60x6", "S275");
    obra.find_barra_mut(b_tirante)->es_tirante = true;
    obra.find_barra_mut(b_tirante)->tipo_estructural = TipoBarraEstructural::Tirante;

    const auto b_oculta = obra.add_barra(n2, n4, "UPN 140", "S275");
    obra.find_barra_mut(b_oculta)->capa_id = capa_oculta;

    obra.add_lamina({{0.0, 0.0, 0.0}, {6.0, 0.0, 0.0}, {6.0, 4.0, 0.0}, {0.0, 4.0, 0.0}}, 0.20, "C25/30");
    obra.add_panyo({{0.0, 0.0, 4.0}, {6.0, 0.0, 4.0}, {6.0, 4.0, 4.0}, {0.0, 4.0, 4.0}});

    Camera3d cam;
    cam.set_preset_view("Isometrica");
    cam.zoom_extents(obra.bounding_box(), 1200, 800);

    SceneTessellator tess;
    CadCategoryFilter cat_filter{};

    // La barra en `capa_oculta` debe quedar filtrada automáticamente; las otras 3 son visibles
    const auto scene_full = tess.build_scene(
        obra, cam, 1200, 800,
        RenderRepresentationMode::PerfilRealExtruido3d,
        cat_filter, true, true
    );
    CAD_ASSERT(scene_full.visible_barra_count == 3);
    CAD_ASSERT(scene_full.extruded_bar_meshes.size() == 3);
    CAD_ASSERT(scene_full.local_axes_segments.size() == 3 * 3);
    CAD_ASSERT(!scene_full.support_symbol_segments.empty());
    CAD_ASSERT(scene_full.visible_lamina_count == 1 && scene_full.surface_triangles.size() == 2);
    CAD_ASSERT(scene_full.visible_panyo_count == 1);

    // Desactivar categoría `Tirantes` y `Pilares` -> sólo queda visible la viga (`b_viga`)
    cat_filter.tirantes_visible = false;
    cat_filter.pilares_visible = false;
    const auto scene_viga_only = tess.build_scene(
        obra, cam, 1200, 800,
        RenderRepresentationMode::AlambricoUnifilar,
        cat_filter
    );
    CAD_ASSERT(scene_viga_only.visible_barra_count == 1);

    // 3. Prueba de Línea Oculta (`cyhlr` / `HiddenLineRemover`):
    // Dos barras horizontales cruzadas a distinta cota Z vistas desde +Z (planta XY):
    // Barra superior en Z = +2.0 (ancha, "HEB 600") oculta el tramo central de la barra inferior en Z = -2.0
    Obra obra_hlr;
    const auto h1 = obra_hlr.add_nudo({-2.0,  0.0,  2.0});
    const auto h2 = obra_hlr.add_nudo({ 2.0,  0.0,  2.0});
    const auto h3 = obra_hlr.add_nudo({ 0.0, -0.05, -2.0});
    const auto h4 = obra_hlr.add_nudo({ 0.0,  0.05, -2.0});
    obra_hlr.add_barra(h1, h2, "HEB 600", "S275"); // Barra frontal ancha en Z = +2.0
    obra_hlr.add_barra(h3, h4, "IPE 80",  "S275"); // Barra pequeña justo debajo en Z = -2.0

    Camera3d cam_top;
    cam_top.set_preset_view("XY");
    cam_top.set_target({0.0, 0.0, 0.0});
    cam_top.set_ortho_half_height(4.0);

    const auto scene_hlr = tess.build_scene(
        obra_hlr, cam_top, 800, 800,
        RenderRepresentationMode::LineaOcultaHlr
    );
    CAD_ASSERT(!scene_hlr.hlr_output.visible_segments.empty());
    CAD_ASSERT(!scene_hlr.hlr_output.occluded_segments.empty());

    // Verificar plano de recorte (`cyhlrclipping`)
    tess.hlr().add_clipping_plane(Plane3d::from_point_and_normal({0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}));
    CAD_ASSERT(tess.hlr().is_point_visible_by_clipping({0.0, 0.0, 1.0}));
    CAD_ASSERT(!tess.hlr().is_point_visible_by_clipping({0.0, 0.0, -1.0}));

    // 4. Integración con AppWindow (comandos de pestaña Ventana y Planos/Niveles)
    cypeclon::gui::AppWindow app;
    CAD_ASSERT(app.mdi_manager().views().size() == 1);
    CAD_ASSERT(app.execute_command("Ventana_AbrirNueva"));
    CAD_ASSERT(app.mdi_manager().views().size() == 2);
    CAD_ASSERT(app.execute_command("Ventana_MosaicoVertical"));
    CAD_ASSERT(app.execute_command("Obra_niveles"));
    CAD_ASSERT(app.obra().estructura().niveles.size() == 1);
    CAD_ASSERT(app.mdi_manager().views().size() == 3);
}

} // namespace

void run_cad_phase3_tests() {
    test_mdi_view_manager();
    test_camera3d_and_rays();
    test_picker_snaps_and_dimensions();
    test_layers_profiles_extrusion_and_hlr();
}

