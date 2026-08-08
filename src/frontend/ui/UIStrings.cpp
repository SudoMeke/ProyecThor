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
            "Archivo", "Salir", "Editar", "Configuraciones...", "Vista",
            "Restablecer Entorno", "Ayuda", "Web", "Donaciones",
            "Acerca de ProyecThor",

            // About
            "Software profesional para gestión de proyecciones.",
            "Sin fines de lucro. Funcionamos mediante donaciones\ndel equipo de desarrollo y la comunidad.",

            // LibraryPanel
            "Canciones", "Videos", "Imágenes", "Biblia", "Documentos",
            "Buscar por nombre o letra...", "Sin reproductor disponible.",

            // BibleView
            "Biblia:", "Libros", "Capítulos",
            "Buscar libro, cap. o vers. (Gn 1:1)", "Limpiar Historial",
            "Error: No se pudo cargar el archivo XML de la Biblia.",
            "Editar", "Guardar en XML",
            "  LIBROS", "  CAPÍTULOS",

            // SongView
            "Tabla de sonidos", "Eliminar letras", "Editar esta canción",

            // MediaView
            "Seleccione un elemento de la biblioteca.",
            "Proyectar Imagen",
            "Video listo: %s",
            "Usa los controles del monitor para proyectar.",

            // TransitionPanel
            "Efectos de Transición",
            "Sin transición",
            "Disolver",
            "Zoom In",
            "Zoom Out",
            "Duración de la transición",
            "Consejo: Las transiciones se aplican al cambiar de estrofa o proyectar nuevo contenido.",

            // DocumentView
            "El documento no tiene páginas generadas.",
            "Documento: %s",
            "Página %d de %d",
            "de",
            "<< Anterior",
            "Siguiente >>",
            "Proyectar Página Actual",

            // MonitorView
            "PREVIEW", "  PREVIEW ",
            "LIVE",   "  ON AIR ",
            "TRANSMITIR##trans",
            "[ Audio desactivado ]",
            "MUTE##lm", "Stp##p",

            // ControlPanel
            "Controles Rápidos",
            "QUITAR LETRA",
            "DETENER VIDEO",
            "Solo se detectó 1 pantalla. Conecta un segundo monitor para proyectar.",
            "Pantallas detectadas: %d",
            "Salida: [%d] %s  (%dx%d)",
            "EMPEZAR PROYECCIÓN",
            "APAGAR PROYECTOR",
            "Proyectando activamente",
            "Proyector inactivo",

            // LayersPanel
            "  Fondos  ", "  Estilos de Letra  ",
            "    Fondos y Videos", "    Recargar    ",
            "Arrastra videos a assets/backgrounds",
            "  + Nuevo Estilo  ", "  Recargar Fuentes  ",
            "Crea tu primer estilo con el botón de arriba",
            "  Ajustes Rápidos (sin guardar)",
            "Fuente", "Color del Texto", "Tamaño  %.0f px",
            "Alineación horizontal", "Alineación vertical",
            "Izq", "Centro", "Der",
            "Arriba", "Centro##v", "Abajo",
            "ACTIVO", "Guardar en XML",

            // OClock (reset reutiliza str.reset = "Restablecer" de Generales)
            "Contadores",
            "Configurar Cuenta Regresiva",
            "Minutos",
            "Segundos",
            "INICIAR",
            "PAUSAR/STOP",
            "Transmitir a Pantalla Principal",

            // QuickNotes
            "Notas Rápidas",
            "Escribe un mensaje para mostrar instantáneamente en pantalla.",
            "Mostrar en Pantalla (F5)",
            "Ocultar Mensaje (ESC)",
            "EN VIVO",

            // SettingsPanel
            "Selecciona el idioma de la interfaz de usuario.",
            "Idioma de la Interfaz",
            "El cambio de idioma se aplica al guardar y reiniciar la aplicación.\nAlgunas cadenas de texto pueden requerir reinicio completo.",
            "Vista Previa de Cadenas",
            "Preferencias",
            "Buscar (Libro Abreviado + 1:1)",
            "Limpiar Historial",

            // Hub
            "Empezar a proyectar",
            "Abrir configuración",
            "Solo el panel de Biblioteca, con Render incluido",
            "Novedades",
            "v%s disponible — tecla N",
            "Descargar subtítulos",
            "Bájalos como .txt desde una URL",
            "Accesos rápidos",
            "Proyecciones totales",
            "FPS promedio",
            "Canción más proyectada",
            "Sin datos aún",
            "Más proyectada (%d)",
            "HISTORIAL DE VERSIONES",
            "Versión v%s",

            // Hub: "Descargar subtitulos"
            "Pega el link de un video. Se buscan sus subtítulos (español primero, si no inglés) y se guardan como un .txt suelto — no crea una canción.",
            "Guardar en",
            "Preguntar cada vez",
            "Carpeta fija",
            "Sin elegir...",
            "Elegir...",
            "Elegir carpeta para subtítulos descargados",
            "Buscando subtítulos...",
            "Descargar",
            "Guardado en: %s",
            "No se pudo escribir el archivo en esa ubicación.",

            // LibrarySongs (Canciones + Playlists)
            "Playlists",
            "Sin playlists todavía",
            "Renombrar",
            "< Volver",
            "%d canción",
            "%d canciones",
            "Esta playlist no tiene canciones todavía",
            "+ Agregar canciones",
            "+ Nueva playlist",
            "Nombre de la playlist",
            "Crear",
            "Nuevo nombre",
            "Agregar canciones",
            "Buscar por título o autor...",
            "Agregada",
            "Agregar a la playlist",
            "Sin resultados",
            "Listo",
            "%d canción encontrada",
            "%d canciones encontradas",
            "Asignar etiqueta",
            "Quitar todas las etiquetas",

            // LibrarySidebar
            "Letra",
            "Medios",
            "Doc"
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
            "File", "Exit", "Edit", "Settings...", "View",
            "Reset Layout", "Help", "Documentation", "Donations",
            "About ProyecThor",

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

            // TransitionPanel
            "Transition Effects",
            "No transition",
            "Dissolve",
            "Zoom In",
            "Zoom Out",
            "Transition duration",
            "Tip: Transitions are applied when changing stanzas or projecting new content.",

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
            "BROADCAST##trans",
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
            "Clear History",

            // Hub
            "Start Projecting",
            "Open Settings",
            "Just the Library panel, Render included",
            "What's New",
            "v%s available — key N",
            "Download Subtitles",
            "Download them as .txt from a URL",
            "Quick Access",
            "Total Projections",
            "Average FPS",
            "Most Projected Song",
            "No data yet",
            "Most projected (%d)",
            "VERSION HISTORY",
            "Version v%s",

            // Hub: "Download Subtitles"
            "Paste a video link. Its subtitles are looked up (Spanish first, then English) and saved as a standalone .txt file — no song is created.",
            "Save to",
            "Ask every time",
            "Fixed folder",
            "Not chosen...",
            "Choose...",
            "Choose folder for downloaded subtitles",
            "Searching for subtitles...",
            "Download",
            "Saved to: %s",
            "Could not write the file to that location.",

            // LibrarySongs (Songs + Playlists)
            "Playlists",
            "No playlists yet",
            "Rename",
            "< Back",
            "%d song",
            "%d songs",
            "This playlist has no songs yet",
            "+ Add Songs",
            "+ New Playlist",
            "Playlist name",
            "Create",
            "New name",
            "Add Songs",
            "Search by title or author...",
            "Added",
            "Add to playlist",
            "No results",
            "Done",
            "%d song found",
            "%d songs found",
            "Assign Tag",
            "Remove All Tags",

            // LibrarySidebar
            "Lyrics",
            "Media",
            "Doc"
        },

        // ─────────────────────────────────────────────────────────────────────
        // [2] PORTUGUES
        // ─────────────────────────────────────────────────────────────────────
        {
            // Generales
            "ProyecThor", "Fechar", "Guardar", "Repor", "Cancelar",
            "Editar", "Eliminar", "Importar", "Novo", "Atualizar", "SEM SINAL",

            // Paneles
            "Biblioteca", "Pré-visualização", "Inspetor de Camadas e Fundos", "Controlo",

            // Menu superior
            "Ficheiro", "Sair", "Editar", "Configurações...", "Ver",
            "Repor Disposição", "Ajuda", "Documentação", "Doações",
            "Sobre ProyecThor",

            // About
            "Software profissional para gestão de projeções.",
            "Sem fins lucrativos. Operamos através de doações\nda equipa de desenvolvimento e comunidade.",

            // LibraryPanel
            "Canções", "Vídeos", "Imagens", "Bíblia", "Documentos",
            "Pesquisar por nome ou letra...", "Sem reprodutor disponível.",

            // BibleView
            "Bíblia:", "Livros", "Capítulos",
            "Pesquisar livro, cap. ou vers. (Gn 1:1)", "Limpar Histórico",
            "Erro: Não foi possível carregar o ficheiro XML da Bíblia.",
            "Editar", "Guardar em XML",
            "  LIVROS", "  CAPÍTULOS",

            // SongView
            "LYRICS DECK", "LIMPAR ECRÃ (CLEAR)", "Editar esta canção...",

            // MediaView
            "Selecione um item da biblioteca.",
            "Projetar Imagem",
            "Vídeo pronto: %s",
            "Use os controlos do monitor para projetar.",

            // TransitionPanel
            "Efeitos de Transição",
            "Sem transição",
            "Dissolver",
            "Zoom In",
            "Zoom Out",
            "Duração da transição",
            "Dica: As transições são aplicadas ao mudar de estrofe ou ao projetar novo conteúdo.",

            // DocumentView
            "O documento não tem páginas geradas.",
            "Documento: %s",
            "Página %d de %d",
            "de",
            "<< Anterior",
            "Seguinte >>",
            "Projetar Página Atual",

            // MonitorView
            "PREVIEW (Só Vídeo)", "  PREVIEW  SEM SINAL",
            "LIVE (Transmissão)", "  ON AIR  SEM SINAL",
            "BROADCAST##trans",
            "[ Áudio desativado ]",
            "MUTE##lm", "Stp##p",

            // ControlPanel
            "Controlos Rápidos",
            "REMOVER LETRA",
            "PARAR VÍDEO",
            "Apenas 1 ecrã detetado. Ligue um segundo monitor para projetar.",
            "Ecrãs detetados: %d",
            "Saída: [%d] %s  (%dx%d)",
            "INICIAR PROJEÇÃO",
            "PARAR PROJETOR",
            "A projetar ativamente",
            "Projetor inativo",

            // LayersPanel
            "  Fundos  ", "  Estilos de Letra  ",
            "    Fundos e Vídeos", "    Recarregar    ",
            "Arraste vídeos para assets/backgrounds",
            "  + Novo Estilo  ", "  Recarregar Fontes  ",
            "Crie o seu primeiro estilo com o botão acima",
            "  Definições Rápidas (não guardado)",
            "Fonte", "Cor do Texto", "Tamanho  %.0f px",
            "Alinhamento horizontal", "Alinhamento vertical",
            "Esq", "Centro", "Dir",
            "Cima", "Centro##v", "Baixo",
            "ATIVO", "Guardar em XML",

            // OClock
            "Relógio e Contadores",
            "Configurar Contagem Decrescente",
            "Minutos",
            "Segundos",
            "INICIAR",
            "PAUSAR/STOP",
            "Transmitir para o Ecrã Principal",

            // QuickNotes
            "Notas Rápidas",
            "Escreva uma mensagem para mostrar instantaneamente no ecrã.",
            "Mostrar no Ecrã (F5)",
            "Ocultar Mensagem (ESC)",
            "EM DIRETO",

            // SettingsPanel
            "Selecione o idioma da interface do utilizador.",
            "Idioma da Interface",
            "A alteração de idioma aplica-se ao guardar e reiniciar.\nAlgumas cadeias de texto podem requerer reinício completo.",
            "Prévia de Cadeias",
            "Preferências",
            "Pesquisar (Livro Abrev. + 1:1)",
            "Limpar Histórico",

            // Hub
            "Iniciar Projeção",
            "Abrir Configurações",
            "Apenas o painel de Biblioteca, com Render incluído",
            "Novidades",
            "v%s disponível — tecla N",
            "Transferir Legendas",
            "Transfira-as como .txt a partir de um URL",
            "Acessos Rápidos",
            "Projeções Totais",
            "FPS Médio",
            "Música Mais Projetada",
            "Ainda sem dados",
            "Mais projetada (%d)",
            "HISTÓRICO DE VERSÕES",
            "Versão v%s",

            // Hub: "Transferir Legendas"
            "Cole o link de um vídeo. As legendas são procuradas (espanhol primeiro, depois inglês) e guardadas como um .txt avulso — não cria uma canção.",
            "Guardar em",
            "Perguntar sempre",
            "Pasta fixa",
            "Não escolhida...",
            "Escolher...",
            "Escolher pasta para legendas transferidas",
            "A procurar legendas...",
            "Transferir",
            "Guardado em: %s",
            "Não foi possível escrever o ficheiro nessa localização.",

            // LibrarySongs (Canções + Playlists)
            "Playlists",
            "Ainda sem playlists",
            "Renomear",
            "< Voltar",
            "%d canção",
            "%d canções",
            "Esta playlist ainda não tem canções",
            "+ Adicionar Canções",
            "+ Nova Playlist",
            "Nome da playlist",
            "Criar",
            "Novo nome",
            "Adicionar Canções",
            "Pesquisar por título ou autor...",
            "Adicionada",
            "Adicionar à playlist",
            "Sem resultados",
            "Concluído",
            "%d canção encontrada",
            "%d canções encontradas",
            "Atribuir Etiqueta",
            "Remover Todas as Etiquetas",

            // LibrarySidebar
            "Letra",
            "Mídia",
            "Doc"
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
