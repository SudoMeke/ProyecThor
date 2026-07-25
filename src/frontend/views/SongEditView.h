#pragma once
#include <string>
#include <vector>
#include <imgui.h>

namespace ProyecThor::UI {

// =============================================================================
//  SongEditView — editor unificado de canciones (rework total del editor).
//
//  Reemplaza tanto SongView::RenderEditorModal (popup flotante viejo, solo
//  Autor + una caja de texto) como LibrarySongs::RenderSongEditor (popup de
//  cancion nueva, Titulo+Autor+Contenido) por una sola pantalla SIN ventana
//  flotante: split izquierda (metadatos + letra) / derecha (preview fiel de
//  las diapositivas), con autoguardado y undo/redo de un solo paso.
//
//  Layout calcado del editor de referencia (Holyrics): Titulo+Autor siempre
//  a la vista (el resto de los metadatos vive detras de un icono de
//  informacion, para no restarle espacio a la letra) + una sola caja de
//  letra continua debajo, y a la derecha una grilla de diapositivas de
//  preview. El estilo/fondo por verso individual se saco a proposito: ya
//  existe un preset de estilo/fondo por CANCION ENTERA (la tarjeta
//  "Ajustes" del grid de estrofas en SongView, ver GetSongStyle/
//  GetSongBackground en LibrarySongs.h) y tener los dos era redundante.
// =============================================================================
class SongEditView {
public:
    SongEditView() = default;

    // Carga letra + metadatos de <filename> (nombre de archivo, con .txt) y
    // reinicia undo/redo. Llamar una vez al entrar al editor (ver
    // SongView::Render, transicion Browse->Edit).
    void Open(const std::string& filename);

    // Renderiza el editor completo. Devuelve false cuando el usuario aprieta
    // "Volver" (SongView debe entonces volver a mostrar la grilla). Hace
    // flush del autoguardado pendiente antes de devolver false.
    bool Render();

    // Fuerza el volcado a disco de cualquier cambio pendiente — llamado por
    // SongView antes de destruir/reusar la instancia (cambio de cancion,
    // cierre de la app) para no perder los ultimos ~1.2s de edicion.
    void FlushIfDirty();

    const std::string& GetFilename() const { return m_Filename; }

private:
    struct EditSnapshot {
        std::string title, author, note, copyright, extra;
        std::string lyrics;

        // Letra "base" (sin el corte de lineas-por-diapositiva aplicado) —
        // se mantiene sincronizada con "lyrics" mientras el usuario escribe
        // a mano, pero NO se toca cuando se aplica el filtro 1/2/3 (ver
        // panel "Excepciones"). Gracias a esto, aplicar "2" y despues "3"
        // siempre parte de la misma letra sin cortes, en vez de intentar
        // re-cortar un texto que ya tiene lineas en blanco insertadas por
        // una aplicacion anterior del filtro (lo que antes impedia UNIR
        // lineas ya separadas, solo separar mas).
        std::string baseLyrics;

        int linesPerSlide = 0; // 0 = centinela legacy (ver LibrarySongMeta.h)
    };

    void PushUndoSnapshot();
    void Undo();
    void Redo();
    void MarkDirty();

    void RenderTopBar(bool& outWantsBack);
    void RenderLeftPane(float width);
    void RenderRightPane(float width);

    std::vector<std::string> ComputePreviewSlides() const;

    std::string m_Filename;   // "Cancion.txt"
    std::string m_FilePath;   // ruta absoluta al .txt

    EditSnapshot m_Current;
    EditSnapshot m_Undo;
    EditSnapshot m_Redo;
    bool m_HasUndo = false;
    bool m_HasRedo = false;

    bool   m_Dirty        = false;
    double m_LastEditTime  = 0.0;
    bool   m_JustSaved     = false;
    double m_JustSavedAt   = 0.0;

    float m_PreviewZoom = 1.0f;
};

} // namespace ProyecThor::UI
