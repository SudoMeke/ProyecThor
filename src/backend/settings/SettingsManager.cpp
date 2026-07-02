#include "SettingsManager.h"
#include "backend/core/PresentationCore.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>

using json = nlohmann::json;

namespace ProyecThor::Settings {

// Devuelve la ruta completa al archivo dentro de %APPDATA%/ProyecThor/
// y crea el directorio si no existe.
static std::string GetSettingsPath() {
    const char* appData = std::getenv("APPDATA");
    if (!appData) {
        // Fallback: directorio de trabajo actual
        return "settings.json";
    }

    std::filesystem::path dir = std::filesystem::path(appData) / "ProyecThor";
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    return (dir / "settings.json").string();
}

void SettingsManager::ApplyProjection() {
    const auto& p    = m_Settings.projection;
    auto&       core = ::ProyecThor::Core::PresentationCore::Get();

    core.SetTargetMonitor(p.targetMonitor);

    float tc[4]      = { p.textColorR, p.textColorG, p.textColorB, p.textColorA };
    float margins[4] = { p.marginTop,  p.marginBottom, p.marginLeft, p.marginRight };

    core.UpdateTextStyle(
        p.textSize,
        tc,
        p.textAlignment,
        p.vAlignment,
        margins,
        p.autoScale,
        p.selectedFont);

    core.SetLayer0_Color(p.defaultBgR, p.defaultBgG, p.defaultBgB);
}

void SettingsManager::SaveSettings() {
    json j;

    j["projection"]["targetMonitor"] = m_Settings.projection.targetMonitor;
    j["projection"]["textSize"]      = m_Settings.projection.textSize;
    j["projection"]["textColorR"]    = m_Settings.projection.textColorR;
    j["projection"]["textColorG"]    = m_Settings.projection.textColorG;
    j["projection"]["textColorB"]    = m_Settings.projection.textColorB;
    j["projection"]["textColorA"]    = m_Settings.projection.textColorA;
    j["projection"]["textAlignment"] = m_Settings.projection.textAlignment;
    j["projection"]["vAlignment"]    = m_Settings.projection.vAlignment;
    j["projection"]["marginTop"]     = m_Settings.projection.marginTop;
    j["projection"]["marginBottom"]  = m_Settings.projection.marginBottom;
    j["projection"]["marginLeft"]    = m_Settings.projection.marginLeft;
    j["projection"]["marginRight"]   = m_Settings.projection.marginRight;
    j["projection"]["autoScale"]     = m_Settings.projection.autoScale;
    j["projection"]["selectedFont"]  = m_Settings.projection.selectedFont;
    j["projection"]["defaultBgR"]    = m_Settings.projection.defaultBgR;
    j["projection"]["defaultBgG"]    = m_Settings.projection.defaultBgG;
    j["projection"]["defaultBgB"]    = m_Settings.projection.defaultBgB;

    std::string langStr = "es";
    if      (m_Settings.general.language == Language::English)    langStr = "en";
    else if (m_Settings.general.language == Language::Portuguese)  langStr = "pt";

    j["general"]["language"]           = langStr;
    j["general"]["dismissedChangelog"] = m_Settings.general.dismissedChangelog;
    j["general"]["startMinimized"]     = m_Settings.general.startMinimized;
    j["general"]["rememberLayout"]     = m_Settings.general.rememberLayout;
    j["general"]["confirmOnExit"]      = m_Settings.general.confirmOnExit;
    j["general"]["autoSave"]           = m_Settings.general.autoSave;
    j["general"]["autoSaveIntervalSec"] = m_Settings.general.autoSaveIntervalSec;
    j["general"]["defaultBiblesFolder"] = m_Settings.general.defaultBiblesFolder;
    j["general"]["defaultMediaFolder"]  = m_Settings.general.defaultMediaFolder;

    j["audio"]["masterVolume"]     = m_Settings.audio.masterVolume;
    j["audio"]["muted"]            = m_Settings.audio.muted;
    j["audio"]["muteOnBlank"]      = m_Settings.audio.muteOnBlank;
    j["audio"]["audioDevice"]      = m_Settings.audio.audioDevice;

    j["updates"]["checkOnStartup"] = m_Settings.updates.checkOnStartup;
    j["updates"]["autoDownload"]   = m_Settings.updates.autoDownload;
    j["updates"]["updateChannel"]  = m_Settings.updates.updateChannel;
    j["updates"]["lastChecked"]    = m_Settings.updates.lastChecked;

    try {
        std::string path = GetSettingsPath();
        std::ofstream f(path);
        if (f.is_open()) {
            f << j.dump(4);
            f.close();
        } else {
            std::cerr << "[Settings] No se pudo abrir para escritura: " << path << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Settings] Error al guardar: " << e.what() << std::endl;
    }
}

void SettingsManager::LoadSettings() {
    std::string path = GetSettingsPath();
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cout << "[Settings] No existe settings.json, usando valores por defecto." << std::endl;
        return;
    }

    try {
        json j;
        f >> j;

        if (j.contains("projection")) {
            auto&       p  = m_Settings.projection;
            const auto& jp = j["projection"];

            p.targetMonitor = jp.value("targetMonitor", 0);
            p.textSize      = jp.value("textSize",      48.0f);
            p.textColorR    = jp.value("textColorR",    1.0f);
            p.textColorG    = jp.value("textColorG",    1.0f);
            p.textColorB    = jp.value("textColorB",    1.0f);
            p.textColorA    = jp.value("textColorA",    1.0f);
            p.textAlignment = jp.value("textAlignment", 1);
            p.vAlignment    = jp.value("vAlignment",    1);
            p.marginTop     = jp.value("marginTop",     50.0f);
            p.marginBottom  = jp.value("marginBottom",  50.0f);
            p.marginLeft    = jp.value("marginLeft",    50.0f);
            p.marginRight   = jp.value("marginRight",   50.0f);
            p.autoScale     = jp.value("autoScale",     true);
            p.selectedFont  = jp.value("selectedFont",  "default");
            p.defaultBgR    = jp.value("defaultBgR",    0.0f);
            p.defaultBgG    = jp.value("defaultBgG",    0.0f);
            p.defaultBgB    = jp.value("defaultBgB",    0.0f);
        }

        if (j.contains("general")) {
            const auto& jg = j["general"];

            std::string langStr = jg.value("language", "es");
            if      (langStr == "en") m_Settings.general.language = Language::English;
            else if (langStr == "pt") m_Settings.general.language = Language::Portuguese;
            else                      m_Settings.general.language = Language::Spanish;

            m_Settings.general.dismissedChangelog  = jg.value("dismissedChangelog",  "");
            m_Settings.general.startMinimized      = jg.value("startMinimized",      false);
            m_Settings.general.rememberLayout      = jg.value("rememberLayout",      true);
            m_Settings.general.confirmOnExit       = jg.value("confirmOnExit",       true);
            m_Settings.general.autoSave            = jg.value("autoSave",            true);
            m_Settings.general.autoSaveIntervalSec = jg.value("autoSaveIntervalSec", 120);
            m_Settings.general.defaultBiblesFolder = jg.value("defaultBiblesFolder", "");
            m_Settings.general.defaultMediaFolder  = jg.value("defaultMediaFolder",  "");
        }

        if (j.contains("audio")) {
            const auto& ja = j["audio"];

            m_Settings.audio.masterVolume = ja.value("masterVolume", 100);
            m_Settings.audio.muted        = ja.value("muted",        false);
            m_Settings.audio.muteOnBlank  = ja.value("muteOnBlank",  false);
            m_Settings.audio.audioDevice  = ja.value("audioDevice",  "");
        }

        if (j.contains("updates")) {
            const auto& ju = j["updates"];

            m_Settings.updates.checkOnStartup = ju.value("checkOnStartup", true);
            m_Settings.updates.autoDownload   = ju.value("autoDownload",   false);
            m_Settings.updates.updateChannel  = ju.value("updateChannel",  "stable");
            m_Settings.updates.lastChecked    = ju.value("lastChecked",    "");
        }

    } catch (const std::exception& e) {
        std::cerr << "[Settings] Error al cargar: " << e.what() << std::endl;
    }
}

} // namespace ProyecThor::Settings