cmake_minimum_required(VERSION 3.11)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(ProyecThor
    VERSION 0.3.2
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ── VERSION GENERADA AUTOMATICAMENTE ─────────────────────────────────────────
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Version.h.in"
    "${CMAKE_BINARY_DIR}/generated/Version.h"
)

# ── 1. DEPENDENCIAS EXTERNAS (JSON, GLM y TagLib) ────────────────────────────

include(FetchContent)

# -- JSON --
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# -- GLM --
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# ── 2. RUTAS DE LIBRERIAS ────────────────────────────────────────────────────
set(IMGUI_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/src/external/imgui-docking")
set(GLFW_SDK   "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/sdk/glfw")
set(GLEW_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/sdk/glew")
set(VLC_SDK    "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/sdk/vlc")
set(PDFIUM_SDK "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/sdk/pdfium")
set(HTTPLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/sdk")

# ── 3. ARCHIVOS FUENTE ───────────────────────────────────────────────────────

# -- BACKEND --
file(GLOB BACKEND_CORE_SOURCES      CONFIGURE_DEPENDS "src/backend/core/*.cpp")
file(GLOB BACKEND_MEDIA_SOURCES     CONFIGURE_DEPENDS "src/backend/media/*.cpp")
file(GLOB BACKEND_SETTINGS_SOURCES  CONFIGURE_DEPENDS "src/backend/settings/*.cpp")
file(GLOB BACKEND_LIBRARY_SOURCES   CONFIGURE_DEPENDS "src/backend/library/*.cpp")
file(GLOB BACKEND_CONTENT_SOURCES   CONFIGURE_DEPENDS "src/backend/content/*.cpp")
file(GLOB BACKEND_MONITOR_SOURCES   CONFIGURE_DEPENDS "src/backend/monitors/*.cpp")
file(GLOB BACKEND_SHADERS_SOURCES   CONFIGURE_DEPENDS "src/backend/shaders/*.cpp")
file(GLOB EXTERNAL_TOOLS            CONFIGURE_DEPENDS "src/external/tools/*.cpp")

file(GLOB FRONTEND_UI_BIN_SOURCES   CONFIGURE_DEPENDS "src/frontend/ui/bin/*.cpp")

# -- FRONTEND --
file(GLOB FRONTEND_PANELS_SOURCES   CONFIGURE_DEPENDS "src/frontend/panels/*.cpp")
file(GLOB FRONTEND_CAPTURE_SOURCES  CONFIGURE_DEPENDS "src/frontend/panels/capture/*.cpp")
file(GLOB FRONTEND_LAYERS_SOURCES   CONFIGURE_DEPENDS "src/frontend/panels/layers/*.cpp")
file(GLOB FRONTEND_MONITOR_SOURCES  CONFIGURE_DEPENDS "src/frontend/panels/monitor/*.cpp")
file(GLOB FRONTEND_STYLES_SOURCES   CONFIGURE_DEPENDS "src/frontend/panels/styles/*.cpp")
file(GLOB FRONTEND_VIEWS_SOURCES    CONFIGURE_DEPENDS "src/frontend/views/*.cpp")
file(GLOB FRONTEND_UI_SOURCES       CONFIGURE_DEPENDS "src/frontend/ui/*.cpp")
file(GLOB PREVIEW_DOC_SOURCES       CONFIGURE_DEPENDS "src/frontend/views/documents/*.cpp")
file(GLOB PREVIEW_MON_SOURCES       CONFIGURE_DEPENDS "src/frontend/panels/preview/monitors/*.cpp")
file(GLOB PREVIEW_MON_SOURCES       CONFIGURE_DEPENDS "src/frontend/panels/stb_image_write_impl.cpp")
file(GLOB FRONTEND_BIBLIO_SOURCES   CONFIGURE_DEPENDS "src/frontend/panels/biblio/*.cpp")
# -- RAIZ src/ (main.cpp y similares) --
file(GLOB ROOT_SOURCES              CONFIGURE_DEPENDS "src/*.cpp")

# -- IMGUI --
set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)
# -- AUDIO --
file(GLOB AUDIO_SOURCES             CONFIGURE_DEPENDS "src/frontend/views/audio/*.cpp")
# -- RECURSO WINDOWS --
set(RESOURCE_FILE "src/resources.rc")
set(CMAKE_RC_FLAGS "-O coff --use-temp-file")

# --- COPIA DE RECURSOS AL DIRECTORIO BUILD ---
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/shaders"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

if(EXISTS "${PDFIUM_SDK}/bin/pdfium.dll")
    file(COPY "${PDFIUM_SDK}/bin/pdfium.dll"
         DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endif()

# ── 4. CREACION DEL EJECUTABLE ───────────────────────────────────────────────
add_executable(ProyecThor WIN32
${AUDIO_SOURCES}   
${FRONTEND_BIBLIO_SOURCES}
    ${ROOT_SOURCES}
    ${FRONTEND_UI_BIN_SOURCES}
    ${BACKEND_CORE_SOURCES}
    ${BACKEND_MEDIA_SOURCES}
    ${BACKEND_SETTINGS_SOURCES}
    ${BACKEND_LIBRARY_SOURCES}
    ${BACKEND_CONTENT_SOURCES}
    ${BACKEND_MONITOR_SOURCES}
    ${BACKEND_SHADERS_SOURCES}
    ${FRONTEND_PANELS_SOURCES}
    ${FRONTEND_CAPTURE_SOURCES}
    ${FRONTEND_LAYERS_SOURCES}
    ${FRONTEND_MONITOR_SOURCES}
    ${FRONTEND_STYLES_SOURCES}
    ${FRONTEND_VIEWS_SOURCES}
    ${FRONTEND_UI_SOURCES}
    ${PREVIEW_DOC_SOURCES}
    ${PREVIEW_MON_SOURCES}
    ${EXTERNAL_TOOLS}
    ${IMGUI_SOURCES}
    ${RESOURCE_FILE}
    "${GLEW_DIR}/src/glew.c"
)

# ── 5. DEFINICIONES Y DIRECTORIOS DE INCLUDE ─────────────────────────────────
target_compile_definitions(ProyecThor PRIVATE
    GLEW_STATIC
    GLFW_INCLUDE_NONE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    CPPHTTPLIB_NO_EXCEPTIONS
)

target_include_directories(ProyecThor PRIVATE
    # Version generada
    "${CMAKE_BINARY_DIR}/generated"

    # Raiz src/ para includes absolutos desde src/
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
 "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/views/audio" 
    # BACKEND
    "${CMAKE_CURRENT_SOURCE_DIR}/src/backend"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/media"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/settings"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/library"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/content"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/monitors"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/shaders"

    # FRONTEND
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/capture"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/layers"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/monitor"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/styles"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/preview"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/views/documents"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/preview/monitors"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/views"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/toolbar"
"${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/biblio"
    # EXTERNAS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/external/tools"
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${GLFW_SDK}/include"
    "${GLEW_DIR}/include"
    "${VLC_SDK}/include"
    "${PDFIUM_SDK}/include"

    # httplib — está directamente en src/backend/sdk/httplib.h
    "${HTTPLIB_DIR}"
    # BIN
    "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/ui/bin"
)

# ── 6. LINKEO DE LIBRERIAS ───────────────────────────────────────────────────
set(PDFIUM_LIB "${PDFIUM_SDK}/lib/pdfium.dll.lib")

if(NOT EXISTS "${PDFIUM_LIB}")
    message(FATAL_ERROR
        "No se encontro pdfium.dll.lib en ${PDFIUM_SDK}/lib\n"
        "Verifica que el archivo existe en: ${PDFIUM_LIB}"
    )
endif()

message(STATUS "PDFium encontrado: ${PDFIUM_LIB}")

target_link_libraries(ProyecThor PRIVATE
    nlohmann_json::nlohmann_json
    "${GLFW_SDK}/lib-mingw-w64/libglfw3.a"
    "${VLC_SDK}/lib/libvlc.lib"
    "${VLC_SDK}/lib/libvlccore.lib"
    glm::glm 
    "${PDFIUM_LIB}"
    opengl32
    gdi32
    shell32
    dwmapi
    comdlg32
    winmm
    ws2_32
    iphlpapi
)

if(WIN32)
    target_link_libraries(ProyecThor PRIVATE winhttp)
endif()

# ── 7. OPCIONES DE COMPILACION ────────────────────────────────────────────────
target_link_options(ProyecThor PRIVATE
    "-static"
)
# Copiar DLLs de VLC
if(EXISTS "${VLC_SDK}/libvlc.dll")
    add_custom_command(TARGET ProyecThor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VLC_SDK}/libvlc.dll"
            "$<TARGET_FILE_DIR:ProyecThor>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VLC_SDK}/libvlccore.dll"
            "$<TARGET_FILE_DIR:ProyecThor>"
        COMMENT "Copiando DLLs de VLC al directorio de salida"
    )
endif()

# Copiar carpeta plugins de VLC (necesaria en runtime)
if(EXISTS "${VLC_SDK}/plugins")
    add_custom_command(TARGET ProyecThor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${VLC_SDK}/plugins"
            "$<TARGET_FILE_DIR:ProyecThor>/plugins"
        COMMENT "Copiando plugins de VLC"
    )
endif()
