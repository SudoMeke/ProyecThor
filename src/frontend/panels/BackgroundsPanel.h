#pragma once
#include "IPanel.h"
#include <string>
#include <vector>
#include <memory>

namespace ProyecThor::UI {

struct BgEntry {
    std::string fullPath;   // ruta absoluta en disco
    std::string name;       // nombre de archivo sin extensión
    std::string ext;        // extensión en minúsculas (.mp4, .png, etc.)
    std::string folder;     // nombre de la subcarpeta relativa (vacío = raíz)
    bool        isImage = false;
};

class LayersBgTab; // Tu clase original que renderiza los fondos

class BackgroundsPanel : public IPanel {
public:
    BackgroundsPanel();
    ~BackgroundsPanel() override;

    void Render() override;
    std::string GetName() const override { return "Fondos"; }

private:
    std::unique_ptr<LayersBgTab> m_BgTab;
};

} // namespace ProyecThor::UI