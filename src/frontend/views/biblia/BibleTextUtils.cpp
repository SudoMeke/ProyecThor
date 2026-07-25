#include "BibleTextUtils.h"

#include <algorithm>
#include <cctype>

namespace ProyecThor::UI::TextUtils {

std::string ToLowerUTF8(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s)
        out += static_cast<char>(std::tolower(c));
    return out;
}

std::string StripAccents(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) { out += static_cast<char>(c); i++; }
        else if (c == 0xC3 && i + 1 < s.size()) {
            unsigned char n = static_cast<unsigned char>(s[i + 1]);
            char rep = '?';
            if      (n >= 0xA0 && n <= 0xA5) rep = 'a';
            else if (n >= 0xA8 && n <= 0xAB) rep = 'e';
            else if (n >= 0xAC && n <= 0xAF) rep = 'i';
            else if (n >= 0xB2 && n <= 0xB6) rep = 'o';
            else if (n >= 0xB9 && n <= 0xBC) rep = 'u';
            else if (n == 0xB1)              rep = 'n';
            else if (n >= 0x80 && n <= 0x85) rep = 'a';
            else if (n >= 0x88 && n <= 0x8B) rep = 'e';
            else if (n >= 0x8C && n <= 0x8F) rep = 'i';
            else if (n >= 0x92 && n <= 0x96) rep = 'o';
            else if (n >= 0x99 && n <= 0x9C) rep = 'u';
            else if (n == 0x91)              rep = 'n';
            else rep = static_cast<char>(n);
            out += rep; i += 2;
        } else { i++; }
    }
    return out;
}

std::string Normalize(const std::string& s) {
    std::string r = StripAccents(ToLowerUTF8(s));
    r.erase(std::remove(r.begin(), r.end(), ' '), r.end());
    return r;
}

ImU32 Col(float r, float g, float b, float a) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

} // namespace ProyecThor::UI::TextUtils