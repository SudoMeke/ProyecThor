#pragma once
#include <string>
#include <array>

namespace ProyecThor::UI {

    class QuickNotes {
    public:
        QuickNotes();
        ~QuickNotes();

        std::string GetName() const;
        void Render();

    private:
        // Envia el texto del buffer al PresentationCore,
        // identico a SetLayer2_Text() en SongView.
        void PushToCore();

        static const size_t MAX_NOTE_LENGTH = 1024;
        std::array<char, MAX_NOTE_LENGTH> m_TextBuffer{};

        bool m_IsLive;
    };

} // namespace ProyecThor::UI
