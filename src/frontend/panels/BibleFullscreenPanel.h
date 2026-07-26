#pragma once
#include "IPanel.h"
#include "frontend/views/BibleView.h"
#include <string>

namespace ProyecThor::UI {

// ── BibleFullscreenPanel ─────────────────────────────────────────────────────
// Modo "Biblia" del workspace (ver WorkspaceMode en UIManager.h): el MISMO
// BibleView que ya se usa dentro de Home (RenderHomeContent), pero a
// pantalla completa en vez de apretado en el contenido de Home -- Biblia
// ya se dibuja sola con su propio layout izquierda/derecha (libros/
// capitulos a la izquierda, texto a la derecha, ver BibleView::Render),
// asi que solo hacia falta darle mas lugar. Instancia PROPIA (no la de
// Home): BibleView no depende de ningun servidor/recurso compartido, asi
// que tener dos independientes es seguro (cada una con su propia Biblia
// abierta/historial/favoritos en memoria).
class BibleFullscreenPanel : public IPanel {
public:
    void        Render()  override;
    std::string GetName() const override { return "Biblia"; }

private:
    BibleView m_BibleView;
};

} // namespace ProyecThor::UI
