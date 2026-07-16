#pragma once

namespace ProyecThor::UI {

// Orden fijo: debe coincidir con HomeSidebarSettings::categoryColor y con el
// dispatch de contenido en HomePanel::Render().
enum class HomeSection {
    Home         = 0,
    Clock        = 1,
    Announcements = 2,
    QuickNotes   = 3,
    Capture      = 4,
    Streaming    = 5,
};

// Dibuja el sidebar de iconos de Home (mismo look que LibrarySidebar.cpp) y
// actualiza currentSection al click. No necesita un "context" como el de
// Biblioteca: Home no tiene busqueda/renombrado/playlists que compartir.
void RenderHomeSidebar(HomeSection& currentSection);

} // namespace ProyecThor::UI
