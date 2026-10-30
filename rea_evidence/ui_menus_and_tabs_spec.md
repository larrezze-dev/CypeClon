# Especificación Técnica de Interfaz de Usuario, Menús, Barras de Herramientas, Docking y Diálogos de CYPE 3D (Versión 2027)

## 1. Metadatos de Evidencia y Binarios Analizados

| Binario | Ruta Completa | Tamaño (Bytes) | SHA-256 | Herramientas / Evidencia REA |
| :--- | :--- | ---: | :--- | :--- |
| `CYPE 3D.exe` | `D:\CYPE Ingenieros\Versión 2027\CYPE 3D\CYPE 3D.exe` | 91,064 | `cf4c9b2ab68bc5519b471a519eb168115f0047a73e09ab9bc1fb1428ef689894` | Ejecución en vivo + Win32 UI Inspection (`PrintWindow` / `EnumChildWindows`) |
| `ed3dlib.dll` | `D:\CYPE Ingenieros\Versión 2027\CYPE 3D\ed3dlib.dll` | 29,771,960 | `ef10cf813a8a4e71ccca06234172889a603f6f1d31e10e36186ffd1afd30dead` | `inspect_native_exports`, `inspect_pe_imports`, `inspect_pe_resources` (`ev_4fc67a60ef1e5aae8fdef32f07bbbb6b950fc2fe8a1b8763f81c3c68e2246337`), `inspect_with_objdump` (`ev_7a9a4ae31aa88744bb3039b949eba0a7c43a3569d8a64573529cd774a47211b4`) |
| `planocim.dll` | `D:\CYPE Ingenieros\Versión 2027\CYPE 3D\planocim.dll` | 3,751,096 | `f1c9ce969b7b4fa16fa8b30989fed498c04d03d7507337c7a64083d825baf4e8` | `inspect_native_exports` (`ev_87ebe5de209b4f6ad1194ecdd52d580585f79e857947ac5f1d51d46333581b93`), `inspect_pe_imports`, `inspect_pe_resources` (`ev_abcf7a3512df9370751745585e3411b704eb5a2f17b06fdefc25d3c389b48b81`), Ghidra 12.1.4 Decompilation (`ev_a320baca7517d7a751d387d9e6c9fb4d8aff9ffa4db8f68e2511bf91463090f0`, `ev_57ca8b855768ca915b3bf62ff2461e5690993243a7dc1fe1f7e5adbaadcea2ba`, `ev_ab73da780add09f6449873a29c279132c20c8743f4aab9e59d6355d6472cf37b`, `ev_e7fcc495a83fb604f33d6c6e7cda6fe0b07319bfe8402553bf81610106b80233`) |

---

## 2. Arquitectura General de Ventanas y Clases Win32 (`panelwin.dll` / `entomenu.dll` / `entodock.dll`)

La inspección en vivo mediante la API Win32 (`EnumWindows`, `EnumChildWindows`, `GetClassName`, `GetWindowRect`) sobre el proceso `CYPE 3D.exe` y la decompilación de las DLLs base demuestran que la interfaz gráfica utiliza clases de ventana nativas registradas por `panelwin.dll`, `entomenu.dll`, `entodock.dll` y `entograf.dll`:

| Clase Win32 | Rol en la Interfaz | Dimensiones Típicas (1920×1080) | Descripción Técnica |
| :--- | :--- | :--- | :--- |
| `CypeAplicacion` | Ventana principal de nivel superior | `0, 0, 1920×1032` | Marco contenedor principal de la aplicación (`CYPE 3D v2027.a - <obra>.ed3`). Contiene la barra de título personalizada, la barra de acceso rápido (QAT), la cinta de opciones (Ribbon), el contenedor de solapas y la barra de estado. |
| `CypeDialogo` | Diálogos modales y asistentes | Variable (`537×285`, `682×457`, `848×774`) | Ventanas emergentes con barra de título roja personalizada (`RGB(168, 32, 26)`), área cliente gris clara y pie de botones (`&Aceptar`, `Cancelar`, `< &Anterior`, `&Siguiente >`, `&Terminar`). |
| `CypeMiniSolapas` | Contenedor de solapas inferiores | `0, 50, 1920×964` | Gestiona las solapas principales de trabajo registradas mediante `ebasolap_nousar_crea`: **Estructura** (`"METAL3D_ESTRUCTURA"`) y **Cimentación** (`"ID_CIMENTACION"`), dibujando el selector de pestañas en la franja inferior (`y = 994..1012`). |
| `CypeMdiFrame` | Contenedor MDI de vistas 2D/3D | `390, 180, 1530×814` | Aloja las ventanas interiores de trabajo (`3D`, vistas 2D de planos, pórticos) dentro del panel derecho del `pndock`. |
| `CypeDialogoInterior` | Marco de vista interior activa | `390, 180, 1530×814` | Encapsula la barra de título de la vista activa (`3D`), su barra vertical izquierda de herramientas de cámara/proyección (`24×24` px por icono) y el lienzo gráfico. |
| `CypeAreaDibujo` | Lienzo CAD 3D / 2D OpenGL | `414, 200, 1506×794` (Estructura)<br>`0, 142, 1920×852` (Cimentación) | Área de renderizado vectorial/OpenGL gestionada por `entograf.dll` y `earpanel_encapsula_panel_trabajo_planos`. |
| `ToolbarWindow32` | Botones de Ribbon, QAT y barras de herramientas | Variable | Cada pestaña del Ribbon (`y = 26, h = 24`), cada botón de comando dentro de la banda del Ribbon (`y = 51..143`) y cada icono de barra de herramientas rápida (`24×24`) se instancia como un control `ToolbarWindow32` individual. |

### 2.1. Sistema de Carga de Iconos y Recursos (`componen.dll` / `cypemsgs.dll`)
Tanto `ed3dlib.dll` como `planocim.dll` carecen de recursos de icono embebidos en su tabla PE `.rsrc` (`resource_count = 0` en `planocim.dll`, y solo `RT_DIALOG` / `RT_RCDATA` / `RT_MANIFEST` en `ed3dlib.dll`). En su lugar:
- Todos los iconos se cargan dinámicamente por clave de texto ASCII mediante:
  ```c
  imagen_t *imagen_nousar_carga_recurso(
      static_cache_slot_t *cache_slot,
      const char *subcarpeta_o_null,
      const char *source_file_tag, // p.ej. "ed3mnuob.c (ED3DLIB.DLL)" o "plcpanel.c (PLANOCIM.DLL)"
      const char *icon_resource_name // p.ej. "datobra2", "nudo_new2", "nuevocim"
  );
  ```
- Todos los textos visibles de menús, botones y tooltips se resuelven mediante tablas de mensajes `.msg` (`"cimentacion.msg"`, etc.) invocando `cypemsgs.dll!nousar_msglocal_original_u` o `nousar_msglocal_trad_fich_u`.

---

## 3. Flujo Inicial de Arranque, Asistente de Nueva Obra y Datos Generales

### 3.1. Pantalla de Bienvenida / Sin Obra Abierta (`01_startup_welcome_screen.png`)
Al iniciar `CYPE 3D.exe` sin obra abierta, `CypeAplicacion` (`1920×1032`) muestra el panel central de inicio dividido en tres tarjetas sobre fondo gris oscuro (`#737373`):
1. **Columna `Archivo` (`x = 560..699, y = 402..685`):**
   - `Nuevo...` (`ToolbarWindow32` en `562, 449, 135×44`)
   - `Gestión archivos` (`ToolbarWindow32` en `562, 501, 135×44`)
   - `Ejemplos` (`ToolbarWindow32` en `562, 553, 135×44`)
2. **Columna `Últimos ficheros` (`x = 721..1125, y = 402..685`):**
   - Lista de hasta 6 obras recientes (`1 D:\CYPE Ingenieros\Proyectos\CYPE 3D\...ed3`, etc., cada una en un `ToolbarWindow32` de `400×27`) y botón inferior `Más...` (`902, 672, 43×27`).
3. **Columna `Ayuda` (`x = 1147..1360, y = 402..685`):**
   - Enlaces a `Documentación del programa`, `CYPE 3D - Memoria de Cálculo`, `CYPE 3D - Ejemplo`, `Ir a la FAQ de CYPE`, `Novedades`, `Contrato de Licencia de Programas`, `Cláusula de Responsabilidades`, `Soporte técnico`, `Comunidad de usuarios`, `Recursos de aprendizaje`, `Acerca de ...`, y botón inferior `Gestión de licencias` (`1231, 716, 136×28`).
4. **Barra de estado inferior (`y = 1014, h = 18`):**
   - Muestra el mensaje `"Para utilizar el programa, debe tener una obra abierta. Puede crear una obra nueva o abrir una existente."`

### 3.2. Diálogo `Nueva obra` — Clave y Descripción (`02_composite_nueva_obra.png`)
Al pulsar `Nuevo...`, se abre el diálogo modal `Nueva obra` (`CypeDialogo`, `696, 378, 537×285`):
- **Grupo `Nombre de la obra` (`717, 425, 495×105`):**
  - Ruta actual (`Static`: `D:\CYPE Ingenieros\Proyectos\CYPE 3D\`) y botón `Examinar` (`1125, 451, 72×27`).
  - Etiqueta `Nombre del fichero`, campo `Edit` (`841, 492, 328×23`) y sufijo `.ed3`.
- **Grupo `Descripción` (`717, 538, 495×64`):**
  - Campo `Edit` (`732, 564, 465×23`).
- **Pie del diálogo:** Botones `&Aceptar` (`707, 625, 75×27`) y `Cancelar` (`1152, 625, 70×27`). Si el fichero `.ed3` ya existe, muestra el cuadro `Pregunta` (`346×162`): `"El fichero ya existe. ¿Desea sobreescribirlo?"` con botones `&Sí` y `No`.

### 3.3. Asistente `Nueva obra` — Modo de Creación (`03_wizard_nueva_obra.png`)
Tras confirmar el nombre del fichero `.ed3`, se abre el segundo diálogo `Nueva obra` (`CypeDialogo`, `624, 292, 682×457`):
- **Radio buttons de selección:**
  - `Obra vacía` (`640, 334, 80×16`, seleccionado por defecto).
  - `Introducción automática IFC` (`640, 356, 178×16`, para importación de modelos BIM/IFC de BIMserver.center).
- **Vista previa gráfica (`CypeAreaDibujo`, `629, 384, 672×320`):** Ilustración vectorial 3D de una nave/estructura metálica.
- **Pie del diálogo:** Botones `&Aceptar` (`635, 711, 75×27`) y `Cancelar` (`1225, 711, 70×27`).

### 3.4. Diálogo Asistente `Obra vacía` / `Datos generales` (`04_after_wizard_aceptar.png` y `14_dialog_datos_generales.png`)
Tanto el paso final del asistente (`Obra vacía`, con botones `Cancelar`, `< Anterior`, `Siguiente >`, `Terminar`) como el comando `Obra -> Datos generales` (`Datos generales`, con botón `Aceptar`) presentan el mismo panel maestro de configuración (`848×774`):
- **Barra superior `Normas:`** Botón selector de normativas activas (`603, 176, 770×27`), mostrando p.ej. `"Eurocódigo 2, CTE DB SE-A, Código Estructural, CTE DB SE-M y Eurocódigo 9"`.
- **Columna izquierda (`x = 557..888`):**
  - **Sección `Perfiles`:**
    - `Acero laminado`: `ComboBox` (`S275 (UNE-EN 10025-2)`) + botón biblioteca (`20×20`).
    - `Acero conformado`: `ComboBox` (`S235`).
    - `Madera`: `Button` (`Aserrada, procedente de coníferas o ...`).
    - `Aluminio`: `Button` (`EN AW-5083 - F`).
    - `Hormigón`: `ComboBox` (`C25/30`).
  - **Sección `Acciones`:**
    - Checkbox `Con sismo dinámico`.
    - Botones de subdiálogo (`331×27` cada uno):
      1. `Resistencia al fuego`
      2. `Estados límite (combinaciones)` (abre el diálogo `Estados límite` `358×438` capturado en `05_dialog_step5.png`)
      3. `Hipótesis adicionales`
      4. `Cimentación`
      5. `Proceso constructivo`
  - **Sección `Opciones`:**
    - 4 botones en cuadrícula 2×2: `Pilares` (`146×27`), `Cimentación` (`179×27`), `Vigas` (`146×27`), `Uniones` (`179×27`).
  - **Sección `Ambiente`:**
    - `Vigas`: botón `X0` (`167×27`).
- **Columna derecha (`x = 900..1373`):**
  - **Sección `Hormigón armado`:**
    - `Hormigón para pilares`: `ComboBox` (`C25/30`).
    - `Hormigón para vigas de forjado`: `ComboBox` (`C25/30`).
    - `Hormigón para elementos de cimentación`: `ComboBox` (`C25/30`).
    - `Acero de barras`: `Button` (`S-400`).
    - `Características del árido`: `Button` (`Cuarcita (15 mm), 30 mm`).
    - Botones inferiores: `Recubrimientos` (`266×27`) y `Mermas de acero` (`201×27`).
  - **Sección `Terreno de cimentación`:**
    - Checkbox `Verificar deslizamiento de zapatas`.
    - `Tensiones admisibles del terreno`:
      - `Situaciones persistentes (A2 - Conjunto C)`: `Edit` (`0.200`) `MPa` + botón asistente (`20×20`).
      - `Situaciones sísmicas y accidentales (A2 - Conjunto C)`: `Edit` (`0.300`) `MPa`.
    - Control `SysTabControl32` (`479×247`) con dos pestañas: `Pilotes (Hormigón armado)` y `Micropilotes`:
      - `Excentricidad para el momento mínimo`: `Edit` (`0.10`) `m`.
      - Checkbox `Considerar esfuerzos con un axil reducido`.
      - `Pilotes que trabajan por fuste`: `Edit` (`0.00`) `· Axil`.
      - `Pilotes que trabajan solo por punta`: `Edit` (`0.80`) `· Axil`.
      - `Coeficiente de pandeo`: `Edit` (`1.00`).
      - Checkbox `Análisis de la estabilidad lateral del pilote`.
      - Botón `Configuración avanzada` (`455×27`).

---

## 4. Solapa 1: **Estructura** (`ed3dlib.dll` — `ed3base_crea_solapa`)

### 4.1. Inicialización, Tipos de Datos y Layout de Docking (`ed3base.c`)

La función exportada [`ed3base_crea_solapa`](file:///D:/CYPE%20Ingenieros/Versión%202027/CYPE%203D/ed3dlib.dll) (`0x1800f2190`) delega en el constructor interno `0x1800f32a0`, el cual realiza el registro completo de la solapa **Estructura**:

1. **Creación de la Solapa (`ebasolap_nousar_crea` en `0x1800f36b9`):**
   - **ID interno de la solapa:** `"METAL3D_ESTRUCTURA"` (`0x180a3e918`).
   - **Tipos de datos de estado registrados:**
     - Modelo de obra: `"ed3datos_t"` (`0x180a3e678`).
     - Estado de vista activa: `"ed3datvi_t"` (`0x180a3e6a0`).
   - **Receptor de estados:** `0x180589040` -> `ebarcbas_nousar_crea_receptor_estados(..., "ed3datos_t", "ed3datvi_t")`.
   - **Receptor de acción de obra:** `0x1806e9d60` -> `ebarcbas_nousar_crea_receptor_accion_obra(..., "ed3datos_t", "ed3datvi_t")`.
   - **Receptor de información de objeto (tooltips/inspección):** `0x1800f2de5` -> `ebarcbas_nousar_crea_receptor_info_objeto(..., "ed3datos_t", "ed3datvi_t")`.
   - **Configuración de barra de capturas CAD:** `0x1800f2fb5` -> `ebarcbca_crea_configuracion_barra_capturas(...)`.

2. **Construcción del Panel de Docking (`0x1800f2e10`, registrado vía `ebasolap_set_callback_panel_solapa_dock`):**
   - Crea un contenedor raíz horizontal mediante `pndock_crea("ed3base_dock", 1)` (`1` = split horizontal izquierda/derecha).
   - **Sub-dock izquierdo vertical (`pndock_crea(L"", 0)` en `0x1800f2e59`, ancho `382 px` en pantalla):**
     - **Panel superior (`gestor + 0x7a`):** `"ed3base_pncapas_visualizacion"` (título en UI: **`Etiquetas`** y **`Categorías`**), añadido con `pndock_append_panel_con_barra_titulo(..., 1, 0, 1)`. Contiene:
       - Barra de herramientas de capas (`+`, `X`, subir, bajar, gestionar etiquetas).
       - `SysListView32` de `Etiquetas` (`Referencia`, `Activa`, `Color`, iconos de cubo 3D, ojo de visibilidad y candado de bloqueo).
       - `SysListView32` de `Categorías` (`Nudos`, `Barras`, `Tirantes`, `Pilares`, `Vigas`, `Paños`, `Láminas` con ojo de visibilidad y candado).
       - Checkbox `Representar la estructura usando los colores de las capas`.
       - `ComboBox` `Elementos considerados en el análisis` (`Tensión / Deformación`).
     - **Panel inferior (`gestor + 0x82`):** `"ed3base_pncapas_preferencias_dibujo"` (título en UI: **`Opciones de visualización`**), añadido con `pndock_append_panel_con_barra_titulo(..., 0, 1, 1)`. Contiene:
       - Checkboxes con icono a la izquierda:
         - `Mostrar la información de los elementos`
         - `Mostrar textos sobre los elementos`
         - `Mostrar los ejes locales de los elementos`
         - `Dibujar las láminas con relleno`
         - `Dibujar las cargas con relleno`
       - Campos numéricos/combos:
         - `Dimensiones de los nudos`: `Edit` (`15.0`) `cm`.
         - `Tamaño de los textos`: `Edit` (`100.0`) `%`.
         - `Posición de los tooltips`: `ComboBox` (`En la esquina superior dere...`).
   - **Panel central/derecho fijo (`"ed3base_area_dibujo"` en `0x1800f2ec9`, `1530×814 px` en pantalla):**
     - Encapsulado mediante `earpanel_encapsula_panel_trabajo_planos` y añadido con `pndock_append_panel_fijo`.
     - Titulado **`Área de trabajo`**, aloja las ventanas MDI (`3D`, etc.) y su barra vertical izquierda de 10 iconos de visualización de perfiles/modelo (`Unifilar`, `Perfil real extruido`, `Con/sin paños`, `Vista 3D real`, etc.).

---

### 4.2. Mapa Completo de Pestañas del Ribbon y Comandos de **Estructura** (`0x1800f3120`)

La función `0x1800f3120` construye los menús principales de la solapa **Estructura** y los registra en el contenedor Ribbon (`ebamenu_set_aspecto_menu_ribbon`). Además, el marco general de `entomenu.dll` añade la pestaña estándar **`Ventana`** y las barras superiores de acceso rápido y de visualización/capturas CAD.

#### Pestaña 1: **`Obra`** (`0x18087eaf0` en `ed3mnuobrb.c` — Captura `06_estructura_tab_obra.png`)

| Grupo del Ribbon | Función Constructora | ID del Submenú / Botón (`entomenu.dll`) | Recurso de Icono (`componen.dll`) | Etiqueta Visible en UI | Tipo de Botón / Callback |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`Obra`** | `0x1803db6b0` (`"obra"`) | `"Obra_DatosGenerales"` | `"datobra2"` | **Datos generales** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Obra`** | `0x1803db6b0` | `"Gestion_biblioteca_perfiles"` | `"biblioteca_obra2"` | **Biblioteca de perfiles** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Obra`** | `0x1803db6b0` | `"biblioteca_catalogos"` | *(catálogo)* | **Catálogos de fabricante** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Obra`** | `0x1803db6b0` | `"Calculo_BibRotulasPlasticas"` | `"bibrotplas"` | **Biblioteca de rótulas plásticas** | `ebaboton_nousar_crea_boton_panel_obra_u` |
| **`Obra`** | `0x1803db6b0` | `"Calculo_CfgRendANL"` | `"cfgrend"` | **Rendimiento** | `ebaboton_nousar_crea_boton_panel_obra_u` |
| **`Obra`** | `0x1803db6b0` | `"Obra_Unidades_Acotacion"` | `"uacot"` | **Unidades de acotación** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| *(Alternativo sin niveles)* | `0x1803dbce0` | `"Obra_perfiles_metalicos"` / `"Obra_perfiles_madera"` | `"perf_met2"`, `"perf_mad2"` | Perfiles metálicos / Perfiles de madera | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Numeración`** | `0x1803db250` (`"numeracion"`) | `"Obra_opcs_numeracion"` | `"numel"` | **Editar** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Numeración`** | `0x1803db250` | `"Obra_reiniciar_numeracion"` | `"numelrec"` | **Reiniciar** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Vista 3D`** | `0x1803dcd90` (`"Vistas3D"`, icono `"vista3d"`) | `"Obra_Vista3D"` | `"v3dcomp"` | **Estructura completa** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Vista 3D`** | `0x1803dcd90` | `"Obra_Vista3D_vista"` | `"v3dvntact"` | **Ventana activa** | `ebaboton_nousar_crea_boton_dlg_obra_vista_con_1_extra_u` |
| **`Vista 3D`** | `0x1803dcd90` | `"Obra_Vista3DSeleccionPiezas"` | `"v3dpzas"` | **Selección de elementos** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180552390`) |
| **`Vista 3D`** | `0x1803dcd90` | `"Obra_UnionesEnVista3D"` | `"unv3d"` | **Uniones** (Interruptor) | `ebaboton_nousar_crea_boton_interruptor_u` |
| **`Listados`** | `0x1803db430` (`"listados"`) | `"Obra_ListadoElementosSeleccionados"` | `"p"` | **Elementos seleccionados** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1805619f0`) |
| **`Listados`** | `0x1803db430` | `"Obra_ExportacionCSVElementosSeleccionados"` | `"expcsv"` | **CSV de los elementos...** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180561270`) |
| **`Listados`** | `0x1803db430` -> `0x18074e910` | *(Etiquetas de vigas)* | *(etiquetas)* | **Etiquetas de vigas** | Subfunción `0x18074e910` |
| *(Extensor derecha)* | `ebamenu_append_extensor` | — | — | Alínea el bloque BIM a la derecha | — |
| **`BIMserver.center`** | `0x1803dad60` (`"gestion_bim"`, icono `"bim_server"`) | `"vincular_bim"` | `ifcbase_icono_ifc_vincular` | **Vincular** | `ebaboton_nousar_crea_boton_dlg_obra_vista_con_nombre_y_descripcion_1extra_u` |
| **`BIMserver.center`** | `0x1803dad60` | `"desvincular_BIM"` | `ifcbase_icono_ifc_desvincular` | **Desvincular** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`BIMserver.center`** | `0x1803dad60` | `"actualizar_bim"` | `mbimgsfich_icono_actualizar` | **Consultar** / Actualizar | `ebaboton_nousar_crea_boton_dlg_obra_vista_con_nombre_y_descripcion_1extra_u` |
| **`BIMserver.center`** | `0x1803dad60` | `"exportar_bim"` | `mbimgsfich_icono_exportar` | **Compartir** / Exportar | `ebaboton_nousar_crea_boton_dlg_obra_vista_con_nombre_y_descripcion_1extra_u` |

---

#### Pestaña 2: **`Geometría`** (`0x18087ea00` en `ed3mnugeo.c` — Captura `07_estructura_tab_geometria.png`)

| Grupo del Ribbon | Función Constructora | ID del Submenú / Botón (`entomenu.dll`) | Recurso de Icono (`componen.dll`) | Etiqueta Visible en UI | Tipo de Botón / Callback |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`Planos`** | `0x1803de3d0` (`"plano"`, icono `"rejilla"`) | `"Planos_Rejilla"` | `"rejilla"` | **Rejilla** (dentro de desplegable **Planos**) | `ebaboton_nousar_crea_boton_dlg_obra_u` (`0x1806132a0`) |
| **`Planos`** | `0x1803de3d0` | `"Obra_niveles"` | `"niveles"` | **Niveles** | `ebaboton_nousar_crea_boton_dlg_obra_u` (`0x180612db0`) |
| **`Planos`** | `0x1803de3d0` -> `0x180410a40` (`"acotacion"`, icono `"k"`, modeless) | `"Planos_Cotas_Anadir"`, `"Planos_Cotas_Borrar"`, `"Planos_Cotas_Mostrar"`, `"Planos_Cotas_Ocultar"` | `"k"`, `"l"`, `"m"`, `"n"` | **Acotación** (Añadir, Borrar, Mostrar, Ocultar) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1805755c0..620`) |
| **`Planos`** | `0x1803de3d0` | `"Planos_Referencias"` | `"0"` | **Referencias** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Planos`** | `0x1803de3d0` | `"Planos_MostrarOcultar"` | `"s"` | **Mostrar / Ocultar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057dbc0`) |
| **`Planos`** | `0x1803de3d0` | `"Planos_Visibilidad"` | `"t"` | **Visibilidad** | `ebaboton_nousar_crea_boton_interruptor_u` |
| **`Planos`** | `0x1803de3d0` | `"Planos_Agrupar"` / `"Planos_Desagrupar"` | `"q"`, `"r"` | **Agrupar** / **Desagrupar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057a9e0`, `0x18057aad0`) |
| **`Nudos`** | `0x1803de0b0` (`"nudos"`) | `"Nudo_Nuevo"` | `"nudo_new2"` | **Nuevo** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1805570b0`) |
| **`Nudos`** | `0x1803de0b0` | `"Nudo_Borrar"` | `"nudo_bor2"` | **Borrar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180566ae0`) |
| **`Nudos`** | `0x1803de0b0` | `"Nudo_Mover"` | `"nudo_mov2"` | **Mover** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180566da0`) |
| **`Barras`** | `0x1803b8e70` (`"barra"` en `ed3mnubar.c`) | `"Barra_Nueva"` | `"0"` | **Barra** (Nueva) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180556ed0`) |
| **`Barras`** | `0x1803b8e70` | `"Barra_CrearPieza"` | `"2"` | **Pieza** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180568080`) |
| **`Barras`** | `0x1803b8e70` -> `0x1807a7640` / `0x18074e9d0` | `"pilares_nuevo"` / `"pv_ed_nuevo_portico"` | `"nuec"` | **Pilar** / **Viga** | `ebaboton_nousar_crea_receptor_opcion_u` |
| **`Barras`** | `0x1803b8e70` | `"Barra_Borrar"` | `"1"` | **Borrar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180551910`) |
| **`Barras`** | `0x1803b8e70` | `"Barra_Describir"` | `"5"` | **Sección** (Describir) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180551cb0`) |
| **`Barras`** | `0x1803b8e70` | `"Barra_DescribirMaterial"` | `"7"` | **Material** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180551ef0`) |
| **`Barras`** | `0x1803b8e70` | `"Barra_DescribirEnrase"` | `"6"` | **Disposición** (Enrase) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180551d40`) |
| **`Barras`** | `0x1803b8e70` | `"Barra_mover_extremo"` | `"movext"` | **Mover extremo** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1805521c0`) |
| **`Barras`** | `0x1803b8e70` | `"Barra_InvertirEjeX"` | `"g"` | **Invertir eje 'x'** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180552130`) |
| **`Barras`** | `0x1803b8e70` | `"Barra_Agrupar"` / `"Barra_Desagrupar"` | `"3"`, `"4"` | **Agrupar** / **Desagrupar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1805517f0`, `0x180551a90`) |
| **`Barras`** | `0x1803b8e70` | `"Barra_Receptor_Informacion"` | `"info3"` | **Información** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180561300`) |
| **`Barras`** | `0x1803b8e70` | `"Barra_act_des_conex_int_barras_a_laminas"` | `"conexlam"` | Conexión interior barras a láminas (icono lateral) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180551760`) |
| **`Barras`** | `0x1803b8e70` | `"Barra_ConDivisionBarras"` | `"f"` | Generar nudos en cortes entre barras (Interruptor) | `ebaboton_nousar_crea_boton_interruptor_u` |
| **`Barras`** | `0x1803b8e70` -> `0x1807a7c50` (`"pilares"`, icono `"edit"`) | `"pilares_nuevo"`, `"pilares_cantos_forjado"`, `"pilares_referencias"`, `"pilares_numerar"` | `"nuec"`, `"edcf"`, `"edref"`, `"num"` | **Edición de pilares** (desplegable) | Submenú en `ed3mnupi.c` |
| **`Barras`** | `0x1803b8e70` -> `0x18074f1e0` (`"pv_ed"`, icono `"edit"`) | `"pv_ed_nuevo_portico"`, `"pv_ed_editar_nudos"`, `"pv_ed_editar_forjados"`, `"pv_ed_referencias_vigas"`, `"pv_ed_referencias_porticos_vigas"`, `"pv_ed_Flecha_Limite"`, `"pv_ed_editar_ambiente_hormigon"` | `"nuec"`, `"ednu"`, `"edforj"`, `"edref"`, `"edrefc"`, `"flelim"`, `"amb"` | **Edición de vigas** (desplegable) | Submenú en `ed3mnupv.c` |
| **`Barras`** | `0x1803b8e70` | `"Barra_AsignarPlanta"` | `"asocbapla"` | Asignar planta (condicional) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180551880`) |
| **`Láminas`** | `0x180701ac0` (`"laminas"` en `ed3mnulamef.c`) | `"lamef_nueva_lamina"` | `"nue"` | **Nueva** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180564880`) |
| **`Láminas`** | `0x180701ac0` | `"lamef_borrar"` | `"bor"` | **Borrar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180562520`) |
| **`Láminas`** | `0x180701ac0` | `"lamef_desc_seccion"` | `"edit"` | **Sección** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180562890`) |
| **`Láminas`** | `0x180701ac0` | `"lamef_material"` | `"mat"` | **Material** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1805627d0`) |
| **`Láminas`** | `0x180701ac0` | `"lamef_enrase"` | `"enra"` | **Disposición** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180562770`) |
| **`Láminas`** | `0x180701ac0` | `"lamef_mover_nudos"` | `"mov"` | **Mover** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1806c5bd0`) |
| **`Láminas`** | `0x180701ac0` | `"lamef_unir"` | `"unir"` | **Unir** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180562ad0`) |
| **`Láminas`** | `0x180701ac0` | `"lamef_dividir"` | `"div"` | **Dividir** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1806c1f80`) |
| **`Láminas`** | `0x180701ac0` | `"lamef_invertir_eje_z"` | `"invz"` | **Invertir eje 'z'** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1805629b0`) |
| **`Láminas`** | `0x180701ac0` | `"lamef_set_arista_define_eje_x"` | `"invx"` | **Definir eje 'x'** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180562a40`) |
| **`Láminas`** | `0x180701ac0` | `"lamef_ed_angulo_giro_ejes_lamina"` | `"giroej"` | **Ejes locales** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180562650`) |
| **`Herramientas`** | `0x1803dd1e0` (`"herramientas"`) -> `0x180410e20` (`"generacion"`, icono `"gen"`) | `"Obra_GenerarPlanos"`, `"Obra_GenerarNudosYBarras"`, `"Obra_GenerMallaTetraedros"` | `"y"`, `"w"`, `"v"` | **Generar** (desplegable: Planos, Nudos y barras, Malla de tetraedros) | `ebaboton_nousar_crea_receptor_opcion_u` |
| **`Herramientas`** | `0x1803dd1e0` | `"movimiento_multiple"` | `"elem_mov2"` | Mover elementos (icono matriz) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057e200`) |
| **`Herramientas`** | `0x1803dd1e0` | `"giro_multiple_plano_xy"` | `"gxy"` | Giro en plano XY | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057e170`) |
| **`Herramientas`** | `0x1803dd1e0` | `"copia_multiple"` | `"copiar"` | Copiar elementos | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057df60`) |
| **`Herramientas`** | `0x1803dd1e0` | `"simetria_multiple_plano_xy"` | `"simcpxy"` | Simetría en plano XY | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057e2a0`) |
| **`Herramientas`** | `0x1803dd1e0` | `"giro_multiple"` | `"g2p"` | Giro alrededor de un eje (2 puntos) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057e030`) |
| **`Herramientas`** | `0x1803dd1e0` | `"giro_multiple_3_ptos"` | `"g3p"` | Giro mediante 3 puntos | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057e0e0`) |
| **`Herramientas`** | `0x1803dd1e0` | `"simetria_multiple"` | `"simcopiar"` | Simetría respecto a un plano | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057e340`) |
| **`Herramientas`** | `0x1803dd1e0` | `"Obra_Buscar"` | `"buscarel"` | Buscar (icono lupa) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1808867b0`) |
| **`Herramientas`** | `0x1803dd1e0` | `"Obra_Referencia"` | `"asgref"` | Referencia (icono etiqueta `ABC`) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18088e330`) |
| **`Herramientas`** | `0x1803dd1e0` | `"Obra_RepararGeometria"` | `"repobra"` | **Reparar** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Importar`** | `0x1803ddd70` (`"importar"`) | `"Obra_Importar"` | `"dxfplt2"` | **DXF o DWG** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18055d280`) |
| **`Importar`** | `0x1803ddd70` | `"Obra_Importar_ascii"` | `"impascii2"` | **Fichero de texto** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18055d1e0`) |

---

#### Pestaña 3: **`Propiedades`** (`0x18087ebd0` en `ed3mnuobrb.c` — Captura `08_estructura_tab_propiedades.png`)

| Grupo del Ribbon | Función Constructora | ID del Submenú / Botón (`entomenu.dll`) | Recurso de Icono (`componen.dll`) | Etiqueta Visible en UI | Tipo de Botón / Callback |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`Diafragmas`** | `0x1803da6d0` (`"diafragmas"`) | `"opc_diafragmas"` | `"diafrags"` | **Editar** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Diafragmas`** | `0x1803da6d0` | `"mod_ed_diafragma_nudo"` | `"anudiaf"` | **Diafragma** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1807f0eb0`) |
| **`Diafragmas`** | `0x1803da6d0` | `"gen_diafrags"` | `"dgdiafs"` | **Generar por niveles** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Nudos`** | `0x1803dbfb0` (`"props_nudos"`) | `"Nudo_Describir_Vinc_Int"` | `"1"` | **Vinculación interior** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18056cc50`) |
| **`Nudos`** | `0x1803dbfb0` | `"Nudo_Describir_Vinc_Ext"` | `"2"` | **Vinculación exterior** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180566ce0`) |
| **`Nudos`** | `0x1803dbfb0` | `"Nudo_Ligaduras"` | `"ligaduras"` | **Ligaduras** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18056b820`) |
| **`Barras`** | `0x1803b85e0` (`"props_barras"`) | `"Barra_Pandeo"` | `"8"` | **Pandeo** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180552270`) |
| **`Barras`** | `0x1803b85e0` | `"Barra_Pandeo_Lateral"` | `"9"` | **Pandeo lateral** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180552300`) |
| **`Barras`** | `0x1803b85e0` | `"Barra_EmparejarArriostramientos"` | `"parri"` | **Parejas** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180876550`) |
| **`Barras`** | `0x1803b85e0` | `"Barra_Articular"` | `"i"` | **Articular** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180874070`) |
| **`Barras`** | `0x1803b85e0` | `"Barra_Empotramiento_Extremos_Pieza"` | `"d"` | **Empotramiento** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180551980`) |
| **`Barras`** | `0x1803b85e0` | `"Barra_rotulas_plasticas"` | `"rotplas"` | **Rótulas plásticas** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180864b80`) |
| **`Barras`** | `0x1803b85e0` -> `0x1803c7590` (`"grupos_flecha"`, icono `"grufle"`) | `"Barra_Crear_grupo_flecha"`, `"Barra_Editar_grupo_flecha"`, `"Barra_Flecha_Limite"` | `"b"`, `"e"`, `"a"` | **Grupos de flecha** (desplegable) | `ebaboton_nousar_crea_receptor_opcion_u` |
| **`Barras`** | `0x1803b85e0` -> `0x1803c78c0` (`"resistencia_fuego"`, icono `"resfuego2"`) | `"Resistencia_Fuego_Datos_Comprobacion"`, `"Resistencia_Fuego_Exposicion_Caras"`, `"Resistencia_Fuego_Espesor_Revestimiento"`, `"Resistencia_Fuego_Grupos"` | `"inccomp"`, `"inccarexp"`, `"incesp"`, `"incgru"` | **Resistencia al fuego** (desplegable) | `ebaboton_nousar_crea_receptor_opcion_u` / `panel_obra_u` |
| **`Barras`** | `0x1803b85e0` | `"Barra_anmodal"` | `"vib"` | **Análisis modal** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180551b00`) |
| **`Barras`** | `0x1803b85e0` | `"Barra_sist_arriostr"` | `"sisre"` | Sistemas resistentes a sismo (condicional) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180551f80`) |
| **`Láminas`** | `0x1807010c0` (`"props_laminas"` en `ed3mnulamef.c`) | `"lamef_vinc_int_aristas"` | `"vincint"` | **Vinculación interior** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180562950`) |
| **`Láminas`** | `0x1807010c0` | `"lamef_vinc_ext_aristas"` | `"vincext"` | **Vinculación exterior** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1805628f0`) |
| **`Láminas`** | `0x1807010c0` | `"lamef_bandas_integracion"` | `"bandint"` | **Bandas de integración** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1805626b0`) |
| **`Láminas`** | `0x1807010c0` | `"lamef_discretizacion"` | `"discr"` | **Discretización** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180562710`) |
| **`Láminas`** | `0x1807010c0` | `"lamef_edfrig"` | `"edfrig"` | **Factores de rigidez** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180562590`) |
| **`Láminas`** | `0x1807010c0` | `"lamef_ed_pandeo"` | `"panlam"` | **Pandeo** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180562830`) |
| **`Láminas`** | `0x1807010c0` | `"lamef_ed_modal"` | `"vib"` | **Análisis modal** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1805625f0`) |
| **`Láminas`** | `0x1807010c0` | `"lamef_asignar_plantas"` | `"asocpla"` | Asignar plantas (condicional) | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180562490`) |

---

#### Pestaña 4: **`Carga`** (`0x1803c8630` en `ed3mnucar.c` — Captura `09_estructura_tab_carga.png`)

| Grupo del Ribbon | Función Constructora | ID del Submenú / Botón (`entomenu.dll`) | Recurso de Icono (`componen.dll`) | Etiqueta Visible en UI | Tipo de Botón / Callback |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`Visualización`** | `0x1803d89f0` (`"cargas_opcs_visualiz"`) | `"Carga_hipotesis_vista"` | `"0"` | **Hipótesis vista** | `ebaboton_nousar_crea_boton_dlg_obra_u` (`0x180612b90`) |
| **`Visualización`** | `0x1803d89f0` | `"Carga_escalas"` | `"dimens2"` | **Escalas** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Nudos`** | `0x1803d84a0` (`"cargas_nudos"`) | `"Carga_nudo_desp_y_giros_prescritos"` | `"7"` | **Desplazamientos prescritos** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180566c20`) |
| **`Nudos`** | `0x1803d84a0` | `"Carga_nudo_nueva_carga"` | `"4"` | **Nueva** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18055a860`) |
| **`Nudos`** | `0x1803d84a0` | `"Carga_nudo_editar_carga"` | `"5"` | **Editar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18055a0a0`) |
| **`Nudos`** | `0x1803d84a0` | `"Carga_nudo_borrar_carga"` | `"6"` | **Borrar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180559e50`) |
| **`Nudos`** | `0x1803d84a0` | `"Editar_hipotesis_carga_nudo"` | `"chnud"` | **Hipótesis** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18055a180`) |
| **`Barras`** | `0x1803d7a40` (`"cargas_barras"`) | `"Carga_barra_nueva_carga"` | `"1"` | **Nueva** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18055ad70`) |
| **`Barras`** | `0x1803d7a40` | `"Carga_barra_editar_carga"` | `"2"` | **Editar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180559fc0`) |
| **`Barras`** | `0x1803d7a40` | `"Carga_barra_borrar_carga"` | `"3"` | **Borrar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180559d50`) |
| **`Barras`** | `0x1803d7a40` | `"Editar_hipotesis_cargas_barras"` | `"chbar"` | **Hipótesis** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18055a1f0`) |
| **`Barras`** | `0x1803d7a40` | `"Carga_barra_activar_desactiva_pp"` | `"adpp"` | **Peso propio** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1806b6c80`) |
| **`Láminas`** | `0x1803d7f80` (`"cargas_laminas"`) | `"nueva_carga_lamina"` | `"nuelam"` | **Nueva** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1806e9900`) |
| **`Láminas`** | `0x1803d7f80` | `"editar_carga_lamina"` | `"edlam"` | **Editar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18055a030`) |
| **`Láminas`** | `0x1803d7f80` | `"borrar_cargas_laminas"` | `"borlam"` | **Borrar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180559dd0`) |
| **`Láminas`** | `0x1803d7f80` | `"editar_hipotesis_cargas_laminas"` | `"edhlam"` | **Hipótesis** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18055a110`) |
| **`Láminas`** | `0x1803d7f80` | `"activar_desactiva_pp_lamina"` | `"adpplam"` | **Peso propio** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1806b6be0`) |
| **`Paños`** | `0x1803d8c00` (`"cargas_panyos"`) | `"nuevo_panyo"` | `"d"` | **Nuevo** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18056d360`) |
| **`Paños`** | `0x1803d8c00` | `"editar_panyo"` | `"i"` | **Editar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18056f9b0`) |
| **`Paños`** | `0x1803d8c00` | `"borrar_panyo"` | `"e"` | **Borrar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18056f940`) |
| **`Paños`** | `0x1803d8c00` | `"mover_panyo"` | `"m"` | **Mover** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180570180`) |
| **`Paños`** | `0x1803d8c00` | `"dividir_panyo"` | `"divpa"` | **Dividir** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180871160`) |
| **`Paños`** | `0x1803d8c00` | `"seleccionar_direcc_panyo"` | `"f"` | **Dirección de reparto** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18056fa40`) |
| **`Cargas superficiales`** | `0x1803d91e0` (`"cargas_superf"`) | `"nueva_superficie_carga"` | `"8"` | **Nueva** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18056d2d0`) |
| **`Cargas superficiales`** | `0x1803d91e0` | `"editar_superficie_carga"` | `"a"` | **Editar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18055a2d0`) |
| **`Cargas superficiales`** | `0x1803d91e0` | `"borrar_superficie_carga"` | `"9"` | **Borrar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180559ed0`) |
| **`Cargas superficiales`** | `0x1803d91e0` | `"Editar_hipotesis_superficie_carga"` | `"chcsup"` | **Hipótesis** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18055a260`) |
| **`Cargas superficiales`** | `0x1803d91e0` | `"copiar_superficies_carga"` | `"b"` | **Copiar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180559f50`) |
| **`Cargas superficiales`** | `0x1803d91e0` | `"mover_superficie_carga"` | `"n"` | **Mover** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180570210`) |
| **`Cargas superficiales`** | `0x1803d91e0` | `"activar_desactivar_cargas_superf_barra"` | `"l"` | **Asignar** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1805516d0`) |

---

#### Pestaña 5: **`Uniones`** (`0x1803dc290` `"uniones_ed"` y `0x1803da4a0` `"uniones_e3di"` en `ed3mnuob.c` — Captura `10_estructura_tab_uniones.png`)

`ed3dlib.dll` registra dos variantes de menú de uniones según el modo de edición activa de la obra (`"uniones_ed"` para CYPE Connect / StruBIM Steel integrado en `0x1803dc290`, visible por defecto en la versión 2027, y `"uniones_e3di"` en `0x1803da4a0` que agrupa `uniones_auto` + `Placas_Anclaje`):

1. **Modo `uniones_ed` (`0x1803dc290` — visible en `10_estructura_tab_uniones.png`):**
   - `"ued_nueva"` (icono `"nuunione3d2"`, **Nueva**, callback `0x1805867e0`)
   - `"ued_borrar"` (icono `"bounione3d2"`, **Borrar**, callback `0x1805844e0`)
   - `"ued_editar"` (icono `"edunione3d2"`, **Editar**, callback `0x180584670`)
   - *(Separador)*
   - `"ued_editar_asignar"` (icono `"asunione3d2"`, **Asignar**, callback `0x1805846d0`)
   - `"ued_agrupar"` (icono `"agrunione3d2"`, **Agrupar**, callback `0x1805843d0`)
   - `"ued_desagrupar"` (icono `"dagrunione3d2"`, **Desagrupar**, callback `0x180584550`)
   - `"ued_bloquear"` (icono `"blqunione3d2"`, **Bloquear**, callback `0x180584450`)
   - `"ued_desbloquear"` (icono `"dblqunione3d2"`, **Desbloquear**, callback `0x1805845e0`)
   - `"ued_Buscar"` (icono `"busunione3d2"`, **Buscar**, callback `0x180559640`)
   - *(Separador)*
   - `"ued_generar"` (icono `"genunione3d2"`, **Generar**, `ebaboton_nousar_crea_boton_dlg_obra_u`)
   - `"ued_analisis_rigidez_rotacional"` (icono `"rig_rot3"`, **Rigideces rotacionales**, callback `0x1805850f0`)

2. **Modo clásico `uniones_e3di` (`0x1803da4a0` -> `0x18040e6f0` `"uniones_auto"` + `0x18040e0f0` `"Placas_Anclaje"`):**
   - **Submenú `"uniones_auto"` (`0x18040e6f0`):**
     - `"uaut_opcs_calculo"` (icono `"opc_uniones"`)
     - `"uaut_analizar"` (icono `"dimunione3d2"`)
     - `"uaut_consulta_uniones"` / `"uaut_consulta_uniones_sin_calculo"` (icono `"ver_uniones"`)
     - `"uaut_analisis_rigidez_rotacional"` (icono `"rig_rot3"`)
     - `"uaut_consultar_incidencias"` (icono `"ver_uniones"`, `"Listado de incidencias"`)
   - **Submenú `"Placas_Anclaje"` (`0x18040e0f0`):**
     - `"Placas_Anclaje_generar"` (icono `"genplaca2"`)
     - `"Placas_Anclaje_borrar"` (icono `"borplaca2"`, callback `0x180566b50`)
     - `"Placas_Anclaje_editar"` (icono `"editplaca2"`, callback `0x180566c80`)
     - `"Placas_Anclaje_igualar"` (icono `"igplaca2"`, callback `0x180566d40`)
     - `"Placas_Anclaje_comprobar"` (icono `"g"`, callback `0x180566bc0`)
     - `"Placas_Anclaje_dimensionar"` (icono `"j"`, `ebaboton_nousar_crea_boton_dlg_obra_u`)

---

#### Pestaña 6: **`Cálculo`** (`0x1803da560` en `ed3mnuob.c` — Captura `11_estructura_tab_calculo.png`)

| Grupo del Ribbon | Función Constructora | ID del Submenú / Botón (`entomenu.dll`) | Recurso de Icono (`componen.dll`) | Etiqueta Visible en UI | Tipo de Botón / Callback |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`Tensión / Deformación`** | `0x18040cec0` (`"Calculo_ts"`) | `"Calculo_CombsANL"` | `"combsanl"` | **Combinaciones no lineales** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Tensión / Deformación`** | `0x18040cec0` | `"Calculo_calcular"` | `"calcts"` | **Calcular** | `ebaboton_nousar_crea_boton_dlg_obra_vista_con_1_extra_u` |
| **`Tensión / Deformación`** | `0x18040cec0` -> `0x18040de30` (`"dimensionar"`, icono `"dimensio2"`) | Submenús: `0x18040ee40` (`"Calculo_Dimensionamiento"`: `"dimensionamiento_desde_perfil_actual"` `"d"`, `"dimensionamiento_desde_primer_perfil"` `"b"`), `0x1807a7a20` (`"pilares_calc"`), `0x18074efb0` (`"pv_calc"`), `0x18040da20` (`"uniones_ed_calc"`), más botón `"Calculo_Dimensionar_revestimiento_incendio"` (`"dimesprev"`) | `"dimensio2"`, `"d"`, `"b"`, `"pil"`, `"vigas"`, `"un"`, `"dimesprev"` | **Dimensionar** (desplegable) | `ebamenu_crea_con_icono_u` |
| **`Tensión / Deformación`** | `0x18040cec0` | `"Calculo_Desplazamientos"` | `"3"` | **Desplazamientos** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057fbf0`) |
| **`Tensión / Deformación`** | `0x18040cec0` | `"Calculo_Reacciones"` | `"4"` | **Reacciones** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057f3b0`) |
| **`Tensión / Deformación`** | `0x18040cec0` | `"Calculo_esfuerzos"` | `"8"` | **Esfuerzos** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18072c450`) |
| **`Tensión / Deformación`** | `0x18040cec0` | `"Calculo_esfuerzos3D"` | `"defiso"` | **Isovalores** | `ebaboton_nousar_crea_boton_dlg_obra_vista_con_nombre_y_descripcion_2extra_u` |
| **`Tensión / Deformación`** | `0x18040cec0` | `"Calculo_Empresillado"` | `"5"` | **Empresillado** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180552010`) |
| **`Tensión / Deformación`** | `0x18040cec0` | `"Calculo_sismo_dinamico"` | `"6"` | **Sismo dinámico** | `ebaboton_nousar_crea_boton_dlg_const_obra_con_nombre_y_descripcion_u` |
| **`Tensión / Deformación`** | `0x18040cec0` | `"Calculo_comprueba_elementos"` | `"comproba2"` | **Comprobar elementos** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180551a10`) |
| **`Tensión / Deformación`** | `0x18040cec0` | `"Calculo_comprobaciones_ELU"` | `"comprobeluels2"` | **Comprobaciones E.L.U.** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180580a60`) |
| **`Pandeo`** | `0x18040bfc0` (`"AnalisisPandeo"`) | `"Calculo_GruAnPan"` | `"grupan"` | **Grupos de análisis** | `ebaboton_nousar_crea_boton_panel_obra_u` |
| **`Pandeo`** | `0x18040bfc0` | `"Calculo_CombsPanL"` | `"combpan"` | **Combinaciones** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Pandeo`** | `0x18040bfc0` | `"Calculo_CalcPan"` | `"clpand"` | **Calcular** | `ebaboton_nousar_crea_boton_dlg_obra_vista_con_1_extra_u` |
| **`Pandeo`** | `0x18040bfc0` | `"Calculo_ModosPandeo"` | `"modospan"` | **Modos de pandeo** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18083a650`) |
| **`Pandeo`** | `0x18040bfc0` | `"Calculo_esfuerzos3D"` | `"panisoval"` | **Isovalores** | `ebaboton_nousar_crea_boton_dlg_obra_vista_con_nombre_y_descripcion_2extra_u` |
| **`Pandeo`** | `0x18040bfc0` | `"Calculo_AsignaCoefsPan"` | `"asgcoefpan"` | **Asignar coeficientes** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Modal`** | `0x18040bc50` (`"AnalisisModal"`) | `"Calculo_CombsModal"` | `"vibccarga"` | **Casos de carga** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Modal`** | `0x18040bc50` | `"Calculo_CalcModal"` | `"vibcalc"` | **Calcular** | `ebaboton_nousar_crea_boton_dlg_obra_vista_con_1_extra_u` |
| **`Modal`** | `0x18040bc50` | `"Calculo_ModosVibracion"` | `"vibmodos"` | **Modos de vibración** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18087f1d0`) |
| **`Modal`** | `0x18040bc50` | `"Calculo_listado_modos_vibracion"` | `"viblst"` | **Listado** | `ebaboton_nousar_crea_boton_dlg_const_obra_con_nombre_y_descripcion_u` |
| **`Pushover`** | `0x18040c650` (`"AnalisisPushover"`) | `"PO_Combinaciones"` | `"poccarga"` | **Casos de carga** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Pushover`** | `0x18040c650` | `"PO_SelNuCtrl"` | `"ponuctrl"` | **Nudo de control** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180566e30`) |
| **`Pushover`** | `0x18040c650` | `"PO_Calcular"` | `"pocalc"` | **Calcular** | `ebaboton_nousar_crea_boton_dlg_obra_vista_con_1_extra_u` |
| **`Pushover`** | `0x18040c650` | `"PO_Desplazamientos"` | `"podesps"` | **Desplazamientos** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057fbd0`) |
| **`Pushover`** | `0x18040c650` | `"PO_Reacciones"` | `"poreaccs"` | **Reacciones** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18057f390`) |
| **`Pushover`** | `0x18040c650` | `"PO_Esfuerzos"` | `"poesfs"` | **Esfuerzos** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x18072c420`) |
| **`Pushover`** | `0x18040c650` | `"PO_esfuerzos3D"` | `"poisoval"` | **Isovalores** | `ebaboton_nousar_crea_boton_dlg_obra_vista_con_nombre_y_descripcion_2extra_u` |
| **`Pushover`** | `0x18040c650` | `"PO_curva_capacidad"` | `"pocapac"` | **Capacidad** | `ebaboton_nousar_crea_boton_dlg_obra_u` |
| **`Pushover`** | `0x18040c650` | `"PO_comp_rotulas"` | `"poesfsrot"` | **Rótulas plásticas** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180892140`) |
| *(Extensor derecha)* | `ebamenu_append_extensor` | — | — | Alínea Incidencias a la derecha | — |
| **`Incidencias`** | `0x18040e060` (`"incidencias"`) | `"Calculo_mostrar_errores"` | `"erroravi2"` | **Incidencias** | `ebaboton_nousar_crea_boton_mostrar_errores` |

---

#### Pestaña 7: **`Armaduras`** (`0x180886670` en `ed3mnuarmrb.c` — Captura `12_estructura_tab_armaduras.png`)

| Grupo del Ribbon | Función Constructora | ID del Submenú / Botón (`entomenu.dll`) | Recurso de Icono (`componen.dll`) | Etiqueta Visible en UI | Tipo de Botón / Callback |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`Pilares`** | `0x1807a7750` (`"pilares"` en `ed3mnupi.c`) | `"pilares_armaduras"` | `"edarm"` | **Editar armaduras** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1807adb80`) |
| **`Pilares`** | `0x1807a7750` | `"pilares_bloquear"` | `"bloq"` | **Bloquear** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1807ad9a0`) |
| **`Pilares`** | `0x1807a7750` | `"pilares_desbloquear"` | `"dbloq"` | **Desbloquear** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1807ada30`) |
| **`Vigas`** | `0x18074eb20` (`"pv_ed"` en `ed3mnupv.c`) | `"pv_ed_editar_todos_portico"` | `"edarm"` | **Editar armaduras** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180758560`) |
| **`Vigas`** | `0x18074eb20` | `"pv_ed_editar_porticos_ventana_activa"` | `"edarm"` | *(Condicional `CYPE3D_EDITAR_PORTICOS_VENTANA_ACTIVA`)* | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1807584f0`) |
| **`Vigas`** | `0x18074eb20` | `"pv_ed_asignar"` | `"igu"` | **Asignar armaduras** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1807578e0`) |
| **`Vigas`** | `0x18074eb20` | `"pv_ed_bloquear"` | `"bloq"` | **Bloquear** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x180757940`) |
| **`Vigas`** | `0x18074eb20` | `"pv_ed_desbloquear"` | `"dbloq"` | **Desbloquear** | `ebaboton_nousar_crea_receptor_opcion_u` (`0x1807579d0`) |

---

#### Pestaña 8: **`Ventana`** (Marco MDI de `entomenu.dll` — Captura `13_estructura_tab_ventana.png`)

Contiene el grupo **`Ventana`** con 9 botones para la gestión de subventanas MDI (`3D`, vistas 2D de pórticos/planos):
1. **Abrir Nueva**
2. **Renombrar**
3. **Cerrar**
4. **Anterior** (flecha izquierda)
5. **Siguiente** (flecha derecha)
6. **Cascada**
7. **Mosaico horizontal**
8. **Mosaico vertical**
9. **Ordenar ventanas**
10. **Seleccionar ventana** (desplegable)

#### Menús Auxiliares en `ed3dlib.dll` (`Menu_Exportacion` y `Debug`)
- **`Menu_Exportacion` (`0x1803daa40` en `ed3mnuob.c`):** Registrado mediante `ebamenu_crea_menu_exportacion("Menu_Exportacion")` (integrado en el menú de aplicación / exportación):
  - `"Exportacion_A_Tekla"` (icono `"tekla"`)
  - `"Exportacion_A_CIS2"` (icono `"cis2"`)
  - `"Exportacion_A_CSV"` (icono `"expcsv"`)
- **`Debug` (`0x1803da680` -> `0x18040dcc0`):** Solo visible en compilaciones internas de depuración (`"Calc_MantenerFicherosFront"`, `"Calc_Modelo3D"`).

---

## 5. Solapa 2: **Cimentación** (`planocim.dll` — `plcpanel_crea_solapa_cimentacion`)

### 5.1. Decompilación de `plcpanel_crea_solapa_cimentacion` (`0x18009a890`, Evidencia Ghidra `ev_a320baca7517d7a751d387d9e6c9fb4d8aff9ffa4db8f68e2511bf91463090f0`)

La función exportada [`plcpanel_crea_solapa_cimentacion`](file:///D:/CYPE%20Ingenieros/Versión%202027/CYPE%203D/planocim.dll) (`0x18009a890`) recibe 4 parámetros (`int param_1, undefined8 param_2, undefined8 param_3, undefined8 param_4`, donde `param_1` indica si se admiten elementos lineales/zapatas corridas) y construye la solapa **Cimentación** (captura `15_cimentacion_main_window.png`):

```c
void plcpanel_crea_solapa_cimentacion(int param_1, undefined8 param_2, undefined8 param_3, undefined8 param_4) {
    uVar1 = FUN_1800970c0();
    uVar2 = FUN_180097090();
    uVar3 = FUN_1800985b0();
    uVar4 = FUN_180098580();
    uVar5 = FUN_180098550();
    uVar6 = FUN_18009a800(param_1);
    uVar7 = FUN_180098750();
    uVar8 = FUN_1800972f0();
    uVar9 = FUN_18009a840();
    uVar10 = FUN_180097250(param_1, param_2); // ebarcbca_crea_configuracion_barra_capturas
    uVar11 = FUN_1800c3cf0(param_1);          // ebarcbas_nousar_crea_receptor_info_objeto
    uVar12 = FUN_180097ed0(param_1, param_3, param_4); // ebamenu_crea_u("menu_unico", ...)
    uVar13 = FUN_1800ffbf0("ID_CIMENTACION"); // traduce desde "cimentacion.msg" -> "Cimentación"
    uVar14 = ebasolap_nousar_crea(
        uVar13, uVar12, 0, uVar11, 0, uVar10,
        uVar9, uVar8, uVar7, uVar6, uVar5, uVar4, uVar3, 0,
        uVar2, uVar1,
        "plcdatos_t", "ebadatvi_datos_vista_nulo_t"
    );
    return uVar14;
}
```

Características arquitectónicas clave de **Cimentación**:
- **Sin pestañas múltiples en el Ribbon (`"menu_unico"`):** En lugar de múltiples pestañas superiores como en **Estructura**, `0x180097ed0` crea un único menú raíz `"menu_unico"` con `ebamenu_set_aspecto_menu_ribbon(..., 0x10)` y añade directamente los 4 bloques de banda (`menu_obra`, `menu_cimentacion`, `limites`, `menu_calculo`) mediante `ebamenu_append_submenu_desplegable`.
- **Sin panel lateral de docking (`panel_dock = NULL`):** El área de dibujo 2D/3D (`CypeAreaDibujo`) ocupa todo el ancho de la ventana (`0, 142, 1920×852`).
- **Tipos de datos:** `"plcdatos_t"` (modelo de cimentación) y `"ebadatvi_datos_vista_nulo_t"` (vista).
- **Configuración de barra de capturas CAD (`0x180097250`):**
  `ebarcbca_crea_configuracion_barra_capturas(0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, param_1, param_2)`.

---

### 5.2. Mapa Completo de Grupos y Comandos de la Solapa **Cimentación** (`15_cimentacion_main_window.png`)

#### Grupo 1: **`Obra`** (`0x18009a200` — `"menu_obra"`, `x = 1..216, y = 51..142`)

| ID del Botón / Submenú (`entomenu.dll`) | Recurso de Icono (`componen.dll`) | Etiqueta Visible en UI | Tipo de Botón / Submenú y Handler (`planocim.dll`) |
| :--- | :--- | :--- | :--- |
| `"datos_generales"` | `"datobra3"` | **Datos generales** | `ebaboton_nousar_crea_boton_dlg_obra_u` -> `0x180035f70` (`plcpaneles_panel_datos_generales` `0x18009ddd0`) |
| **`"menu_opciones"`** (`0x180097310`) | `"opdimens2"` | **Opciones** (Desplegable) | Submenú `ebamenu_crea_con_icono_u`:<br>1. `"opciones_recubrimientos"` (`0x180036880`)<br>2. `"opciones_mermas"` (icono `"tantp1002"`, `0x180036820`)<br>3. `"opciones_hormigon_limpieza"` (`0x1800366c0`)<br>4. `"opciones_comprobacion_fisuracion_u"` (icono `"fisuracion2"`, `0x1800363e0`)<br>5. `"opciones_elemento_aislado"` (icono `"cimen2"`, `0x180036630`)<br>6. `"opciones_elemento_lineal"` (condicional si `param_1 == 1`, `0x180036680`)<br>7. `"opciones_encepados"` (icono `"encepado2"`, `0x1800365f0`)<br>8. `"opciones_vigas_centradoras"` (`0x180036930`)<br>9. `"opciones_vigas_atado"` (`0x1800368e0`) |
| **`"submenu_tablas_armado"`** (`0x180097af0`) | `"opctabla2"` | **Tablas de armado** (Desplegable) | Submenú `ebamenu_crea_con_icono_u`:<br>1. `"tablas_armado_zapatas_aisladas"` (icono `"tbarmzp2"`, `0x180036a40`)<br>2. `"tablas_armado_zapatas_lineales"` (condicional si `param_1 == 1`, `0x180036aa0`)<br>3. `"tablas_armado_vig_cen"` (icono `"tbarmvc2"`, `0x1800369e0`)<br>4. `"tablas_armado_vig_ata"` (`0x180036980`) |
| *(Separador)* | — | — | `ebamenu_append_separador` |
| `"vista_3D"` | `"vista3d2"` | **Vista 3D** | `ebaboton_nousar_crea_boton_dlg_const_obra_con_nombre_y_descripcion_u` -> `0x1800970f0` |

#### Grupo 2: **`Elementos de cimentación`** (`0x180098bc0` — `"menu_cimentacion"`, `x = 223..803, y = 51..142`)

| ID del Botón / Submenú (`entomenu.dll`) | Recurso de Icono (`componen.dll`) | Etiqueta Visible en UI | Tipo de Botón / Submenú y Handler (`planocim.dll`) |
| :--- | :--- | :--- | :--- |
| **`"nuevo_elemento"`** (`0x180099910`) | `"nuevocim"` | **Nuevo** (Desplegable modeless) | `ebamenu_append_submenu_modeless`:<br>1. `"nuevo_zapata_horm_arm"` (icono `FUN_18002f360(0)`, handler `0x18002ec70`)<br>2. `"nuevo_zapata_horm_masa"` (icono `FUN_18002f360(1)`, handler `0x18002edb0`)<br>3. `"nuevo_encepado"` (icono `FUN_180025a30()`, handler `0x180024be0`)<br>4. `"nuevo_elemento_lineal_por_objeto"` (si `param_1 == 1`, icono `FUN_18001df80()`, handler `0x18001dbf0`)<br>5. `"nueva_viga_atado"` (icono `FUN_18002bc40()`, handler `0x18002b8e0`)<br>6. `"nueva_viga_centradora"` (icono `FUN_18002dd60()`, handler `0x18002d9d0`)<br>7. `"nueva_viga_automatica"` (icono `FUN_18002c600()`, handler `0x18002bc80`) |
| `"borrar"` | `"cancelcim"` | **Borrar** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x180003370` |
| `"editar"` | `"edicim"` | **Editar** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x180003480` |
| `"igualar_elemento"` | `FUN_1800059b0()` | **Igualar** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x180004df0` |
| `"informacion"` | `"infocim"` | **Información** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x180005a20` |
| *(Separador)* | — | — | `ebamenu_append_separador` |
| `"mover_elemento"` | `"movelcim2"` | **Mover** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x1800072e0` |
| **`"rotar_elementos_aislados"`** (`0x18009a460`) | `"rotzapat2"` | **Rotar** (Desplegable modeless) | `ebamenu_append_submenu_modeless`:<br>1. `"introduccion_manual"` (icono `"rotcalc"`, handler `0x180006bf0`)<br>2. `"introduccion_dos_puntos"` (icono `"rotpun"`, handler `0x1800068c0`)<br>3. `"angulo_del_pilar"` (icono `"rotpil"`, handler `0x180006820`) |
| `"unir_elemento"` | `"unirzp2"` | **Unir** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x180007bd0` |
| *(Separador)* | — | — | `ebamenu_append_separador` |
| `"ajustar_viga"` | `FUN_18002cb20()` | **Ajustar** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x18002c640` |
| `"prolongar_viga"` | `FUN_18002d730()` | **Prolongar** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x18002d200` |
| `"desplazar_viga"` | `FUN_18002d180()` | **Desplazar** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x18002cb90` |
| `"centrar_extremos_viga"` | `"centvig2"` | **Centrar extremos** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x18002d7b0` |

#### Grupo 3: **`Límites`** (`0x180099520` — `"limites"`, `x = 810..1029, y = 51..142`)
Los iconos de este grupo se obtienen dinámicamente mediante `plcpanel_iconos_limites(int idx)` (`0x18009acf0`):

| ID del Botón / Submenú (`entomenu.dll`) | Recurso de Icono (`plcpanel_iconos_limites`) | Etiqueta Visible en UI | Tipo de Botón / Submenú y Handler (`planocim.dll`) |
| :--- | :--- | :--- | :--- |
| **`"nuevo_limite"`** (`0x180099fb0`) | `plcpanel_iconos_limites(0)` (`"intlim2"`) | **Nuevo** (Desplegable modeless) | `ebamenu_append_submenu_modeless`:<br>1. `"crear_limite_por_puntos"` (`plcpanel_iconos_limites(1)` `"intlimpt"`, handler `0x180028040`)<br>2. `"crear_limite_con_polilinea_dxf"` (`plcpanel_iconos_limites(2)` `"intlimpl"`, handler `0x180027ea0`) |
| `"borrar_limite"` | `plcpanel_iconos_limites(3)` (`"borrlim2"`) | **Borrar** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x180027af0` |
| `"mover_limite"` | `plcpanel_iconos_limites(4)` (`"movlim2"`) | **Mover** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x180028750` |
| `"insertar_punto_limite"` | `plcpanel_iconos_limites(5)` (`"insvert2"`) | **Nuevo vértice** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x180028500` |
| `"borrar_tramo"` | `plcpanel_iconos_limites(6)` (`"bortram2"`) | **Borrar un tramo** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x180027bc0` |

#### Grupo 4: **`Cálculo`** (`0x180098780` — `"menu_calculo"`, `x = 1036..1390, y = 51..142`)

| ID del Botón / Submenú (`entomenu.dll`) | Recurso de Icono (`componen.dll`) | Etiqueta Visible en UI | Tipo de Botón / Submenú y Handler (`planocim.dll`) |
| :--- | :--- | :--- | :--- |
| `"generar_zapatas_y_vigas"` | `FUN_180005dc0()` | **Generar zapatas y vigas** | `ebaboton_nousar_crea_boton_dlg_obra_u` -> `0x180003740` |
| `"eliminar_solapes"` | `FUN_180033440()` | **Eliminar solapes** | `ebaboton_nousar_crea_boton_dlg_obra_u` -> `0x1800331a0` |
| *(Separador)* | — | — | `ebamenu_append_separador` |
| `"comprobar"` | `"comproba2"` | **Comprobar** | `ebaboton_nousar_crea_receptor_opcion_u` -> `0x180003170` |
| `"dimensionar"` | `"dimensio2"` | **Dimensionar** | `ebaboton_nousar_crea_boton_dlg_obra_u` -> `0x180003310` |
| *(Separador)* | — | — | `ebamenu_append_separador` |
| `"Calculo_MostrarErrores"` | `"erroravi2"` (estándar) | **Incidencias** | `ebaboton_nousar_crea_boton_mostrar_errores(..., "plcdatos_t", "ebadatvi_datos_vista_nulo_t")` |

---

## 6. Catálogo de Capturas de Referencia (`d:\Coding\CypeClon\rea_evidence\screenshots\`)

Todas las capturas en vivo fueron obtenidas en resolución nativa `1920×1032` (y recortes exactos de diálogo) mediante los scripts [`ui_driver.ps1`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/ui_driver.ps1), [`capture_flow.ps1`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/capture_flow.ps1) y [`capture_tabs.ps1`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/capture_tabs.ps1):

| Archivo PNG | Dimensiones | Contenido Documentado |
| :--- | :---: | :--- |
| [`01_startup_welcome_screen.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/01_startup_welcome_screen.png) | `1920×1032` | Pantalla inicial de bienvenida (`Archivo`: `Nuevo...`, `Gestión archivos`, `Ejemplos`; `Últimos ficheros`; `Ayuda`). |
| [`02_dialog_nueva_obra.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/02_dialog_nueva_obra.png) / [`02_composite_nueva_obra.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/02_composite_nueva_obra.png) | `537×285` / `1920×1032` | Diálogo modal `Nueva obra` (selección de ruta, nombre de fichero `.ed3` y descripción). |
| [`03_dialog_wizard_nueva_obra.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/03_dialog_wizard_nueva_obra.png) / [`03_wizard_nueva_obra.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/03_wizard_nueva_obra.png) | `682×457` / `1920×1032` | Asistente `Nueva obra` (`Obra vacía` vs `Introducción automática IFC` con previsualización vectorial 3D). |
| [`04_dialog_step4.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/04_dialog_step4.png) / [`04_after_wizard_aceptar.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/04_after_wizard_aceptar.png) | `848×774` / `1920×1032` | Asistente `Obra vacía` (configuración inicial de Normas, Perfiles, Hormigón armado, Acciones, Opciones, Terreno de cimentación y Pilotes/Micropilotes). |
| [`05_dialog_step5.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/05_dialog_step5.png) / [`05_after_step5.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/05_after_step5.png) | `358×438` / `1920×1032` | Diálogo `Estados límite` (combinaciones E.L.U., categoría de nieve y desplazamientos). |
| [`06_estructura_tab_obra.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/06_estructura_tab_obra.png) | `1920×1032` | Solapa **Estructura** — Pestaña Ribbon **`Obra`**, paneles laterales `Etiquetas`, `Categorías`, `Opciones de visualización` y `Área de trabajo` 3D. |
| [`07_estructura_tab_geometria.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/07_estructura_tab_geometria.png) | `1920×1032` | Solapa **Estructura** — Pestaña Ribbon **`Geometría`** (`Planos`, `Nudos`, `Barras`, `Láminas`, `Herramientas`, `Importar`). |
| [`08_estructura_tab_propiedades.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/08_estructura_tab_propiedades.png) | `1920×1032` | Solapa **Estructura** — Pestaña Ribbon **`Propiedades`** (`Diafragmas`, `Nudos`, `Barras`, `Láminas`). |
| [`09_estructura_tab_carga.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/09_estructura_tab_carga.png) | `1920×1032` | Solapa **Estructura** — Pestaña Ribbon **`Carga`** (`Visualización`, `Nudos`, `Barras`, `Láminas`, `Paños`, `Cargas superficiales`). |
| [`10_estructura_tab_uniones.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/10_estructura_tab_uniones.png) | `1920×1032` | Solapa **Estructura** — Pestaña Ribbon **`Uniones`** (`Nueva`, `Borrar`, `Editar`, `Asignar`, `Agrupar`, `Desagrupar`, `Bloquear`, `Desbloquear`, `Buscar`, `Generar`, `Rigideces rotacionales`). |
| [`11_estructura_tab_calculo.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/11_estructura_tab_calculo.png) | `1920×1032` | Solapa **Estructura** — Pestaña Ribbon **`Cálculo`** (`Tensión / Deformación`, `Pandeo`, `Modal`, `Pushover`, `Incidencias`). |
| [`12_estructura_tab_armaduras.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/12_estructura_tab_armaduras.png) | `1920×1032` | Solapa **Estructura** — Pestaña Ribbon **`Armaduras`** (`Pilares`, `Vigas`). |
| [`13_estructura_tab_ventana.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/13_estructura_tab_ventana.png) | `1920×1032` | Solapa **Estructura** — Pestaña Ribbon **`Ventana`** (`Abrir Nueva`, `Renombrar`, `Cerrar`, `Anterior`, `Siguiente`, `Cascada`, `Mosaico horizontal`, `Mosaico vertical`, `Ordenar ventanas`, `Seleccionar ventana`). |
| [`14_dialog_datos_generales.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/14_dialog_datos_generales.png) | `1920×1032` | Diálogo modal **`Datos generales`** abierto desde el botón `Obra -> Datos generales` sobre la ventana principal. |
| [`15_cimentacion_main_window.png`](file:///d:/Coding/CypeClon/rea_evidence/screenshots/15_cimentacion_main_window.png) | `1920×1032` | Solapa **Cimentación** — Banda Ribbon única (`Obra`, `Elementos de cimentación`, `Límites`, `Cálculo`) y área de dibujo 2D/3D a pantalla completa. |

