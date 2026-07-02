#include "UIStrings.h"
#include "settings/SettingsManager.h"

namespace ProyecThor::UI {

    static const UIStrings k_Strings[] = {

        // ─────────────────────────────────────────────────────────────────────
        // [0] ESPAÑOL
        // ─────────────────────────────────────────────────────────────────────
        {
            // Generales
            "ProyecThor", "Cerrar", "Guardar", "Restablecer", "Cancelar",
            "Editar", "Eliminar", "Importar", "Nuevo", "Actualizar", "Sin señal",

            // Paneles
            "Biblioteca", "Preview", "Inspector de Capas y Fondos", "Control",

            // Menu superior
            "Archivo", "Salir", "Editar", "Preferencias...", "Vista",
            "Restablecer Entorno", "Ayuda", "Web", "Donaciones",
            "Acerca de ProyecThor",

            // About
            "Software profesional para gestion de proyecciones.",
            "Sin fines de lucro. Funcionamos mediante donaciones\ndel equipo de desarrollo y la comunidad.",
// TransitionPanel
        "Efectos de Transicion",
        "Sin transicion",
        "Disolver",
        "Zoom In",
        "Zoom Out",
        "Duracion de la transicion",
        "Tip: Las transiciones se aplican al cambiar de estrofa o proyectar nuevo contenido.",
    
            // LibraryPanel
            "Canciones", "Videos", "Imagenes", "Biblia", "Documentos",
            "Buscar por nombre o letra...", "Sin reproductor disponible.",

            // BibleView
            "Biblia:", "Libros", "Capitulos",
            "Buscar libro, cap. o vers. (Gn 1:1)", "Limpiar Historial",
            "Error: No se pudo cargar el archivo XML de la Biblia.",
            "Editar", "Guardar en XML",
            "  LIBROS", "  CAPITULOS",

            // SongView
            "Tabla de sonidos", "Eliminar letras", "Editar esta canción",

            // MediaView
            "Seleccione un elemento de la biblioteca.",
            "Proyectar Imagen",
            "Video listo: %s",
            "Usa los controles del monitor para proyectar.",

            // DocumentView
            "El documento no tiene paginas generadas.",
            "Documento: %s",
            "Pagina %d de %d",
            "de",
            "<< Anterior",
            "Siguiente >>",
            "Proyectar Pagina Actual",

            // MonitorView
            "PREVIEW", "  PREVIEW ",
            "LIVE",   "  ON AIR ",
            "TRANSMITIR##trans",
            "[ Audio desactivado ]",
            "MUTE##lm", "Stp##p",

            // ControlPanel
            "Controles Rapidos",
            "QUITAR LETRA",
            "DETENER VIDEO",
            "Solo se detecto 1 pantalla. Conecta un segundo monitor para proyectar.",
            "Pantallas detectadas: %d",
            "Salida: [%d] %s  (%dx%d)",
            "EMPEZAR PROYECCION",
            "APAGAR PROYECTOR",
            "Proyectando activamente",
            "Proyector inactivo",

            // LayersPanel
            "  Fondos  ", "  Estilos de Letra  ",
            "    Fondos y Videos", "    Recargar    ",
            "Arrastra videos a assets/backgrounds",
            "  + Nuevo Estilo  ", "  Recargar Fuentes  ",
            "Crea tu primer estilo con el boton de arriba",
            "  Ajustes Rapidos (sin guardar)",
            "Fuente", "Color del Texto", "Tamanio  %.0f px",
            "Alineacion horizontal", "Alineacion vertical",
            "Izq", "Centro", "Der",
            "Arriba", "Centro##v", "Abajo",
            "ACTIVO", "Guardar en XML",

            // OClock (reset reutiliza str.reset = "Restablecer" de Generales)
            "Reloj y Contadores",
            "Configurar Cuenta Regresiva",
            "Minutos",
            "Segundos",
            "INICIAR",
            "PAUSAR/STOP",
            "Transmitir a Pantalla Principal",

            // QuickNotes
            "Notas Rapidas",
            "Escribe un mensaje para mostrar instantaneamente en pantalla.",
            "Mostrar en Pantalla (F5)",
            "Ocultar Mensaje (ESC)",
            "EN VIVO",

            // SettingsPanel
            "Selecciona el idioma de la interfaz de usuario.",
            "Idioma de la Interfaz",
            "El cambio de idioma se aplica al guardar y reiniciar la aplicacion.\nAlgunas cadenas de texto pueden requerir reinicio completo.",
            "Vista Previa de Cadenas",
            "Preferencias",
            "Buscar (Libro Abreviado + 1:1)",
            "Limpiar Historial"
        },

        // ─────────────────────────────────────────────────────────────────────
        // [1] ENGLISH
        // ─────────────────────────────────────────────────────────────────────
        {
            // Generales
            "ProyecThor", "Close", "Save", "Reset", "Cancel",
            "Edit", "Delete", "Import", "New", "Refresh", "NO SIGNAL",

            // Paneles
            "Library", "Preview", "Layers & Backgrounds Inspector", "Control",

            // Menu superior
            "File", "Exit", "Edit", "Preferences...", "View",
            "Reset Layout", "Help", "Documentation", "Donations",
            "About ProyecThor",
// TransitionPanel
        "Transition Effects",
        "No transition",
        "Dissolve",
        "Zoom In",
        "Zoom Out",
        "Transition duration",
        "Tip: Transitions are applied when changing stanzas or projecting new content.",
            // About
            "Professional software for projection management.",
            "Non-profit. We operate through donations\nfrom the development team and community.",

            // LibraryPanel
            "Songs", "Videos", "Images", "Bible", "Documents",
            "Search by name or lyrics...", "No player available.",

            // BibleView
            "Bible:", "Books", "Chapters",
            "Search book, ch. or verse (Gen 1:1)", "Clear History",
            "Error: Could not load the Bible XML file.",
            "Edit", "Save to XML",
            "  BOOKS", "  CHAPTERS",

            // SongView
            "LYRICS DECK", "CLEAR SCREEN (CLEAR)", "Edit this song...",

            // MediaView
            "Select an item from the library.",
            "Project Image",
            "Video ready: %s",
            "Use the monitor controls to project.",

            // DocumentView
            "The document has no generated pages.",
            "Document: %s",
            "Page %d of %d",
            "of",
            "<< Previous",
            "Next >>",
            "Project Current Page",

            // MonitorView
            "PREVIEW (Video Only)", "  PREVIEW  NO SIGNAL",
            "LIVE (Broadcast)",     "  ON AIR  NO SIGNAL",
            "TRANSMITIR##trans",
            "[ Audio disabled ]",
            "MUTE##lm", "Stp##p",

            // ControlPanel
            "Quick Controls",
            "REMOVE TEXT",
            "STOP VIDEO",
            "Only 1 screen detected. Connect a second monitor to project.",
            "Screens detected: %d",
            "Output: [%d] %s  (%dx%d)",
            "START PROJECTION",
            "STOP PROJECTOR",
            "Projecting actively",
            "Projector inactive",

            // LayersPanel
            "  Backgrounds  ", "  Letter Styles  ",
            "    Backgrounds & Videos", "    Reload    ",
            "Drag videos to assets/backgrounds",
            "  + New Style  ", "  Reload Fonts  ",
            "Create your first style with the button above",
            "  Quick Settings (unsaved)",
            "Font", "Text Color", "Size  %.0f px",
            "Horizontal alignment", "Vertical alignment",
            "Left", "Center", "Right",
            "Top", "Middle##v", "Bottom",
            "ACTIVE", "Save to XML",

            // OClock
            "Clock & Timers",
            "Countdown Setup",
            "Minutes",
            "Seconds",
            "START",
            "PAUSE/STOP",
            "Broadcast to Main Screen",

            // QuickNotes
            "Quick Notes",
            "Type a message to display instantly on screen.",
            "Show on Screen (F5)",
            "Hide Message (ESC)",
            "LIVE",

            // SettingsPanel
            "Select the user interface language.",
            "Interface Language",
            "Language change applies after saving and restarting.\nSome strings may require a full restart.",
            "String Preview",
            "Preferences",
            "Search (Abbrev. Book + 1:1)",
            "Clear History"
        },

        // ─────────────────────────────────────────────────────────────────────
        // [2] PORTUGUES
        // ─────────────────────────────────────────────────────────────────────
        {
            // Generales
            "ProyecThor", "Fechar", "Guardar", "Repor", "Cancelar",
            "Editar", "Eliminar", "Importar", "Novo", "Atualizar", "SEM SINAL",

            // Paneles
            "Biblioteca", "Pre-visualizacao", "Inspetor de Camadas e Fundos", "Controlo",

            // Menu superior
            "Ficheiro", "Sair", "Editar", "Preferencias...", "Ver",
            "Repor Disposicao", "Ajuda", "Documentacao", "Doacoes",
            "Sobre ProyecThor",

            // About
            "Software profissional para gestao de projecoes.",
            "Sem fins lucrativos. Operamos atraves de doacoes\nda equipa de desenvolvimento e comunidade.",

            // LibraryPanel
            "Cancoes", "Videos", "Imagens", "Biblia", "Documentos",
            "Pesquisar por nome ou letra...", "Sem reprodutor disponivel.",
// TransitionPanel
        "Efeitos de Transicao",
        "Sem transicao",
        "Dissolver",
        "Zoom In",
        "Zoom Out",
        "Duracao da transicao",
        "Dica: As transicoes sao aplicadas ao mudar de estrofe ou ao projetar novo conteudo.",
            // BibleView
            "Biblia:", "Livros", "Capitulos",
            "Pesquisar livro, cap. ou vers. (Gn 1:1)", "Limpar Historico",
            "Erro: Nao foi possivel carregar o ficheiro XML da Biblia.",
            "Editar", "Guardar em XML",
            "  LIVROS", "  CAPITULOS",

            // SongView
            "LYRICS DECK", "LIMPAR ECRA (CLEAR)", "Editar esta cancao...",

            // MediaView
            "Selecione um item da biblioteca.",
            "Projetar Imagem",
            "Video pronto: %s",
            "Use os controlos do monitor para projetar.",

            // DocumentView
            "O documento nao tem paginas geradas.",
            "Documento: %s",
            "Pagina %d de %d",
            "de",
            "<< Anterior",
            "Seguinte >>",
            "Projetar Pagina Atual",

            // MonitorView
            "PREVIEW (So Video)", "  PREVIEW  SEM SINAL",
            "LIVE (Transmissao)", "  ON AIR  SEM SINAL",
            "TRANSMITIR##trans",
            "[ Audio desativado ]",
            "MUTE##lm", "Stp##p",

            // ControlPanel
            "Controlos Rapidos",
            "REMOVER LETRA",
            "PARAR VIDEO",
            "Apenas 1 ecra detetado. Conecte um segundo monitor para projetar.",
            "Ecras detetados: %d",
            "Saida: [%d] %s  (%dx%d)",
            "INICIAR PROJECAO",
            "PARAR PROJETOR",
            "A projetar ativamente",
            "Projetor inativo",

            // LayersPanel
            "  Fundos  ", "  Estilos de Letra  ",
            "    Fundos e Videos", "    Recarregar    ",
            "Arraste videos para assets/backgrounds",
            "  + Novo Estilo  ", "  Recarregar Fontes  ",
            "Crie o seu primeiro estilo com o botao acima",
            "  Definicoes Rapidas (nao guardado)",
            "Fonte", "Cor do Texto", "Tamanho  %.0f px",
            "Alinhamento horizontal", "Alinhamento vertical",
            "Esq", "Centro", "Dir",
            "Cima", "Centro##v", "Baixo",
            "ATIVO", "Guardar em XML",

            // OClock
            "Relogio e Contadores",
            "Configurar Contagem Decrescente",
            "Minutos",
            "Segundos",
            "INICIAR",
            "PAUSAR/STOP",
            "Transmitir para o Ecra Principal",

            // QuickNotes
            "Notas Rapidas",
            "Escreva uma mensagem para mostrar instantaneamente no ecra.",
            "Mostrar no Ecra (F5)",
            "Ocultar Mensagem (ESC)",
            "EM DIRETO",

            // SettingsPanel
            "Selecione o idioma da interface do utilizador.",
            "Idioma da Interface",
            "A alteracao de idioma aplica-se ao guardar e reiniciar.\nAlgumas cadeias de texto podem requerer reinicio completo.",
            "Previa de Cadeias",
            "Preferencias",
            "Pesquisar (Livro Abrev. + 1:1)",
            "Limpar Historico"
        }
    };

    const UIStrings& GetUIStrings() {
        int idx = static_cast<int>(
            ProyecThor::Settings::SettingsManager::Get().GetSettings().general.language
        );
        if (idx < 0 || idx >= 3) idx = 0;
        return k_Strings[idx];
    }

} // namespace ProyecThor::UI
