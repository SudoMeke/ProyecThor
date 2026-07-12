#include "BibleQuickNav.h"
#include "BibleTextUtils.h"
#include "BibleBookData.h"
#include "frontend/ui/bin/StyleGeneralApp.h"
#include <cctype>
#include <algorithm> // Requerido para std::clamp

namespace ProyecThor::UI {

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers (Declarados al inicio para evitar errores de ámbito)
// ─────────────────────────────────────────────────────────────────────────────
namespace {

void DrawHint(const std::string& text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.48f, 0.55f, 1.0f));
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
}

void DrawHintIcon(const char* iconName, const std::string& text) {
    auto it = StyleGeneralApp::Icons.find(iconName);
    if (it != StyleGeneralApp::Icons.end() && it->second.textureID) {
        float sz = ImGui::GetTextLineHeight();
        ImGui::Image((ImTextureID)it->second.textureID, ImVec2(sz, sz),
                     ImVec2(0, 0), ImVec2(1, 1),
                     ImVec4(0.45f, 0.48f, 0.55f, 1.0f),   // tint_col
                     ImVec4(0.0f, 0.0f, 0.0f, 0.0f));     // border_col (transparent = no border)
        ImGui::SameLine(0.0f, 6.0f);
    }
    DrawHint(text);
}

} // namespace anonimo

// ─────────────────────────────────────────────────────────────────────────────
//  Open / Close
// ─────────────────────────────────────────────────────────────────────────────

void BibleQuickNav::Open() {
    m_Open          = true;
    m_Step          = QuickNavStep::Book;
    m_OpenSince     = ImGui::GetTime();
    m_OpenedFrame   = ImGui::GetFrameCount();
    m_ClosingUntil  = -1.0;

    m_BookBuffer.clear();
    m_ChapterBuffer.clear();
    m_VerseBuffer.clear();
    m_BookCandidates.clear();
    m_StatusMessage.clear();

    m_Resolution = QuickNavResolution();
}

void BibleQuickNav::Close() {
    m_Open         = false;
    m_ClosingUntil = ImGui::GetTime() + 0.12; // debe coincidir con kFadeOutDuration en Render()
}

void BibleQuickNav::Render(const BibleData& bible) {
    if (!m_Open && m_ClosingUntil < 0.0) return;

    constexpr double kFadeInDuration  = 0.15;
    constexpr double kFadeOutDuration = 0.12;

    double now      = ImGui::GetTime();
    float  progress = 1.0f;

    if (m_Open) {
        double elapsed = now - m_OpenSince;
        progress = (m_OpenSince < 0.0) ? 1.0f
                 : static_cast<float>(std::clamp(elapsed / kFadeInDuration, 0.0, 1.0));
    } else {
        if (now >= m_ClosingUntil) return; // ya cerrado del todo
        double remaining = m_ClosingUntil - now;
        progress = static_cast<float>(std::clamp(remaining / kFadeOutDuration, 0.0, 1.0));
    }

    // Ease-out simple para que no se sienta lineal
    float eased = progress * (2.0f - progress);

    ImGuiViewport* vp = ImGui::GetMainViewport();

    // Fondo semitransparente
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.55f * eased));
    ImGui::Begin("##QuickNavDim", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs);
    ImGui::End();
    ImGui::PopStyleColor();

    ImVec2 cardSize    = ImVec2(440.0f, 260.0f);
    float  slideOffset = (1.0f - eased) * 14.0f; // entra/sale deslizando 14px
    ImVec2 center      = vp->GetCenter();
    
    ImGui::SetNextWindowPos(ImVec2(center.x, center.y + slideOffset), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(cardSize, ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.08f, 0.11f, 0.98f * eased));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.25f, 0.35f, 0.55f, 0.85f * eased));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(22.0f, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,            eased);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav      | ImGuiWindowFlags_NoScrollbar;

    ImGui::Begin("##QuickNavCard", nullptr, kFlags);
    m_CardMin = ImGui::GetWindowPos();
ImVec2 winSize = ImGui::GetWindowSize();
m_CardMax = ImVec2(m_CardMin.x + winSize.x, m_CardMin.y + winSize.y);
{
    ImVec2 winSize = ImGui::GetWindowSize();
    m_CardMax = ImVec2(m_CardMin.x + winSize.x, m_CardMin.y + winSize.y);
}
    // ── Cabecera: breadcrumb con lo ya confirmado ─────────────────────
    if (m_Resolution.hasBook) {
        std::string crumb = bible.books[m_Resolution.bookIdx].name;
        if (m_Resolution.hasChapter)
            crumb += "   >   Capitulo " + std::to_string(m_Resolution.chapterNumber);
        DrawHint(crumb);
    } else {
        DrawHintIcon("searchico", "Buscador rapido");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();

    const char* stepLabel =
        (m_Step == QuickNavStep::Book)    ? "Libro" :
        (m_Step == QuickNavStep::Chapter) ? "Capitulo" : "Versiculo";

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.43f, 0.50f, 1.0f));
    ImGui::TextUnformatted(stepLabel);
    ImGui::PopStyleColor();

    const std::string& currentBuffer =
        (m_Step == QuickNavStep::Book)    ? m_BookBuffer :
        (m_Step == QuickNavStep::Chapter) ? m_ChapterBuffer : m_VerseBuffer;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 1.0f, 1.0f));
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextUnformatted(currentBuffer.empty() ? "_" : currentBuffer.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::Spacing();

    if (m_Step == QuickNavStep::Book) {
        if (!m_BookBuffer.empty()) {
            if (m_BookCandidates.empty()) {
                DrawHint("Ningun libro coincide todavia...");
            } else {
                std::string preview = BibleBooks::GetCanonicalBookName(m_BookCandidates.front());
                if (m_BookCandidates.size() > 1)
                    preview += "  (+" + std::to_string(m_BookCandidates.size() - 1) + " mas, sigue escribiendo)";
                DrawHintIcon("arrow_forward", preview);
            }
        } else {
            DrawHint("Escribe el libro (ej: gn, 1co, salmos) y presiona Enter");
        }
    } else if (m_Step == QuickNavStep::Chapter) {
        const BookData& book = bible.books[m_Resolution.bookIdx];
        if (!book.chapters.empty()) {
            DrawHint("Capitulos disponibles: " + std::to_string(book.chapters.front().number)
                + " - " + std::to_string(book.chapters.back().number)
                + "   (Enter vacio = capitulo " + std::to_string(book.chapters.front().number) + ")");
        }
    } else {
        const ChapterData& chap = bible.books[m_Resolution.bookIdx].chapters[m_Resolution.chapterIdx];
        if (!chap.verses.empty()) {
            DrawHint("Versiculos disponibles: " + std::to_string(chap.verses.front().number)
                + " - " + std::to_string(chap.verses.back().number)
                + "   (Enter vacio = versiculo " + std::to_string(chap.verses.front().number) + ")");
        }
    }

    if (!m_StatusMessage.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.40f, 0.40f, 1.0f));
        ImGui::TextUnformatted(m_StatusMessage.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    DrawHint(m_Step == QuickNavStep::Book
    ? "Enter: confirmar libro    Ctrl+F o clic afuera: cerrar"
    : "Enter: confirmar    Backspace (vacio): paso anterior    Ctrl+F: cerrar");

    ImGui::End();
    ImGui::PopStyleVar(4); // Rounding, Padding, BorderSize, Alpha
    ImGui::PopStyleColor(2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  RefreshBookCandidates
// ─────────────────────────────────────────────────────────────────────────────

void BibleQuickNav::RefreshBookCandidates(const BibleData& bible) {
    m_BookCandidates.clear();
    if (m_BookBuffer.empty() || bible.books.empty()) return;

    std::string norm = TextUtils::Normalize(m_BookBuffer);
    if (norm.empty()) return;

    m_BookCandidates = BibleBooks::FindBookCandidates(norm);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Confirmacion de cada paso
// ─────────────────────────────────────────────────────────────────────────────

void BibleQuickNav::ConfirmBookStep(const BibleData& bible) {
    if (m_BookCandidates.empty()) {
        m_StatusMessage = "Ningun libro coincide con lo escrito";
        return;
    }

    int bestCanonical = m_BookCandidates.front();
    for (int bi = 0; bi < (int)bible.books.size(); bi++) {
        if (bible.books[bi].canonicalNumber == bestCanonical) {
            m_Resolution.bookIdx            = bi;
            m_Resolution.hasBook            = true;
            m_Resolution.bookCandidateCount = (int)m_BookCandidates.size();
            break;
        }
    }

    if (!m_Resolution.hasBook) {
        m_StatusMessage = "Ese libro no esta cargado en esta Biblia";
        return;
    }

    m_StatusMessage.clear();
    m_ChapterBuffer.clear();
    m_Step = QuickNavStep::Chapter;
}

void BibleQuickNav::ConfirmChapterStep(const BibleData& bible) {
    const BookData& book = bible.books[m_Resolution.bookIdx];
    if (book.chapters.empty()) {
        m_StatusMessage = "Este libro no tiene capitulos cargados";
        return;
    }

    int chapNum = 0;
    if (m_ChapterBuffer.empty()) {
        chapNum = book.chapters.front().number;
    } else {
        try { chapNum = std::stoi(m_ChapterBuffer); }
        catch (...) { m_StatusMessage = "Capitulo invalido"; return; }
    }

    for (int ci = 0; ci < (int)book.chapters.size(); ci++) {
        if (book.chapters[ci].number == chapNum) {
            m_Resolution.hasChapter        = true;
            m_Resolution.chapterNumber     = chapNum;
            m_Resolution.chapterIdx        = ci;
            m_Resolution.chapterVerseCount = (int)book.chapters[ci].verses.size();
            break;
        }
    }

    if (!m_Resolution.hasChapter) {
        m_StatusMessage = "Ese capitulo no existe";
        return;
    }

    m_StatusMessage.clear();
    m_VerseBuffer.clear();
    m_Step = QuickNavStep::Verse;
}

bool BibleQuickNav::ConfirmVerseStep(const BibleData& bible) {
    const BookData&    book = bible.books[m_Resolution.bookIdx];
    const ChapterData& chap = book.chapters[m_Resolution.chapterIdx];

    if (chap.verses.empty()) {
        m_StatusMessage = "Este capitulo no tiene versiculos cargados";
        return false;
    }

    int verseNum = 0;
    if (m_VerseBuffer.empty()) {
        verseNum = chap.verses.front().number;
    } else {
        try { verseNum = std::stoi(m_VerseBuffer); }
        catch (...) { m_StatusMessage = "Versiculo invalido"; return false; }
    }

    for (int vi = 0; vi < (int)chap.verses.size(); vi++) {
        if (chap.verses[vi].number == verseNum) {
            m_Resolution.hasVerse    = true;
            m_Resolution.verseNumber = verseNum;
            m_Resolution.verseIdx    = vi;
            return true;
        }
    }

    m_StatusMessage = "Ese versiculo no existe";
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  GoBackStep
// ─────────────────────────────────────────────────────────────────────────────

void BibleQuickNav::GoBackStep(const BibleData& bible) {
    switch (m_Step) {
        case QuickNavStep::Chapter:
            m_Step = QuickNavStep::Book;
            m_Resolution.hasBook    = false;
            m_Resolution.hasChapter = false;
            m_ChapterBuffer.clear();
            m_StatusMessage.clear();
            RefreshBookCandidates(bible);
            break;

        case QuickNavStep::Verse:
            m_Step = QuickNavStep::Chapter;
            m_Resolution.hasVerse = false;
            m_VerseBuffer.clear();
            m_StatusMessage.clear();
            break;

        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────────────────────────
bool BibleQuickNav::Update(const BibleData& bible) {
    ImGuiIO& io = ImGui::GetIO();
    bool hotkeyPressed = io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false);

    if (!m_Open) {
        if (hotkeyPressed && !bible.books.empty())
            Open();
        return false;
    }

    // Si este es el mismo frame en que se abrio, ignoramos el toggle de
    // cierre: es el mismo Ctrl+F que lo abrio, no debe volver a cerrarlo.
    bool justOpenedThisFrame = (ImGui::GetFrameCount() == m_OpenedFrame);

    bool clickOutside = false;
    if (!justOpenedThisFrame && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mp = io.MousePos;
        bool insideCard = mp.x >= m_CardMin.x && mp.x <= m_CardMax.x &&
                          mp.y >= m_CardMin.y && mp.y <= m_CardMax.y;
        if (!insideCard) clickOutside = true;
    }

    if ((hotkeyPressed && !justOpenedThisFrame) || clickOutside) {
        Close();
        return false;
    }

    std::string* activeBuffer =
        (m_Step == QuickNavStep::Book)    ? &m_BookBuffer :
        (m_Step == QuickNavStep::Chapter) ? &m_ChapterBuffer :
                                             &m_VerseBuffer;

    bool bookBufferChanged = false;

    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true)) {
        if (!activeBuffer->empty()) {
            activeBuffer->pop_back();
            if (m_Step == QuickNavStep::Book) bookBufferChanged = true;
            m_StatusMessage.clear();
        } else if (m_Step != QuickNavStep::Book) {
            GoBackStep(bible);
        }
    }

    for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
        ImWchar wc = io.InputQueueCharacters[i];
        if (wc < 32 || wc >= 128) continue;
        char c = (char)wc;

        if (m_Step == QuickNavStep::Book) {
            if (std::isalnum((unsigned char)c) || c == ' ') {
                m_BookBuffer += c;
                bookBufferChanged = true;
                m_StatusMessage.clear();
            }
        } else if (std::isdigit((unsigned char)c)) {
            activeBuffer->push_back(c);
            m_StatusMessage.clear();
        }
    }

    if (bookBufferChanged) RefreshBookCandidates(bible);

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
        switch (m_Step) {
            case QuickNavStep::Book:
                ConfirmBookStep(bible);
                break;
            case QuickNavStep::Chapter:
                ConfirmChapterStep(bible);
                break;
            case QuickNavStep::Verse:
                if (ConfirmVerseStep(bible)) {
                    Close();
                    return true;
                }
                break;
        }
    }

    return false;
}

} // namespace ProyecThor::UI