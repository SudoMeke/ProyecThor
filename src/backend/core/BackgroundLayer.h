#pragma once
#include <GL/glew.h>
#include "backend/media/VLCBasePlayer.h"
#include "backend/shaders/PostProcessorFSR.h"
#include <string>

namespace ProyecThor::Core {

    class BackgroundLayer {
    private:
        VLCBasePlayer m_Player;
        bool m_IsVideo = false;
        float m_BgColor[3] = { 0.0f, 0.0f, 0.0f };

        ProyecThor::Shaders::PostProcessorFSR m_FSR;
        bool  m_FSREnabled   = true;
        float m_FSRSharpness = 0.2f;

        bool  m_StretchToFill = true;

    public:
        BackgroundLayer() = default;
        ~BackgroundLayer() = default;

        void Update();
        void Render(int outputW, int outputH);

        void  SetFSREnabled(bool enabled);
        bool  GetFSREnabled() const;
        void  SetFSRSharpness(float sharpness);
        float GetFSRSharpness() const;

        void  SetStretchToFill(bool stretch);
        bool  GetStretchToFill() const;
        void* GetProcessedTexture(int targetW, int targetH);
        void* GetTextureID();
        VLCBasePlayer* GetPlayer();
        void SetVideo(const std::string& path);
        void SetSolidColor(float r, float g, float b);

        // Delega directamente al reproductor interno. Ver comentarios de
        // VLCBasePlayer::BlockPath / UnblockPath.
        void BlockPath(const std::string& path);
        void UnblockPath();
    };

} // namespace ProyecThor::Core