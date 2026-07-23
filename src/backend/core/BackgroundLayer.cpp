#include "BackgroundLayer.h"
#include "frontend/windowing/SecondaryOutputWindow.h"
#include <iostream>
#include <chrono>
#include <GL/glew.h>
#include <unordered_map>

namespace ProyecThor::Core {

    namespace {
        struct BlitResources {
            GLuint vao = 0, vbo = 0, prog = 0;
        };

        static std::unordered_map<GLFWwindow*, BlitResources> s_ResourcesPerContext;

        // Registrado una sola vez: cuando una ventana secundaria (Proyector,
        // Stage, o cualquier otra a futuro) se destruye, purgamos su entrada
        // del cache. Sin esto, si GLFW reutiliza esa direccion de puntero
        // para una ventana nueva, BlitTexture() bindearia un VAO de un
        // contexto GL que ya no existe.
        static bool s_DestroyHookRegistered = [] {
            SecondaryOutputWindow::RegisterContextDestroyCallback(
                [](GLFWwindow* ctx) { s_ResourcesPerContext.erase(ctx); });
            return true;
        }();

        static const float k_QuadVerts[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f,
        };

        static const char* k_BlitVert = R"GLSL(
#version 330 core
layout(location = 0) in vec2 a_Pos;
layout(location = 1) in vec2 a_UV;
out vec2 v_UV;
void main() {
    v_UV        = a_UV;
    gl_Position = vec4(a_Pos, 0.0, 1.0);
}
)GLSL";

        static const char* k_BlitFrag = R"GLSL(
#version 330 core
in  vec2      v_UV;
out vec4      fragColor;
uniform sampler2D u_Tex;
uniform float     u_Alpha;
uniform float     u_FlipY;
void main() {
    vec2 uv = vec2(v_UV.x, mix(v_UV.y, 1.0 - v_UV.y, u_FlipY));
    vec4 c = texture(u_Tex, uv);
    fragColor = vec4(c.rgb, c.a * u_Alpha);
}
)GLSL";

        static BlitResources& EnsureBlitResources()
        {
            GLFWwindow* ctx = glfwGetCurrentContext();
            auto it = s_ResourcesPerContext.find(ctx);
            if (it != s_ResourcesPerContext.end())
                return it->second;

            BlitResources res;
            auto compile = [](GLenum type, const char* src) -> GLuint {
                GLuint id = glCreateShader(type);
                glShaderSource(id, 1, &src, nullptr);
                glCompileShader(id);
                GLint ok = 0;
                glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
                if (!ok) {
                    char log[512];
                    glGetShaderInfoLog(id, 512, nullptr, log);
                    std::cerr << "[BackgroundLayer] Shader error: " << log << "\n";
                    glDeleteShader(id);
                    return 0;
                }
                return id;
            };

            GLuint vert = compile(GL_VERTEX_SHADER,   k_BlitVert);
            GLuint frag = compile(GL_FRAGMENT_SHADER, k_BlitFrag);
            res.prog = glCreateProgram();
            glAttachShader(res.prog, vert);
            glAttachShader(res.prog, frag);
            glLinkProgram(res.prog);
            glDeleteShader(vert);
            glDeleteShader(frag);

            glGenVertexArrays(1, &res.vao);
            glGenBuffers(1, &res.vbo);
            glBindVertexArray(res.vao);
            glBindBuffer(GL_ARRAY_BUFFER, res.vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(k_QuadVerts), k_QuadVerts, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
            glBindVertexArray(0);

            return s_ResourcesPerContext.emplace(ctx, res).first->second;
        }

        static void BlitTexture(GLuint tex, float alpha = 1.0f, float flipY = 0.0f)
        {
            BlitResources& res = EnsureBlitResources();
            glUseProgram(res.prog);
            glUniform1i(glGetUniformLocation(res.prog, "u_Tex"), 0);
            glUniform1f(glGetUniformLocation(res.prog, "u_Alpha"), alpha);
            glUniform1f(glGetUniformLocation(res.prog, "u_FlipY"), flipY);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glBindVertexArray(res.vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
        }

        static double NowSeconds()
        {
            using namespace std::chrono;
            return duration<double>(steady_clock::now().time_since_epoch()).count();
        }
    } // anonymous namespace

    // useHardwareDecode=false: el parametro de VLCBasePlayer existe
    // justamente para diagnosticar "contencion de sesiones de decode de
    // hardware" (ver el comentario en VLCBasePlayer.h) — muchas GPU de
    // consumo (tipico en NVIDIA GeForce) limitan cuantas sesiones NVDEC/
    // VAAPI concurrentes se pueden abrir a la vez. Esta app abre varios
    // VLCBasePlayer en simultaneo (Active+Standby de Fondos/Overlays/Cola
    // del Monitor, mas preview, mas overlay), y pedirle decode por
    // hardware a todos a la vez puede pasarse de ese limite: el sintoma es
    // exactamente "no aparece ningun frame nuevo" o glitches visuales al
    // arrancar un clip nuevo, que es lo que se reportaba con la cola en
    // Linux. Decode por software es un poco mas de CPU, pero no tiene techo
    // de sesiones concurrentes — para una herramienta de produccion en vivo,
    // confiabilidad gana sobre el ahorro de CPU.
    BackgroundLayer::BackgroundLayer(bool forceSilentAudio)
        : m_PlayerA(2, false, forceSilentAudio)
        , m_PlayerB(2, false, forceSilentAudio)
        , m_NativePlayer(2, false, forceSilentAudio, /*nativeWindowOutput=*/true)
    {
    }

    VLCBasePlayer& BackgroundLayer::Active()  { return m_ActiveIsA ? m_PlayerA : m_PlayerB; }
    VLCBasePlayer& BackgroundLayer::Standby() { return m_ActiveIsA ? m_PlayerB : m_PlayerA; }

    void BackgroundLayer::Update()
    {
        // Contenido activo por motor nativo: no hay textura/crossfade que
        // actualizar para ESTE contenido — VLC dibuja directo en
        // m_NativeWindow por su cuenta (ver SetVideo/SyncNativeWindowVisibility).
        // Active()/Standby() pueden tener un Fondo residual cargado (ver
        // SetVideo()), pero no hace falta seguir subiendole textura
        // mientras no sea lo que se este mostrando.
        if (m_ActiveIsNative) return;

        Active().UpdateTexture();
        Active().EnforceSilenceIfNeeded();
        Standby().EnforceSilenceIfNeeded();

        // FIX: antes solo se subia la textura del player Active — mientras
        // habia un swap pendiente, Standby().GetTextureID() seguia
        // mostrando lo que ESE player object tenia de la ultima vez que fue
        // Active (textura vieja/de otro clip), no el clip nuevo que se esta
        // cargando ahora, hasta el mismo frame en que PerformSwap() corria.
        // El crossfade en Render() (que arranca a mostrar standby en cuanto
        // hay HasVideoFrame()) podia entonces blendear con una textura
        // incorrecta durante esa ventana.
        if (m_SwapPending)
            Standby().UpdateTexture();

        // El prefetch (ver Prefetch()) tambien necesita su textura al dia,
        // aunque todavia no este armado el swap — asi cuando CommitPrefetch()
        // lo arme, Render() ya tiene algo correcto para mostrar de una.
        if (m_PrefetchArmed && !m_SwapPending)
        {
            Standby().UpdateTexture();

            // Clave para que el corte sea instantaneo Y no se desincronice:
            // dejamos que Standby() decodifique un rato (kSwapSettleSeconds,
            // ver comentario en el .h) y RECIEN AHI lo pausamos — no
            // apenas aparece el primer frame. Los primeros frames de un
            // codec recien abierto a veces son artefactos del decoder
            // "calentando" (frame parcial/con colores mal); pausar de
            // entrada podia dejar el standby congelado en uno de esos
            // frames rotos para siempre (al estar pausado, nunca mas
            // decodifica otro que lo corrija). Una vez asentado, se busca
            // la posicion 0 ANTES de pausar — asi sigue arrancando
            // exactamente desde el principio, no desde donde se asento.
            // Si se dejara correr sin pausar nunca (version vieja): (1)
            // llegaria al swap ya avanzado, (2) podia alcanzar su propio
            // fin mientras seguia oculto y la cola lo saltaba apenas se
            // mostraba, y (3) doblaba la carga de decode sostenida — un
            // aporte real al desfasaje bajo sobrecarga.
            if (!Standby().IsPaused())
            {
                if (m_PrefetchReadyAt == 0.0 &&
                    Standby().GetLoadState() == VLCBasePlayer::LoadState::Ready)
                {
                    m_PrefetchReadyAt = NowSeconds();
                }

                if (m_PrefetchReadyAt != 0.0 &&
                    (NowSeconds() - m_PrefetchReadyAt) >= kSwapSettleSeconds)
                {
                    Standby().SetPosition(0.0f);
                    Standby().SetPause(true);
                }
            }
        }
        else
        {
            m_PrefetchReadyAt = 0.0;
        }

        if (m_SwapPending)
        {
            VLCBasePlayer& standby = Standby();
            auto standbyState = standby.GetLoadState();

            bool ready   = standbyState == VLCBasePlayer::LoadState::Ready;
            // Un error real (codec no soportado, archivo corrupto, ruta rota)
            // nunca va a convertirse en Ready por mas que se espere — swapear
            // de inmediato hace visible el fin-de-clip/error ya seteado en
            // este player (ver OnVlcEvent), asi MonitorQueueEngine::Update()
            // lo detecta y salta al siguiente item en el mismo frame, en vez
            // de quedar pegado.
            bool errored = standbyState == VLCBasePlayer::LoadState::Error;
            double now   = NowSeconds();

            if (errored)
            {
                PerformSwap();
                m_SwapReadyAt   = 0.0;
                m_SwapSettledAt = 0.0;
            }
            else if (ready)
            {
                // FIX: antes, si standby no llegaba a Ready dentro de 3s, el
                // swap se forzaba IGUAL — mostrando lo que sea que esa
                // instancia tuviera cargado en su textura de una carga
                // ANTERIOR (nunca se limpia al hacer Stop()), no el contenido
                // nuevo. El publico veia "un fondo sin ninguna relacion" por
                // un instante. Ahora NUNCA se corta a algo que no este
                // realmente listo — se espera lo que haga falta (ver el
                // limite de emergencia mas abajo, que abandona el swap sin
                // mostrar nada raro en vez de forzarlo).
                //
                // Una vez listo, en vez de un corte seco se hace un
                // crossfade corto y fijo (frame final del fondo anterior ->
                // frame inicial del nuevo, nada mas en el medio) via
                // m_TransitionProgress, que ya consume Render().
                if (m_SwapReadyAt == 0.0)
                {
                    m_SwapReadyAt = now;
                    float loadedIn = static_cast<float>(now - m_PendingSwapStart);
                    m_RecentLoadDurations.push_back(loadedIn);
                    if (m_RecentLoadDurations.size() > kMaxLoadSamples)
                        m_RecentLoadDurations.pop_front();
                }

                double sinceReady = now - m_SwapReadyAt;
                bool   settled    = sinceReady >= kSwapSettleSeconds;

                if (!settled)
                {
                    // Todavia "asentando": no mostrar nada de standby en el
                    // blend por ahora (ver comentario de kSwapSettleSeconds
                    // en el .h — mismo margen que ya aplico el prefetch
                    // antes de pausar, asi que en el camino de cola esto
                    // suele resolverse casi al instante).
                    m_TransitionProgress = 0.0f;
                }
                else
                {
                    if (m_SwapSettledAt == 0.0)
                        m_SwapSettledAt = now;

                    double blendElapsed = now - m_SwapSettledAt;
                    m_TransitionProgress = static_cast<float>(
                        std::clamp(blendElapsed / kSwapBlendSeconds, 0.0, 1.0));

                    if (blendElapsed >= kSwapBlendSeconds)
                    {
                        PerformSwap();
                        m_SwapReadyAt   = 0.0;
                        m_SwapSettledAt = 0.0;
                    }
                }
            }
            else if ((now - m_PendingSwapStart) > kSwapGiveUpSeconds)
            {
                // Limite de emergencia (carga colgada, no error ni Ready):
                // se abandona el swap y se sigue mostrando lo de antes, en
                // vez de forzar un corte a contenido no listo.
                std::cerr << "[BackgroundLayer] Swap abandonado tras "
                          << kSwapGiveUpSeconds << "s sin quedar listo ni dar error; "
                          << "se mantiene el fondo anterior.\n";
                m_SwapPending   = false;
                m_SwapReadyAt   = 0.0;
                m_SwapSettledAt = 0.0;
            }
            // si no, sigue esperando sin forzar nada.
        }
    }

    void BackgroundLayer::Render(int outputW, int outputH)
    {
        // Contenido activo por motor nativo: la ventana de VLC se muestra
        // por su cuenta, fuera de este compositor GL — nada que blitear.
        if (m_ActiveIsNative) return;

        GLuint rawTex = static_cast<GLuint>(
            reinterpret_cast<uintptr_t>(Active().GetTextureID()));

        if (rawTex == 0)
            return;

        int srcW = 0, srcH = 0;
        Active().GetVideoSize(srcW, srcH);

        if (srcW <= 0 || srcH <= 0)
            return;

        int viewX = 0;
        int viewY = 0;
        int viewW = outputW;
        int viewH = outputH;

        if (!m_StretchToFill) {

            float videoRatio  = static_cast<float>(srcW) / static_cast<float>(srcH);
            float screenRatio = static_cast<float>(outputW) / static_cast<float>(outputH);

            if (videoRatio > screenRatio + 0.001f) {
                viewW = outputW;
                viewH = static_cast<int>(static_cast<float>(outputW) / videoRatio);
                viewX = 0;
                viewY = (outputH - viewH) / 2;
            } else if (videoRatio < screenRatio - 0.001f) {
                viewH = outputH;
                viewW = static_cast<int>(static_cast<float>(outputH) * videoRatio);
                viewX = (outputW - viewW) / 2;
                viewY = 0;
            }
        }

        GLuint finalTex = rawTex;

        if (m_FSREnabled) {
            bool needReinit = (!m_FSR.IsInitialized() ||
                               m_FSR.GetOutputW() != viewW ||
                               m_FSR.GetOutputH() != viewH);

            if (needReinit) {
                bool ok = m_FSR.Init(viewW, viewH);
                if (ok) {
                    m_FSR.SetSharpness(m_FSRSharpness);
                    m_FSR.SetEnabled(true);
                } else {
                    m_FSREnabled = false;
                    std::cout << "[BG] FSR no disponible, usando blit directo.\n";
                }
            }

            if (m_FSREnabled && m_FSR.IsInitialized() && (srcW < viewW || srcH < viewH)) {
                GLuint upscaled = m_FSR.Process(rawTex, srcW, srcH);
                if (upscaled != 0)
                    finalTex = upscaled;
            }
        }

        if (m_SwapPending && Standby().HasVideoFrame())
        {
            GLuint standbyTex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(Standby().GetTextureID()));
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glViewport(viewX, viewY, viewW, viewH);
            BlitTexture(finalTex,   1.0f - m_TransitionProgress, m_FlipVideoY ? 1.0f : 0.0f);
            BlitTexture(standbyTex, m_TransitionProgress,          m_FlipVideoY ? 1.0f : 0.0f);
            glDisable(GL_BLEND);
        }
        else
        {
            glDisable(GL_BLEND);
            glViewport(viewX, viewY, viewW, viewH);
            BlitTexture(finalTex, 1.0f, m_FlipVideoY ? 1.0f : 0.0f);
        }

        glViewport(0, 0, outputW, outputH);
    }

    void BackgroundLayer::RenderLogo(unsigned int logoTex, int logoW, int logoH, int outputW, int outputH)
    {
        if (logoTex == 0 || logoW <= 0 || logoH <= 0 || outputW <= 0 || outputH <= 0) return;

        // Mismo criterio de letterbox que Render() (no estira, mantiene la
        // proporcion real del logo dentro del viewport de salida).
        int viewX = 0, viewY = 0, viewW = outputW, viewH = outputH;

        float logoRatio   = static_cast<float>(logoW) / static_cast<float>(logoH);
        float screenRatio = static_cast<float>(outputW) / static_cast<float>(outputH);

        if (logoRatio > screenRatio + 0.001f) {
            viewW = outputW;
            viewH = static_cast<int>(static_cast<float>(outputW) / logoRatio);
            viewX = 0;
            viewY = (outputH - viewH) / 2;
        } else if (logoRatio < screenRatio - 0.001f) {
            viewH = outputH;
            viewW = static_cast<int>(static_cast<float>(outputH) * logoRatio);
            viewX = (outputW - viewW) / 2;
            viewY = 0;
        }

        glDisable(GL_BLEND);
        glViewport(0, 0, outputW, outputH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glViewport(viewX, viewY, viewW, viewH);
        BlitTexture(static_cast<GLuint>(logoTex), 1.0f, m_FlipVideoY ? 1.0f : 0.0f);

        glViewport(0, 0, outputW, outputH);
    }

    void BackgroundLayer::SetStretchToFill(bool stretch)
    {
        m_StretchToFill = stretch;
    }

    bool BackgroundLayer::GetStretchToFill() const
    {
        return m_StretchToFill;
    }

    void BackgroundLayer::SetFSREnabled(bool enabled)
    {
        m_FSREnabled = enabled;
        m_FSR.SetEnabled(enabled);
    }

    bool BackgroundLayer::GetFSREnabled() const
    {
        return m_FSREnabled;
    }

    void BackgroundLayer::SetFSRSharpness(float sharpness)
    {
        m_FSRSharpness = sharpness;
        m_FSR.SetSharpness(sharpness);
    }

    float BackgroundLayer::GetFSRSharpness() const
    {
        return m_FSRSharpness;
    }

    void* BackgroundLayer::GetTextureID()
    {
        return Active().GetTextureID();
    }

    void* BackgroundLayer::GetStandbyTextureID()
    {
        return Standby().GetTextureID();
    }

    bool BackgroundLayer::StandbyHasFrame()
    {
        return Standby().HasVideoFrame();
    }

    VLCBasePlayer* BackgroundLayer::GetPlayer()
    {
        // Punto unico usado por la cola del Monitor (MonitorQueueEngine::
        // Update -> ConsumeEndReached/ConsumeHadError, sin lo cual la cola
        // nunca avanza) y por los controles de transporte/preview (play,
        // pausa, seek, VU meter) para llegar al reproductor que
        // REALMENTE tiene el contenido activo — con el motor libvlc
        // (m_ActiveIsNative), eso es m_NativePlayer, no Active().
        return m_ActiveIsNative ? &m_NativePlayer : &Active();
    }

    void BackgroundLayer::SetVideo(const std::string& path, bool allowAudio)
    {
        // El motor nativo aplica SOLO a video real (allowAudio=true —
        // Videos/cola del Monitor): Fondos/imagenes (allowAudio=false)
        // siempre necesitan overlays/texto encima, asi que siempre van
        // por OpenGL sin importar este ajuste (ver comentario del
        // miembro m_UseNativeEngine en el .h).
        if (m_UseNativeEngine && allowAudio)
        {
            // Sin crossfade/standby en este motor: corte directo, igual
            // que un reproductor simple. m_TargetMuted/m_TargetVolume son
            // los mismos que ya usa el motor OpenGL (ver SetLiveVolume/
            // SetLiveMute), asi que respetar el volumen ya configurado.
            // Active()/Standby() (el Fondo que hubiera, si alguno) se
            // dejan tal cual estan: no hace falta pararlos, Update()/
            // Render() ya los ignoran mientras m_ActiveIsNative sea true,
            // y vuelven a mostrarse solos si el operador carga otro Fondo.
            m_IsVideo             = true;
            m_ContentAllowsAudio  = allowAudio;
            m_ActiveIsNative      = true;

            // Mostrar la ventana es una llamada GLFW: tiene que correr aca,
            // en el hilo principal (el que llama a SetVideo()).
            void* handle = m_IsLiveToPublic ? m_NativeWindow.Show(m_LastKnownMonitorIndex) : nullptr;

            bool wantActive = m_IsLiveToPublic && allowAudio;
            bool wantMute   = m_TargetMuted || !wantActive;
            int  wantVolume = (wantActive && !m_TargetMuted) ? m_TargetVolume : 0;

            // FIX (colgaba/"No responde" desde el 2do clip en adelante,
            // confirmado con Wine: dos hilos bloqueados entre si en una
            // critical section de Windows): TODO lo que toca
            // m_NativePlayer — adjuntar la ventana, Play(), y el gate de
            // audio real — va combinado en UNA sola accion despachada a
            // m_NativeLoader, nunca repartido entre el hilo principal y el
            // worker. Repartirlo (attach aca, Play() alla) fue justamente
            // lo que causaba la carrera: dos hilos tocando el mismo
            // libvlc_media_player_t a la vez. Ademas, adjuntar la ventana
            // tiene que pasar ANTES de Play() (la doc de libVLC dice que
            // set_hwnd/set_xwindow "toma efecto cuando arranca la
            // reproduccion"), y el gate de audio real tiene que ir DESPUES
            // de Play(): Play(startMuted=true) siempre arranca mudo por
            // diseño (evita un "pop"), asi que hay que reaplicar el mute/
            // volumen real una vez que Play() ya corrio, no antes.
            m_NativeLoader.Request([this, path, handle, wantActive, wantMute, wantVolume]() {
                if (handle) m_NativePlayer.AttachNativeWindow(handle);
                m_NativePlayer.Play(path, /*loop=*/false, /*startMuted=*/true);
                m_NativePlayer.SetAudioActive(wantActive);
                m_NativePlayer.SetMute(wantMute);
                m_NativePlayer.SetVolume(wantVolume);
            });
            return;
        }

        // Esto va por OpenGL: si el contenido activo ANTERIOR era nativo,
        // hay que apagarlo primero — la ventana nativa no debe seguir
        // tapando "ProjectorLive" con un video viejo mientras esto nuevo
        // carga.
        if (m_ActiveIsNative)
        {
            m_ActiveIsNative = false;
            // FIX (deadlock confirmado con Wine: dos hilos bloqueados
            // entre si en una critical section de Windows): Stop() +
            // Detach + mute van combinados en UNA sola accion en el
            // worker -- nunca repartidos entre el hilo principal y el
            // worker (ver el comentario largo en PreviewLoadWorker.h).
            // Ocultar la ventana (GLFW) si corre aca, en el hilo principal.
            m_NativeWindow.Hide();
            m_NativeLoader.Request([this]() {
                m_NativePlayer.SetAudioActive(false);
                m_NativePlayer.Stop();
                m_NativePlayer.DetachNativeWindow();
            });
        }

        // FIX (freeze/flash en clicks repetidos): si esto es EXACTAMENTE lo
        // que ya se esta mostrando (nada en swap, misma ruta ya activa), es
        // un pedido redundante — ignorarlo evita reiniciar innecesariamente
        // el clip y evita el ping-pong entre Active/Standby que un guard a
        // nivel VLCBasePlayer por si solo no cubre (cada swap deja al
        // player saliente con la ruta limpiada por su propio Stop()).
        if (!m_SwapPending && GetTextureID() != nullptr && path == Active().GetCurrentPath())
            return;

        m_IsVideo = true;
        m_ContentAllowsAudio = allowAudio;   // <-- se fija ANTES de reproducir

        // Cualquier carga directa (click manual en Fondos/Videos, etc.)
        // toma standby para si misma — invalida un prefetch de cola que
        // pudiera estar esperando ahi, para que CommitPrefetch() no lo
        // confunda despues con contenido que ya no es el que arranco.
        m_PrefetchArmed = false;
        m_PrefetchedPath.clear();

        if (m_SwapPending || GetTextureID() != nullptr)
        {
            Standby().Play(path, /*loop=*/false, /*startMuted=*/true);
            Standby().SetAudioActive(false);
            m_SwapPending      = true;
            m_PendingSwapStart = NowSeconds();
            // FIX (fondo "de otro video" en el flash): si esto pisa un swap
            // que YA estaba en curso (m_SwapPending ya era true — ej. el
            // operador elige otra cosa mientras la cola todavia estaba
            // blendeando la transicion anterior), m_SwapReadyAt/
            // m_SwapSettledAt quedaban con la marca de tiempo del swap
            // VIEJO. El contenido nuevo entonces "heredaba" un progreso de
            // blend ya adelantado (a veces ya completo), revelandose de
            // golpe en un momento arbitrario con lo que sea que hubiera en
            // el buffer en ese instante — ni el fondo viejo ni el nuevo de
            // verdad, un tercer frame a medio cargar. Cada swap nuevo
            // arranca su propio asentamiento de cero.
            m_SwapReadyAt   = 0.0;
            m_SwapSettledAt = 0.0;
            // FIX (parte 2, la que realmente causaba el flash): resetear
            // SOLO el cronometro no alcanzaba — m_TransitionProgress
            // (el valor de blend en si) quedaba con el numero del swap
            // VIEJO (ej. 0.75 = 75% mezclado hacia el fondo anterior).
            // Eso se veia mientras el nuevo contenido cargaba (mezcla
            // vieja de mas), y despues SALTABA de golpe a 0.0 apenas el
            // codigo de abajo entraba a la rama "todavia asentando" —
            // exactamente el salto/flash que se ve como un tercer fondo.
            // Forzar el valor a 0 aca, en el mismo instante en que se pide
            // el swap nuevo, hace que ese salto pase ANTES de que haya
            // nada nuevo que mostrar (imperceptible) en vez de a mitad de
            // una mezcla ya visible.
            m_TransitionProgress = 0.0f;
        }
        else
        {
            Active().Play(path, /*loop=*/false, /*startMuted=*/true);
            if (!m_IsLiveToPublic || !allowAudio)
            {
                Active().SetAudioActive(false);
            }
            else
            {
                // Play(..., startMuted=true) siempre arranca muteado (evita
                // un "pop" al cargar) — cuando SI corresponde audio real
                // (en vivo + contenido que lo permite) hay que restaurarlo
                // aca, igual que ya hacen PerformSwap() y SetPubliclyLive().
                // Antes esta rama no lo hacia: el fondo quedaba mudo hasta
                // que el operador tocaba mute/desmute a mano.
                Active().SetAudioActive(true);
                Active().SetMute(m_TargetMuted);
                Active().SetVolume(m_TargetMuted ? 0 : m_TargetVolume);
            }
        }
    }

    float BackgroundLayer::GetEstimatedLoadSeconds() const
    {
        if (m_RecentLoadDurations.empty()) return kDefaultEtaSeconds;
        float sum = 0.0f;
        for (float v : m_RecentLoadDurations) sum += v;
        return sum / static_cast<float>(m_RecentLoadDurations.size());
    }

    void BackgroundLayer::Prefetch(const std::string& path, bool allowAudio)
    {
        // Sin crossfade en el motor nativo, no hay nada util que precargar
        // (ver CommitPrefetch(), que en este modo cae directo a SetVideo()).
        // Igual que en SetVideo(): solo aplica a video real (allowAudio).
        if (m_UseNativeEngine && allowAudio) return;

        // Si ya hay un swap en curso, Standby() es justo el player que esta
        // por pasar a Active — pisarlo aca corromperia ese swap en vuelo.
        // Se descarta este prefetch; quien llama puede reintentar en un
        // frame posterior (la cola lo hace de forma natural, ver
        // MonitorQueueEngine: solo prefetch-ea recien cuando SU swap ya
        // termino).
        if (m_SwapPending) return;

        // Igual que la rama "ya hay algo al aire" de SetVideo(), pero SIN
        // armar m_SwapPending: el clip queda cargando (y luego pausado, ver
        // Update()) en standby, listo para cuando CommitPrefetch() lo pida,
        // sin disparar el crossfade por su cuenta.
        m_IsVideo = true;
        m_ContentAllowsAudio = allowAudio;
        m_PrefetchedPath = path;
        m_PrefetchArmed  = true;
        m_PrefetchReadyAt = 0.0; // arranca de cero el asentamiento para ESTE prefetch

        Standby().Play(path, /*loop=*/false, /*startMuted=*/true);
        Standby().SetAudioActive(false);
    }

    void BackgroundLayer::CommitPrefetch(const std::string& path, bool allowAudio)
    {
        // Sin prefetch en el motor nativo (ver Prefetch()): cae directo a
        // un corte simple, igual que si nunca se hubiera precargado nada.
        if (m_UseNativeEngine && allowAudio) { SetVideo(path, allowAudio); return; }

        // Esto va por OpenGL: mismo apagado del nativo que en SetVideo(),
        // por si el contenido activo anterior venia de ahi.
        if (m_ActiveIsNative)
        {
            m_ActiveIsNative = false;
            // FIX (deadlock confirmado con Wine: dos hilos bloqueados
            // entre si en una critical section de Windows): Stop() +
            // Detach + mute van combinados en UNA sola accion en el
            // worker -- nunca repartidos entre el hilo principal y el
            // worker (ver el comentario largo en PreviewLoadWorker.h).
            // Ocultar la ventana (GLFW) si corre aca, en el hilo principal.
            m_NativeWindow.Hide();
            m_NativeLoader.Request([this]() {
                m_NativePlayer.SetAudioActive(false);
                m_NativePlayer.Stop();
                m_NativePlayer.DetachNativeWindow();
            });
        }

        if (m_PrefetchArmed && m_PrefetchedPath == path)
        {
            // Ya esta listo (Update() lo pauso apenas decodifico su primer
            // frame): solo hace falta armar el swap. El gate normal de
            // Update() lo encuentra Ready de inmediato y PerformSwap()
            // lo despausa ahi mismo, asi que el corte es instantaneo.
            m_ContentAllowsAudio = allowAudio;
            m_SwapPending        = true;
            m_PendingSwapStart   = NowSeconds();
            // Mismo motivo que en SetVideo(): nunca heredar timing NI valor
            // de blend de un swap anterior (ver el comentario ahi para el
            // detalle completo de por que ambas cosas hacen falta).
            m_SwapReadyAt        = 0.0;
            m_SwapSettledAt      = 0.0;
            m_TransitionProgress = 0.0f;
            m_PrefetchArmed      = false;
            m_PrefetchedPath.clear();
            return;
        }

        // Nada precargado (o precargo otra cosa, ej. la cola se reordeno):
        // carga en frio normal, mismo camino que un click directo.
        m_PrefetchArmed = false;
        m_PrefetchedPath.clear();
        SetVideo(path, allowAudio);
    }

    void BackgroundLayer::PerformSwap()
    {
        VLCBasePlayer& oldActive = Active();
        m_ActiveIsA = !m_ActiveIsA;
        VLCBasePlayer& newActive = Active();

        // Ahora el swap respeta el permiso asociado al contenido que se esta
        // por mostrar, no solo el estado global "al aire".
        if (m_IsLiveToPublic && m_ContentAllowsAudio)
        {
            newActive.SetAudioActive(true);
            newActive.SetMute(m_TargetMuted);
            newActive.SetVolume(m_TargetMuted ? 0 : m_TargetVolume);
        }
        else
        {
            newActive.SetAudioActive(false);
        }
        newActive.SetPause(false);

        oldActive.SetAudioActive(false);
        oldActive.SetMute(true);
        oldActive.Stop();

        m_SwapPending = false;
    }

    void BackgroundLayer::SetSolidColor(float r, float g, float b)
    {
        m_IsVideo    = false;
        m_BgColor[0] = r;
        m_BgColor[1] = g;
        m_BgColor[2] = b;

        m_SwapPending   = false;
        m_SwapReadyAt   = 0.0;
        m_SwapSettledAt = 0.0;
        m_PrefetchArmed = false;
        m_PrefetchedPath.clear();
        m_PlayerA.Stop();
        m_PlayerB.Stop();

        // Un color solido nunca es "video nativo" — si lo activo hasta
        // ahora era eso, apagarlo y revelar "ProjectorLive" de nuevo.
        if (m_ActiveIsNative)
        {
            m_ActiveIsNative = false;
            // FIX (deadlock confirmado con Wine: dos hilos bloqueados
            // entre si en una critical section de Windows): Stop() +
            // Detach + mute van combinados en UNA sola accion en el
            // worker -- nunca repartidos entre el hilo principal y el
            // worker (ver el comentario largo en PreviewLoadWorker.h).
            // Ocultar la ventana (GLFW) si corre aca, en el hilo principal.
            m_NativeWindow.Hide();
            m_NativeLoader.Request([this]() {
                m_NativePlayer.SetAudioActive(false);
                m_NativePlayer.Stop();
                m_NativePlayer.DetachNativeWindow();
            });
        }
    }

    void* BackgroundLayer::GetProcessedTexture(int targetW, int targetH) {
        GLuint rawTex = static_cast<GLuint>(reinterpret_cast<uintptr_t>(Active().GetTextureID()));
        if (rawTex == 0 || targetW <= 0 || targetH <= 0)
            return nullptr;

        if (!m_FSREnabled)
            return (void*)(uintptr_t)rawTex;

        int srcW = 0, srcH = 0;
        Active().GetVideoSize(srcW, srcH);

        if (srcW <= 0 || srcH <= 0 || (srcW >= targetW && srcH >= targetH))
            return (void*)(uintptr_t)rawTex;

        bool needReinit = (!m_FSR.IsInitialized() ||
                           m_FSR.GetOutputW() != targetW ||
                           m_FSR.GetOutputH() != targetH);

        if (needReinit) {
            if (m_FSR.Init(targetW, targetH)) {
                m_FSR.SetSharpness(m_FSRSharpness);
                m_FSR.SetEnabled(true);
            } else {
                m_FSREnabled = false;
                return (void*)(uintptr_t)rawTex;
            }
        }

        GLuint upscaled = m_FSR.Process(rawTex, srcW, srcH);
        return upscaled ? (void*)(uintptr_t)upscaled : (void*)(uintptr_t)rawTex;
    }

    // Muestra/adjunta o esconde/desvincula m_NativeWindow segun
    // m_IsLiveToPublic && m_ActiveIsNative — llamar despues de cambiar
    // cualquiera de esos dos (SetPubliclyLive, SetVideo/Prefetch/
    // CommitPrefetch/SetSolidColor). Idempotente: llamarla de mas no
    // rompe nada (Show()/Hide()/Attach()/Detach() ya lo son).
    void BackgroundLayer::SyncNativeWindowVisibility()
    {
        // Mostrar/ocultar la ventana (GLFW) se hace aca mismo, en el hilo
        // que llama (siempre el principal) — son llamadas GLFW, tienen que
        // correr ahi. Adjuntar/desvincular la ventana en libVLC (Attach/
        // DetachNativeWindow) y el gate de audio, en cambio, SIEMPRE van
        // combinados en UNA sola accion despachada a m_NativeLoader: nunca
        // deben correr en el hilo principal directo, ni repartidos entre
        // dos pedidos separados al worker (ver el comentario largo en
        // PreviewLoadWorker.h — asi se corrigio un deadlock real).
        if (m_IsLiveToPublic && m_ActiveIsNative)
        {
            void* handle = m_NativeWindow.Show(m_LastKnownMonitorIndex);

            bool wantActive = m_ContentAllowsAudio;
            bool wantMute   = m_TargetMuted || !wantActive;
            int  wantVolume = (wantActive && !m_TargetMuted) ? m_TargetVolume : 0;

            m_NativeLoader.Request([this, handle, wantActive, wantMute, wantVolume]() {
                if (handle) m_NativePlayer.AttachNativeWindow(handle);
                m_NativePlayer.SetAudioActive(wantActive);
                m_NativePlayer.SetMute(wantMute);
                m_NativePlayer.SetVolume(wantVolume);
            });
        }
        else
        {
            m_NativeWindow.Hide();

            m_NativeLoader.Request([this]() {
                m_NativePlayer.SetAudioActive(false);
                m_NativePlayer.DetachNativeWindow();
            });
        }
    }

    void BackgroundLayer::SetPubliclyLive(bool live, int monitorIndex)
    {
        m_IsLiveToPublic = live;
        if (monitorIndex >= 0) m_LastKnownMonitorIndex = monitorIndex;

        // Independiente de si lo activo AHORA es nativo o no: sincroniza
        // la ventana nativa (la esconde si live paso a false, o si lo
        // activo no es nativo) y, mas abajo, el audio del path OpenGL de
        // siempre (inofensivo aunque Active()/Standby() no tengan nada
        // relevante cargado en este momento).
        SyncNativeWindowVisibility();

        if (live)
        {
            // Al pasar a "en vivo", el player activo adopta el target de
            // volumen/mute que el operador ya haya configurado (ver
            // SetLiveVolume/SetLiveMute). Sin embargo, si el contenido
            // actualmente cargado no permite audio, debe permanecer mudo.
            bool activeAudioAllowed = m_ContentAllowsAudio;
            Active().SetAudioActive(activeAudioAllowed);
            Active().SetMute(m_TargetMuted || !activeAudioAllowed);
            Active().SetVolume(activeAudioAllowed && !m_TargetMuted ? m_TargetVolume : 0);
            Standby().SetAudioActive(false);
        }
        else
        {
            // Cortar audio de raiz en ambos players, sin importar el
            // volumen/mute configurado.
            m_PlayerA.SetAudioActive(false);
            m_PlayerB.SetAudioActive(false);
        }
    }

    void BackgroundLayer::SetLiveVolume(int volume0to200)
    {
        m_TargetVolume = volume0to200;
        if (!m_IsLiveToPublic) return;

        if (m_ActiveIsNative) m_NativePlayer.SetVolume(m_TargetMuted ? 0 : m_TargetVolume);
        else                  Active().SetVolume(m_TargetMuted ? 0 : m_TargetVolume);
    }

    void BackgroundLayer::SetLiveMute(bool mute)
    {
        m_TargetMuted = mute;
        if (!m_IsLiveToPublic) return;

        VLCBasePlayer& target = m_ActiveIsNative ? m_NativePlayer : Active();
        target.SetMute(mute);
        target.SetVolume(mute ? 0 : m_TargetVolume);
    }

    // ── Dispositivo de salida de audio ──────────────────────────────────

    std::vector<VLCBasePlayer::AudioDevice> BackgroundLayer::GetAvailableAudioDevices()
    {
        // Cualquiera de los dos players sirve para enumerar: ambos corren
        // en el mismo proceso y ven los mismos dispositivos del sistema.
        return m_PlayerA.GetAvailableAudioDevices();
    }

    void BackgroundLayer::SetAudioOutputDevice(const std::string& deviceId)
    {
        m_AudioDeviceId = deviceId;

        // Se aplica a TODOS los players (no solo al activo): el standby
        // puede pasar a ser el activo en cualquier momento via
        // PerformSwap(), y el nativo puede pasar a estarlo en el proximo
        // SetVideo() con el motor libvlc activo — para entonces ya deben
        // estar apuntando al dispositivo correcto.
        m_PlayerA.SetAudioDevice(deviceId);
        m_PlayerB.SetAudioDevice(deviceId);
        m_NativePlayer.SetAudioDevice(deviceId);
    }

    void BackgroundLayer::BlockPath(const std::string& path)
    {
        m_PlayerA.BlockPath(path);
        m_PlayerB.BlockPath(path);
        m_NativePlayer.BlockPath(path);
    }

    void BackgroundLayer::UnblockPath()
    {
        m_PlayerA.UnblockPath();
        m_PlayerB.UnblockPath();
        m_NativePlayer.UnblockPath();
    }

} // namespace ProyecThor::Core