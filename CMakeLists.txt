cmake_minimum_required(VERSION 3.11)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(ProyecThor
    VERSION 0.5.0
    LANGUAGES C CXX
)

# Sin esto, CMake no aplica ninguna optimizacion por defecto (equivale a
# -O0): el binario resultante corre notablemente mas lento en PCs de bajos
# recursos. Quien necesite un build de debug puede seguir pasando
# -DCMAKE_BUILD_TYPE=Debug explicitamente.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
endif()

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
set(VLC_SDK_LINUX "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/sdk/vlc-linux")
set(PDFIUM_SDK "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/sdk/pdfium")
set(PDFIUM_SDK_LINUX "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/sdk/pdfium-linux")
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
file(GLOB FRONTEND_WINDOWING_SOURCES CONFIGURE_DEPENDS "src/frontend/windowing/*.cpp")
file(GLOB FRONTEND_PANELS_SOURCES   CONFIGURE_DEPENDS "src/frontend/panels/*.cpp")
file(GLOB FRONTEND_CAPTURE_SOURCES  CONFIGURE_DEPENDS "src/frontend/panels/capture/*.cpp")
file(GLOB FRONTEND_LAYERS_SOURCES   CONFIGURE_DEPENDS "src/frontend/panels/layers/*.cpp")
file(GLOB FRONTEND_OVERLAY_SOURCES  CONFIGURE_DEPENDS "src/frontend/panels/overlay/*.cpp")
file(GLOB FRONTEND_MONITOR_SOURCES  CONFIGURE_DEPENDS "src/frontend/panels/monitor/*.cpp")
file(GLOB FRONTEND_STYLES_SOURCES   CONFIGURE_DEPENDS "src/frontend/panels/styles/*.cpp")
file(GLOB FRONTEND_VIEWS_SOURCES    CONFIGURE_DEPENDS "src/frontend/views/*.cpp")
file(GLOB FRONTEND_VIEWS_BIBLIA_SOURCES CONFIGURE_DEPENDS "src/frontend/views/biblia/*.cpp")
file(GLOB FRONTEND_UI_SOURCES       CONFIGURE_DEPENDS "src/frontend/ui/*.cpp")
file(GLOB PREVIEW_DOC_SOURCES       CONFIGURE_DEPENDS "src/frontend/views/documents/*.cpp")
file(GLOB PREVIEW_MON_SOURCES       CONFIGURE_DEPENDS "src/frontend/panels/preview/monitors/*.cpp")
file(GLOB PREVIEW_MON_SOURCES       CONFIGURE_DEPENDS "src/frontend/panels/stb_image_write_impl.cpp")
file(GLOB FRONTEND_BIBLIO_SOURCES   CONFIGURE_DEPENDS "src/frontend/panels/biblio/*.cpp")
file(GLOB FRONTEND_HOME_SOURCES     CONFIGURE_DEPENDS "src/frontend/panels/home/*.cpp")
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

# -- RECURSO WINDOWS (solo aplica en Windows) --
if(WIN32)
    set(RESOURCE_FILE "src/resources.rc")
    set(CMAKE_RC_FLAGS "-O coff --use-temp-file")
else()
    set(RESOURCE_FILE "")
endif()

# --- COPIA DE RECURSOS AL DIRECTORIO BUILD ---
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/src/backend/shaders"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

if(WIN32 AND EXISTS "${PDFIUM_SDK}/bin/pdfium.dll")
    file(COPY "${PDFIUM_SDK}/bin/pdfium.dll"
         DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
endif()

# ── 4. CREACION DEL EJECUTABLE ───────────────────────────────────────────────
# En Windows el ejecutable debe ser WIN32 (subsistema grafico, sin consola).
# En Linux no existe ese concepto, asi que se crea como ejecutable normal.
if(WIN32)
    add_executable(ProyecThor WIN32
        ${AUDIO_SOURCES}
        ${FRONTEND_BIBLIO_SOURCES}
        ${FRONTEND_HOME_SOURCES}
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
        ${FRONTEND_OVERLAY_SOURCES}
        ${FRONTEND_MONITOR_SOURCES}
        ${FRONTEND_STYLES_SOURCES}
        ${FRONTEND_VIEWS_SOURCES}
        ${FRONTEND_VIEWS_BIBLIA_SOURCES}
        ${FRONTEND_UI_SOURCES}
        ${FRONTEND_WINDOWING_SOURCES}
        ${PREVIEW_DOC_SOURCES}
        ${PREVIEW_MON_SOURCES}
        ${EXTERNAL_TOOLS}
        ${IMGUI_SOURCES}
        ${RESOURCE_FILE}
        "${GLEW_DIR}/src/glew.c"
    )
else()
    add_executable(ProyecThor
        ${AUDIO_SOURCES}
        ${FRONTEND_BIBLIO_SOURCES}
        ${FRONTEND_HOME_SOURCES}
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
        ${FRONTEND_OVERLAY_SOURCES}
        ${FRONTEND_MONITOR_SOURCES}
        ${FRONTEND_STYLES_SOURCES}
        ${FRONTEND_VIEWS_SOURCES}
        ${FRONTEND_VIEWS_BIBLIA_SOURCES}
        ${FRONTEND_UI_SOURCES}
        ${FRONTEND_WINDOWING_SOURCES}
        ${PREVIEW_DOC_SOURCES}
        ${PREVIEW_MON_SOURCES}
        ${EXTERNAL_TOOLS}
        ${IMGUI_SOURCES}
        "${GLEW_DIR}/src/glew.c"
    )
endif()

# ── 5. DEFINICIONES Y DIRECTORIOS DE INCLUDE ─────────────────────────────────
target_compile_definitions(ProyecThor PRIVATE
    GLEW_STATIC
    GLEW_NO_GLU
    GLFW_INCLUDE_NONE
    CPPHTTPLIB_NO_EXCEPTIONS
)

if(WIN32)
    target_compile_definitions(ProyecThor PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

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
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/windowing"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/capture"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/layers"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/overlay"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/monitor"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/styles"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/preview"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/views/documents"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/preview/monitors"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/views"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/toolbar"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/biblio"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/panels/home"
    # EXTERNAS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/external/tools"
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${GLFW_SDK}/include"
    "${GLEW_DIR}/include"
    "${VLC_SDK}/include"

    # httplib — está directamente en src/backend/sdk/httplib.h
    "${HTTPLIB_DIR}"
    # BIN
    "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/ui/bin"
)

# El include de PDFium y VLC cambia segun la plataforma
if(WIN32)
    target_include_directories(ProyecThor PRIVATE "${PDFIUM_SDK}/include")
else()
    target_include_directories(ProyecThor PRIVATE
        "${PDFIUM_SDK_LINUX}/include"
        "${VLC_SDK_LINUX}/include"
    )
endif()

# ── 6. LINKEO DE LIBRERIAS ───────────────────────────────────────────────────

target_link_libraries(ProyecThor PRIVATE
    nlohmann_json::nlohmann_json
    glm::glm
)

if(WIN32)
    # ── Rutas y librerias exclusivas de Windows ──────────────────────────────
    set(PDFIUM_LIB "${PDFIUM_SDK}/lib/pdfium.dll.lib")

    if(NOT EXISTS "${PDFIUM_LIB}")
        message(FATAL_ERROR
            "No se encontro pdfium.dll.lib en ${PDFIUM_SDK}/lib\n"
            "Verifica que el archivo existe en: ${PDFIUM_LIB}"
        )
    endif()

    message(STATUS "PDFium encontrado (Windows): ${PDFIUM_LIB}")

    target_link_libraries(ProyecThor PRIVATE
        "${GLFW_SDK}/lib-mingw-w64/libglfw3.a"
        "${VLC_SDK}/lib/libvlc.lib"
        "${VLC_SDK}/lib/libvlccore.lib"
        "${PDFIUM_LIB}"
        opengl32
        gdi32
        shell32
        dwmapi
        comdlg32
        winmm
        ws2_32
        iphlpapi
        winhttp
        d3d11
        dxgi
    )

    target_link_options(ProyecThor PRIVATE
        "-static"
    )

else()
    # ── Rutas y librerias exclusivas de Linux ────────────────────────────────

    find_package(glfw3 QUIET)
    if(NOT glfw3_FOUND)
        message(FATAL_ERROR
            "No se encontro GLFW3 en el sistema.\n"
            "Instala el paquete de desarrollo de GLFW3 para tu distribucion.\n"
            "En CachyOS/Arch: sudo pacman -S glfw\n"
            "En Debian/Ubuntu: sudo apt install libglfw3-dev"
        )
    endif()

    find_package(OpenGL REQUIRED)

    # Camaras reales en el panel de Captura: CapturePanel.cpp ya tiene la
    # enumeracion/lectura real via cv::VideoCapture (V4L2 por debajo) detras
    # de la macro PT_USE_OPENCV -- pero como nada definia esa macro nunca se
    # compilaba, y el panel caia siempre en el stub de "Camara 0 (simulada)"
    # con un patron de prueba animado en vez de listar dispositivos reales.
    # Opcional, igual que XComposite/PipeWire arriba/abajo: si OpenCV no
    # esta instalado, el build sigue igual que antes (stub simulado).
    find_package(OpenCV QUIET COMPONENTS core videoio)
    if(OpenCV_FOUND)
        message(STATUS "OpenCV encontrado (${OpenCV_VERSION}): deteccion real de camaras habilitada en Captura")
        target_link_libraries(ProyecThor PRIVATE ${OpenCV_LIBS})
        target_include_directories(ProyecThor PRIVATE ${OpenCV_INCLUDE_DIRS})
        target_compile_definitions(ProyecThor PRIVATE PT_USE_OPENCV)
    else()
        message(STATUS "OpenCV NO encontrado: Captura seguira usando la camara simulada de prueba")
    endif()

    # VLC para Linux: build .so vendorizado, igual que PDFium (no se
    # compila VLC desde cero, se linkea contra binarios ya generados en
    # src/backend/sdk/vlc-linux/lib/ y src/backend/sdk/vlc-linux/include/)
    set(VLC_LIB_LINUX "${VLC_SDK_LINUX}/lib/libvlc.so")

    if(NOT EXISTS "${VLC_LIB_LINUX}")
        message(FATAL_ERROR
            "No se encontro libvlc.so en ${VLC_SDK_LINUX}/lib\n"
            "Coloca ahi los .so de VLC (libvlc.so, libvlccore.so),\n"
            "la carpeta plugins/ y sus headers en:\n"
            "${VLC_SDK_LINUX}/include"
        )
    endif()

    message(STATUS "libVLC encontrado (Linux): ${VLC_LIB_LINUX}")

    # PDFium para Linux: debe existir un build .so colocado manualmente
    set(PDFIUM_LIB_LINUX "${PDFIUM_SDK_LINUX}/lib/libpdfium.so")

    if(NOT EXISTS "${PDFIUM_LIB_LINUX}")
        message(FATAL_ERROR
            "No se encontro libpdfium.so en ${PDFIUM_SDK_LINUX}/lib\n"
            "Descarga el build de PDFium para Linux y colocalo en:\n"
            "${PDFIUM_SDK_LINUX}/lib/libpdfium.so\n"
            "Y sus headers en:\n"
            "${PDFIUM_SDK_LINUX}/include"
        )
    endif()

    message(STATUS "PDFium encontrado (Linux): ${PDFIUM_LIB_LINUX}")

    target_link_libraries(ProyecThor PRIVATE
        glfw
        OpenGL::GL
        "${VLC_LIB_LINUX}"
        "${PDFIUM_LIB_LINUX}"
        pthread
        dl
    )
endif()

# ── 7. COPIA DE ARCHIVOS EN RUNTIME (solo Windows) ───────────────────────────

if(WIN32)
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
endif()

# ── 7b. COPIA DE extrabuild/ AL DIRECTORIO DE SALIDA (Windows y Linux) ───────
# Contiene binarios/recursos que hoy pegás a mano (bin/, lua/, plugins/,
# libvlc.dll, pdfium.dll, iconos, splash screens, yt-dlp.exe, etc). Se copia
# entero al lado del ejecutable cada vez que se compila.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extrabuild")
    message(STATUS "extrabuild/ encontrado, se copiara al build")
    add_custom_command(TARGET ProyecThor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/extrabuild"
            "$<TARGET_FILE_DIR:ProyecThor>"
        COMMENT "Copiando extrabuild/ al directorio de salida"
    )
else()
    message(WARNING "extrabuild/ NO encontrado en ${CMAKE_CURRENT_SOURCE_DIR}/extrabuild")
endif()

# ── 7c. COPIA DE libpdfium.so Y libvlc.so AL DIRECTORIO DE BUILD (Linux) ─────
# Sin esto, el ejecutable de build-linux/ no encuentra las librerias hasta
# que se hace "install" (que si las copia a /usr/lib/proyecthor).
if(NOT WIN32)
    add_custom_command(TARGET ProyecThor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PDFIUM_SDK_LINUX}/lib/libpdfium.so"
            "$<TARGET_FILE_DIR:ProyecThor>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${VLC_SDK_LINUX}/lib/libvlc.so"
            "$<TARGET_FILE_DIR:ProyecThor>"
        COMMENT "Copiando libpdfium.so y libvlc.so al directorio de build"
    )

    if(EXISTS "${VLC_SDK_LINUX}/plugins")
        add_custom_command(TARGET ProyecThor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${VLC_SDK_LINUX}/plugins"
                "$<TARGET_FILE_DIR:ProyecThor>/plugins"
            COMMENT "Copiando plugins de VLC (Linux) al directorio de build"
        )
    endif()
endif()


if(NOT WIN32)
    include(GNUInstallDirs)

    set(PROYECTHOR_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/proyecthor")

    # RPATH relativo al propio ejecutable: encuentra libpdfium.so/libvlc.so
    # en la misma carpeta sin necesidad de LD_LIBRARY_PATH.
    set_target_properties(ProyecThor PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
        SKIP_BUILD_RPATH FALSE
    )

    # Binario real (no directamente en PATH, lo llama el wrapper de abajo)
    install(TARGETS ProyecThor
        RUNTIME DESTINATION ${PROYECTHOR_INSTALL_DIR}
    )

    # libpdfium.so vive al lado del binario
    install(FILES "${PDFIUM_SDK_LINUX}/lib/libpdfium.so"
        DESTINATION ${PROYECTHOR_INSTALL_DIR}
    )

    # libvlc.so vive al lado del binario
    install(FILES "${VLC_SDK_LINUX}/lib/libvlc.so"
        DESTINATION ${PROYECTHOR_INSTALL_DIR}
    )

    # Plugins de VLC (necesarios en runtime via VLC_PLUGIN_PATH)
    if(EXISTS "${VLC_SDK_LINUX}/plugins")
        install(DIRECTORY "${VLC_SDK_LINUX}/plugins"
            DESTINATION ${PROYECTHOR_INSTALL_DIR}
        )
    endif()

    # Shaders (misma carpeta que ya se copia al lado del binario en build/)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        DESTINATION ${PROYECTHOR_INSTALL_DIR}
    )

    # Wrapper ejecutable en /usr/bin: hace cd a la carpeta real, exporta
    # VLC_PLUGIN_PATH (para que libvlc encuentre sus plugins) y ejecuta el
    # binario desde ahi, para que las rutas relativas (shaders/, etc) sigan
    # resolviendo igual que en desarrollo, sin importar el cwd del usuario.
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/proyecthor-wrapper.sh"
"#!/bin/sh
cd \"${CMAKE_INSTALL_PREFIX}/${PROYECTHOR_INSTALL_DIR}\" || exit 1
export VLC_PLUGIN_PATH=\"${CMAKE_INSTALL_PREFIX}/${PROYECTHOR_INSTALL_DIR}/plugins\"
exec ./ProyecThor \"\$@\"
")
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/proyecthor-wrapper.sh"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME proyecthor
    )

    # Icono y entrada de menu (.desktop) — opcionales, solo si existen los
    # archivos en packaging/. Si todavia no tenes un icono, comenta estas
    # dos lineas de install() de abajo.
    # Recursos de extrabuild/ (splash screens, iconos, config, lua/, bin/)
    # excluyendo binarios exclusivos de Windows.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extrabuild")
        message(STATUS "extrabuild/ encontrado, se incluira en el paquete")
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/extrabuild/"
            DESTINATION ${PROYECTHOR_INSTALL_DIR}
            PATTERN "*.dll" EXCLUDE
            PATTERN "*.exe" EXCLUDE
            PATTERN "pasteinbuild.txt" EXCLUDE
        )
    else()
        message(WARNING "extrabuild/ NO encontrado, el paquete no tendra iconos/splash/config")
    endif()
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(ProyecThor PRIVATE ${X11_LIBRARIES})
    target_include_directories(ProyecThor PRIVATE ${X11_INCLUDE_DIR})

    # XComposite: opcional. Sin ella, la captura de ventanas (CapturePanel)
    # lee XGetImage directo sobre la ventana, que devuelve contenido
    # indefinido/basura en las zonas tapadas por otras ventanas. Si esta
    # disponible, la usamos para leer el pixmap ya compuesto por el
    # servidor X y capturar bien aunque la ventana este solapada. Si no
    # esta instalada (dev headers ausentes), el build sigue igual que
    # antes — no es un requisito duro.
    if(X11_Xcomposite_FOUND)
        message(STATUS "libXcomposite encontrada: captura de ventanas correcta bajo solapamiento")
        target_link_libraries(ProyecThor PRIVATE ${X11_Xcomposite_LIB})
        target_include_directories(ProyecThor PRIVATE ${X11_Xcomposite_INCLUDE_PATH})
        target_compile_definitions(ProyecThor PRIVATE PT_HAVE_XCOMPOSITE)
    else()
        message(STATUS "libXcomposite NO encontrada: captura de ventanas usara XGetImage directo (puede fallar si hay solapamiento)")
    endif()

    # Captura de pantalla/ventana en sesiones Wayland: XGetImage/XComposite
    # (arriba) no sirven ahi -- el compositor Wayland no vuelca contenido
    # real a la "ventana raiz" X11 heredada, es una restriccion de
    # seguridad del protocolo. El camino nativo es el portal de escritorio
    # (org.freedesktop.portal.ScreenCast) + PipeWire para consumir el
    # stream. Opcional: sin estas dos libs, el build sigue igual que hoy y
    # la captura de pantalla queda limitada a X11 puro.
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(PIPEWIRE libpipewire-0.3)
        pkg_check_modules(DBUS dbus-1)
    endif()
    if(PIPEWIRE_FOUND AND DBUS_FOUND)
        message(STATUS "libpipewire-0.3 y dbus-1 encontradas: captura de pantalla/ventana via portal Wayland habilitada")
        target_link_libraries(ProyecThor PRIVATE ${PIPEWIRE_LIBRARIES} ${DBUS_LIBRARIES})
        target_include_directories(ProyecThor PRIVATE ${PIPEWIRE_INCLUDE_DIRS} ${DBUS_INCLUDE_DIRS})
        target_compile_definitions(ProyecThor PRIVATE PT_HAVE_WAYLAND_CAPTURE)
    else()
        message(STATUS "libpipewire-0.3 y/o dbus-1 NO encontradas: sin captura de pantalla/ventana nativa en Wayland")
    endif()
endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/proyecthor.png")
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/proyecthor.png"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        )
    endif()
endif()