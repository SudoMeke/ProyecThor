#include "HomeSidebar.h"
#include "HomeIcons.h"
#include "frontend/ui/IconRail.h"
#include "backend/settings/SettingsManager.h"

namespace ProyecThor::UI {

// Wrapper fino sobre el componente compartido IconRail (barra horizontal,
// ver frontend/ui/IconRail.h) — mismo call-site que antes desde HomePanel.cpp,
// ahora sin duplicar el dibujo (que ya vive en LibrarySidebar.cpp/IconRail.cpp).
void RenderHomeSidebar(HomeSection& currentSection)
{
    static const IconRailItem kItems[] = {
        { (int)HomeSection::Home,          HomeIcons::DrawIcon_Home,      "Home"     },
        { (int)HomeSection::Clock,         HomeIcons::DrawIcon_Clock,     "Reloj"    },
        { (int)HomeSection::Announcements, HomeIcons::DrawIcon_Megaphone, "Anuncios" },
        { (int)HomeSection::QuickNotes,    HomeIcons::DrawIcon_Notepad,   "Notas"    },
        { (int)HomeSection::Capture,       HomeIcons::DrawIcon_Camera,    "Captura"  },
        { (int)HomeSection::Streaming,     HomeIcons::DrawIcon_Broadcast, "Red"      },
    };

    const auto& sidebarSettings = ProyecThor::Settings::SettingsManager::Get().GetSettings().homeSidebar;

    int currentIndex = (int)currentSection;
    RenderIconRail(kItems, (int)(sizeof(kItems) / sizeof(kItems[0])), currentIndex,
                   IconRailOrientation::Horizontal, sidebarSettings.categoryColor);
    currentSection = (HomeSection)currentIndex;
}

} // namespace ProyecThor::UI
