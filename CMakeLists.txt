cmake_minimum_required(VERSION 3.20)

project(CypeClon
    VERSION 2027.1.0
    DESCRIPTION "Clon en C++20 de CYPE 3D (Version 2027)"
    LANGUAGES CXX
)

# Requerir arquitectura de 64 bits (PE32+ x86_64 como el binario original)
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "CypeClon requiere compilacion en 64 bits (x86_64).")
endif()

# Estandar C++20 estricto
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Opciones de compilacion y UTF-8
if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive- /EHsc /MP)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

# Soporte de paralelismo OpenMP (equivalente a cyomp.dll / basicwin.dll)
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP C++ detectado y habilitado para CypeClon")
endif()

# OpenGL y librerias nativas de Windows (entograf.dll / panelwin.dll)
find_package(OpenGL QUIET)

# Directorio global de cabeceras
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# ==============================================================================
# Modulos Nucleo de Fase 1 (cypemem, basicos, cypestr, cypemath, unidades, io)
# ==============================================================================

add_library(cypeclon_core STATIC
    src/core/memory.cpp
    src/core/string_utils.cpp
    src/core/math.cpp
)
target_include_directories(cypeclon_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(OpenMP_CXX_FOUND)
    target_link_libraries(cypeclon_core PUBLIC OpenMP::OpenMP_CXX)
endif()

add_library(cypeclon_units STATIC
    src/units/units.cpp
)
target_link_libraries(cypeclon_units PUBLIC cypeclon_core)

add_library(cypeclon_io STATIC
    src/io/cypesave.cpp
    src/io/undoredo.cpp
)
target_link_libraries(cypeclon_io PUBLIC cypeclon_core cypeclon_units)

# ==============================================================================
# Modulos de Dominio, CAD y GUI (Esqueletos y Base para Fases 2-13)
# ==============================================================================

add_library(cypeclon_model STATIC
    src/model/obra.cpp
)
target_link_libraries(cypeclon_model PUBLIC cypeclon_core cypeclon_units cypeclon_io)

add_library(cypeclon_cad STATIC
    src/cad/cad_engine.cpp
)
target_link_libraries(cypeclon_cad PUBLIC cypeclon_core cypeclon_model)

add_library(cypeclon_solver STATIC
    src/solver/solver_engine.cpp
)
target_link_libraries(cypeclon_solver PUBLIC cypeclon_core cypeclon_model)

add_library(cypeclon_codes STATIC
    src/codes/normativa_engine.cpp
)
target_link_libraries(cypeclon_codes PUBLIC cypeclon_core cypeclon_units cypeclon_model)

add_library(cypeclon_connections STATIC
    src/connections/connections_engine.cpp
)
target_link_libraries(cypeclon_connections PUBLIC cypeclon_core cypeclon_model cypeclon_codes)

add_library(cypeclon_foundation STATIC
    src/foundation/foundation_engine.cpp
)
target_link_libraries(cypeclon_foundation PUBLIC cypeclon_core cypeclon_model cypeclon_codes)

add_library(cypeclon_gui STATIC
    src/gui/app_window.cpp
)
target_link_libraries(cypeclon_gui PUBLIC
    cypeclon_core
    cypeclon_units
    cypeclon_io
    cypeclon_model
    cypeclon_cad
    cypeclon_solver
    cypeclon_codes
    cypeclon_connections
    cypeclon_foundation
)
if(WIN32)
    target_link_libraries(cypeclon_gui PUBLIC user32 gdi32 comctl32 comdlg32 shell32 opengl32 dwmapi)
endif()

# ==============================================================================
# Ejecutable Principal: CypeClon.exe
# ==============================================================================

add_executable(CypeClon
    src/app/main.cpp
)
target_link_libraries(CypeClon PRIVATE cypeclon_gui)

# ==============================================================================
# Suite de Pruebas Unitarias (CTest)
# ==============================================================================

enable_testing()

add_executable(cypeclon_core_tests
    tests/test_main.cpp
    tests/test_math.cpp
    tests/test_units.cpp
    tests/test_io_undoredo.cpp
)
target_link_libraries(cypeclon_core_tests PRIVATE
    cypeclon_core
    cypeclon_units
    cypeclon_io
    cypeclon_model
    cypeclon_cad
)

add_test(NAME CoreUnitTests COMMAND cypeclon_core_tests)

