#include "menu.h"
#include "viewmodel/viewmodel_hook.h"
#include "thirdperson/thirdperson.h"
#include "dx9_hook.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "notifications.h"
#include "core/config_file.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "inter_font.h"
#include "fa_icons.h"
#include <windows.h>
#include <cstdio>
#include <string>
#include <cmath>
#include <math.h>

#define ICON_MIN_FA 0xf000
#define ICON_MAX_FA 0xf950

namespace menu {
    static bool g_visible = false;
    static bool g_menuJuego = false;
    static bool g_cursorBloqueado = false;
    static POINT g_cursorAntes = {};
    static bool g_render = false;
    static ULONGLONG g_animInicio = 0;
    static HWND g_hwnd = nullptr;

    static const ImVec4 C_BG(0.067f, 0.071f, 0.078f, 1.0f);
    static const ImVec4 C_PANEL(0.105f, 0.113f, 0.125f, 1.0f);
    static const ImVec4 C_ACCENT(0.55f, 0.86f, 0.72f, 1.0f);
    static const ImVec4 C_ACCENT_H(0.66f, 0.94f, 0.81f, 1.0f);
    static const ImVec4 C_BORDER(0.19f, 0.20f, 0.22f, 1.0f);
    static const ImVec4 C_DIM(0.58f, 0.60f, 0.63f, 1.0f);
    static const ImVec4 C_TXT(0.93f, 0.94f, 0.95f, 1.0f);
    static const ImVec4 C_AXES[] = {
        ImVec4(0.94f, 0.54f, 0.51f, 1),
        ImVec4(0.91f, 0.77f, 0.48f, 1),
        ImVec4(0.49f, 0.84f, 0.82f, 1)
    };
    static bool g_dirty = false;
    static bool g_saved = false;
    static bool g_saveError = false;
    static bool g_crosshairActivado = false;
    static int g_crosshairTipo = 0;       // +, X, punto, circulo
    static float g_crosshairTamano = 8.0f;
    static float g_crosshairGrosor = 2.0f;
    static ImVec4 g_crosshairColor(0.95f, 0.16f, 0.14f, 1.0f);
    static bool g_crosshairRGB = false;
    static bool g_crosshairAutoForma = false;
    static ULONGLONG g_crosshairAnimInicio = 0;
    static ImVec2 g_panelPos(-1.0f, -1.0f);

    static int g_hotkey = VK_INSERT;
    static bool g_capturando = false;
    static bool g_teclaAntes = false;
    static bool g_capturaAbajo = false;
    static ULONGLONG g_capturaInicio = 0;
    static int g_vmHotkey = 0;
    static bool g_vmCapturando = false;
    static bool g_vmTeclaAntes = true;
    static bool g_vmCapturaAbajo = false;
    static ULONGLONG g_vmCapturaInicio = 0;
    static bool g_vmActivado = true;
    static int g_tpHotkey = 0;
    static bool g_tpCapturando = false;
    static bool g_tpTeclaAntes = false;
    static bool g_tpCapturaAbajo = false;
    static ULONGLONG g_tpCapturaInicio = 0;
    static int g_tab = 0; // 0 = Viewmodel, 1 = Mira, 2 = Tercera persona
    static notices::Queue g_notices;

    static void NotificarCambio(const char* name,bool enabled) {
        g_notices.Show(name,enabled,GetTickCount64());
    }
    static bool TeclaAsignable(int vk) {
        return vk>=0x08 && vk<=0xFE && vk!=VK_ESCAPE &&
            vk!=VK_CONTROL && vk!=VK_LCONTROL && vk!=VK_RCONTROL &&
            vk!=VK_MENU && vk!=VK_LMENU && vk!=VK_RMENU &&
            vk!=VK_SHIFT && vk!=VK_LSHIFT && vk!=VK_RSHIFT &&
            vk!=VK_LWIN && vk!=VK_RWIN;
    }

    static std::wstring RutaConfig() {
        return configfile::BesideModule(reinterpret_cast<const void*>(&RutaConfig));
    }
    static float NumeroConfig(const char* text,float minimum,float maximum,float fallback) {
        char* end=nullptr;
        const double value=strtod(text,&end);
        if(end==text || *end || !std::isfinite(value)) return fallback;
        return float(value<minimum ? minimum : value>maximum ? maximum : value);
    }
    static void CargarConfig() {
        g_notices.Clear(); // Loading preferences is not a user toggle.
        tp::g_activado = false;
        tp::g_distancia = tp::DISTANCIA_DEFAULT;
        g_tpHotkey = 0;
        g_vmHotkey = 0;
        g_vmCapturando = false;
        g_vmTeclaAntes = true;
        g_vmCapturaAbajo = false;
        g_vmCapturaInicio = 0;
        g_vmActivado = true;
        vmh::g_factor = 1.0f;
        vmh::g_offx=vmh::g_offy=vmh::g_offz=0;
        g_hotkey=VK_INSERT;
        g_crosshairActivado = false;
        g_crosshairTipo = 0;
        g_crosshairTamano = 8.0f;
        g_crosshairGrosor = 2.0f;
        g_crosshairColor = ImVec4(0.95f, 0.16f, 0.14f, 1.0f);
        g_crosshairRGB = false;
        g_crosshairAutoForma = false;
        g_crosshairAnimInicio = GetTickCount64();
        const auto path=RutaConfig();
        FILE* f = path.empty() ? nullptr : _wfopen(path.c_str(), L"r");
        if (!f) return;
        char linea[512];
        while (fgets(linea, sizeof(linea), f)) {
            char clave[64]; char valor[256];
            if (sscanf(linea, "%63s = %255s", clave, valor)==2) {
                if (!strcmp(clave,"vmOffx")) vmh::g_offx=NumeroConfig(valor,-20,20,0);
                else if (!strcmp(clave,"vmOffy")) vmh::g_offy=NumeroConfig(valor,-20,20,0);
                else if (!strcmp(clave,"vmOffz")) vmh::g_offz=NumeroConfig(valor,-20,20,0);
                else if (!strcmp(clave,"vmActivado")) g_vmActivado=atoi(valor)!=0;
                else if (!strcmp(clave,"vmTecla")) { int v=atoi(valor); if(TeclaAsignable(v)) g_vmHotkey=v; }
                else if (!strcmp(clave,"hotkey")) {
                    char* end=nullptr; const long v=strtol(valor,&end,10);
                    if(end!=valor && !*end && (v==0 || TeclaAsignable(v))) g_hotkey=int(v);
                }
                else if (!strcmp(clave,"crosshairActivado")) g_crosshairActivado = atoi(valor)!=0;
                else if (!strcmp(clave,"crosshairTipo")) { int v=atoi(valor); g_crosshairTipo = (v>=0 && v<4)?v:0; }
                else if (!strcmp(clave,"crosshairTamano")) {
                    float v=(float)atof(valor);
                    if(v>=2.0f && v<=40.0f) g_crosshairTamano=v;
                }
                else if (!strcmp(clave,"crosshairGrosor")) g_crosshairGrosor=NumeroConfig(valor,1,8,2);
                else if (!strcmp(clave,"crosshairRGB")) g_crosshairRGB=atoi(valor)==1;
                else if (!strcmp(clave,"crosshairAutoForma")) g_crosshairAutoForma=atoi(valor)==1;
                else if (!strcmp(clave,"crosshairR")) g_crosshairColor.x=NumeroConfig(valor,0,1,.95f);
                else if (!strcmp(clave,"crosshairG")) g_crosshairColor.y=NumeroConfig(valor,0,1,.16f);
                else if (!strcmp(clave,"crosshairB")) g_crosshairColor.z=NumeroConfig(valor,0,1,.14f);
                else if (!strcmp(clave,"tpActivado")) tp::g_activado = atoi(valor)==1;
                else if (!strcmp(clave,"tpDistancia")) tp::g_distancia = tp::LimitarDistancia((float)atof(valor));
                else if (!strcmp(clave,"tpTecla")) { int v=atoi(valor); if(TeclaAsignable(v)) g_tpHotkey=v; }
                // Las claves de presets antiguos (ESP, aimbot, glows, ...) se ignoran.
            }
        }
        fclose(f);
        if(g_tpHotkey==g_hotkey) g_tpHotkey=0;
        if(g_vmHotkey==g_hotkey || g_vmHotkey==g_tpHotkey) g_vmHotkey=0;
        g_crosshairAnimInicio=GetTickCount64();
        vmh::g_factor = g_vmActivado ? 1.0f : 0.0f;
    }
    static void GuardarConfig() {
        g_saved=false;
        configfile::AtomicFile pending;
        FILE* f = pending.Open(RutaConfig());
        if (!f) { g_saveError = true; return; }
        bool ok = fprintf(f,
            "vmOffx = %g\nvmOffy = %g\nvmOffz = %g\nvmActivado = %d\nvmTecla = %d\nhotkey = %d\n"
            "crosshairActivado = %d\ncrosshairTipo = %d\ncrosshairTamano = %g\ncrosshairGrosor = %g\n"
            "crosshairRGB = %d\ncrosshairAutoForma = %d\n"
            "crosshairR = %g\ncrosshairG = %g\ncrosshairB = %g\n"
            "tpActivado = %d\ntpDistancia = %g\ntpTecla = %d\n",
            (float)vmh::g_offx, (float)vmh::g_offy, (float)vmh::g_offz,
            g_vmActivado ? 1 : 0, g_vmHotkey, g_hotkey,
            g_crosshairActivado ? 1 : 0, g_crosshairTipo, g_crosshairTamano, g_crosshairGrosor,
            g_crosshairRGB ? 1 : 0, g_crosshairAutoForma ? 1 : 0,
            g_crosshairColor.x, g_crosshairColor.y, g_crosshairColor.z,
            tp::g_activado ? 1 : 0, tp::LimitarDistancia(tp::g_distancia), g_tpHotkey) > 0;
        ok=pending.Commit(ok);
        g_saveError = !ok;
        if (ok) { g_dirty = false; g_saved = true; }
    }

    void Inicializar(HWND hwnd) {
        g_hwnd = hwnd;
        ImGuiIO& io = ImGui::GetIO();
        ImFontConfig base_cfg; base_cfg.FontDataOwnedByAtlas=false;
        io.Fonts->AddFontFromMemoryTTF((void*)inter_font, sizeof(inter_font), 14.0f, &base_cfg, io.Fonts->GetGlyphRangesDefault());
        {
            static const ImWchar fa_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
            ImFontConfig fa_cfg; fa_cfg.MergeMode=true; fa_cfg.FontDataOwnedByAtlas=false; fa_cfg.GlyphMinAdvanceX=14.0f;
            io.Fonts->AddFontFromMemoryTTF((void*)fa_icons, sizeof(fa_icons), 14.0f, &fa_cfg, fa_ranges);
        }
        CargarConfig();
    }
    bool estaVisible(){ return g_visible; }
    bool estaEnMenuJuego(){ return g_visible && g_menuJuego; }
    bool tieneFoco(){
        HWND foreground = GetForegroundWindow();
        return g_hwnd && foreground &&
            GetAncestor(foreground, GA_ROOT) == GetAncestor(g_hwnd, GA_ROOT);
    }
    static void LiberarBloqueo(){
        if(!g_cursorBloqueado) return;
        ClipCursor(nullptr);
        SetCursorPos(g_cursorAntes.x, g_cursorAntes.y);
        g_cursorBloqueado=false;
    }
    void PerderFoco(){
        g_tpCapturando=false; g_tpCapturaInicio=0; g_tpCapturaAbajo=false;
        g_tpTeclaAntes=true;
        g_vmCapturando=false; g_vmCapturaInicio=0; g_vmCapturaAbajo=false;
        g_vmTeclaAntes=true;
        // No devolver el cursor a la posicion del juego al cambiar de aplicacion.
        if(g_cursorBloqueado){ ClipCursor(nullptr); g_cursorBloqueado=false; }
        g_visible=false; g_render=false; g_menuJuego=false;
        g_capturando=false; g_capturaInicio=0; g_capturaAbajo=false;
        g_teclaAntes=true;
        CURSORINFO ci={sizeof(ci)};
        if(GetCursorInfo(&ci) && !(ci.flags & CURSOR_SHOWING)) ShowCursor(TRUE);
        if(ImGui::GetCurrentContext()) ImGui::GetIO().MouseDrawCursor=false;
    }
    bool CancelarCaptura(){
        bool activa=g_capturando || g_tpCapturando || g_vmCapturando;
        g_capturando=false; g_tpCapturando=false; g_vmCapturando=false;
        g_capturaInicio=g_tpCapturaInicio=g_vmCapturaInicio=0;
        g_capturaAbajo=g_tpCapturaAbajo=g_vmCapturaAbajo=false;
        if(activa) g_teclaAntes=g_tpTeclaAntes=g_vmTeclaAntes=true;
        return activa;
    }
    void Cerrar(){ if(!g_visible) return; CancelarCaptura(); g_visible=false; g_menuJuego=false; g_animInicio=GetTickCount64(); g_render=true; LiberarBloqueo(); ClipCursor(nullptr); g_cursorBloqueado=false; }

    static void ApplyModernStyle(){
        ImGuiStyle& s = ImGui::GetStyle();
        ImVec4* c = s.Colors;
        s.WindowRounding=8.0f; s.ChildRounding=4.0f; s.FrameRounding=4.0f; s.PopupRounding=4.0f; s.ScrollbarRounding=3.0f; s.GrabRounding=3.0f;
        s.WindowBorderSize=1.0f; s.FrameBorderSize=0.0f; s.PopupBorderSize=1.0f;
        s.WindowPadding=ImVec2(22,20); s.FramePadding=ImVec2(10,6); s.ItemSpacing=ImVec2(10,10); s.ItemInnerSpacing=ImVec2(8,6); s.ScrollbarSize=5.0f; s.GrabMinSize=10.0f;
        c[ImGuiCol_Text]=C_TXT; c[ImGuiCol_TextDisabled]=C_DIM;
        c[ImGuiCol_WindowBg]=C_BG; c[ImGuiCol_ChildBg]=ImVec4(0,0,0,0);
        c[ImGuiCol_PopupBg]=ImVec4(0.05f,0.05f,0.05f,1.0f);
        c[ImGuiCol_Border]=C_BORDER; c[ImGuiCol_BorderShadow]=ImVec4(0,0,0,0);
        c[ImGuiCol_FrameBg]=C_PANEL; c[ImGuiCol_FrameBgHovered]=ImVec4(0.10f,0.10f,0.10f,1.0f); c[ImGuiCol_FrameBgActive]=ImVec4(0.14f,0.14f,0.14f,1.0f);
        c[ImGuiCol_TitleBg]=C_BG; c[ImGuiCol_TitleBgActive]=C_BG; c[ImGuiCol_TitleBgCollapsed]=C_BG;
        c[ImGuiCol_Button]=C_PANEL; c[ImGuiCol_ButtonHovered]=ImVec4(0.13f,0.13f,0.13f,1.0f); c[ImGuiCol_ButtonActive]=ImVec4(0.16f,0.16f,0.16f,1.0f);
        c[ImGuiCol_SliderGrab]=C_ACCENT; c[ImGuiCol_SliderGrabActive]=C_ACCENT_H;
        c[ImGuiCol_CheckMark]=C_ACCENT; c[ImGuiCol_ScrollbarBg]=ImVec4(0,0,0,0); c[ImGuiCol_ScrollbarGrab]=ImVec4(0.18f,0.18f,0.18f,1.0f); c[ImGuiCol_ScrollbarGrabHovered]=C_DIM; c[ImGuiCol_ScrollbarGrabActive]=C_ACCENT;
        c[ImGuiCol_Separator]=C_BORDER;
        c[ImGuiCol_Header]=C_PANEL;
        c[ImGuiCol_TextSelectedBg]=ImVec4(0.14f,0.14f,0.14f,1.0f);
    }
    static const char* NombreTecla(int vk){
        static char buf[64];
        if(vk<=0) return "-";
        if(vk==VK_INSERT) return "Insert";
        UINT sc = MapVirtualKey((UINT)vk, MAPVK_VK_TO_VSC_EX);
        LONG lp=(LONG)(((sc & 0xFF)<<16) | ((sc & 0xFF00) ? (1 << 24) : 0));
        if(GetKeyNameTextA(lp, buf, sizeof(buf))>0) return buf;
        return "?";
    }

    static int TipoCrosshair(ULONGLONG now) {
        if (!g_crosshairAutoForma) return ImClamp(g_crosshairTipo, 0, 3);
        const ULONGLONG elapsed = now >= g_crosshairAnimInicio ? now - g_crosshairAnimInicio : 0;
        return (g_crosshairTipo + int(elapsed / 700ULL)) % 4;
    }
    static ImVec4 ColorCrosshair(ULONGLONG now) {
        if (!g_crosshairRGB) return ImVec4(g_crosshairColor.x, g_crosshairColor.y, g_crosshairColor.z, 1.0f);
        const float hue = float(now % 4800ULL) * (6.0f / 4800.0f);
        const int sector = int(hue);
        const float f = hue - float(sector);
        switch (sector % 6) {
            case 0: return ImVec4(1.0f, f, 0.0f, 1.0f);
            case 1: return ImVec4(1.0f - f, 1.0f, 0.0f, 1.0f);
            case 2: return ImVec4(0.0f, 1.0f, f, 1.0f);
            case 3: return ImVec4(0.0f, 1.0f - f, 1.0f, 1.0f);
            case 4: return ImVec4(f, 0.0f, 1.0f, 1.0f);
            default: return ImVec4(1.0f, 0.0f, 1.0f - f, 1.0f);
        }
    }
    static void DibujarFormaCrosshair(ImDrawList* dl, const ImVec2& center, int type,
                                     float length, float thickness, ImU32 color) {
        if (!dl) return;
        const ImU32 outline = IM_COL32(0, 0, 0, 225);
        const float gap = 3.0f;
        auto line = [&](const ImVec2& a, const ImVec2& b) {
            dl->AddLine(a, b, outline, thickness + 2.0f);
            dl->AddLine(a, b, color, thickness);
        };
        if (type == 0) {
            line(ImVec2(center.x - gap - length, center.y), ImVec2(center.x - gap, center.y));
            line(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + length, center.y));
            line(ImVec2(center.x, center.y - gap - length), ImVec2(center.x, center.y - gap));
            line(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + length));
        } else if (type == 1) {
            line(ImVec2(center.x - gap - length, center.y - gap - length), ImVec2(center.x - gap, center.y - gap));
            line(ImVec2(center.x + gap, center.y + gap), ImVec2(center.x + gap + length, center.y + gap + length));
            line(ImVec2(center.x + gap + length, center.y - gap - length), ImVec2(center.x + gap, center.y - gap));
            line(ImVec2(center.x - gap, center.y + gap), ImVec2(center.x - gap - length, center.y + gap + length));
        } else if (type == 2) {
            const float radius = ImMax(2.0f, length * 0.22f);
            dl->AddCircleFilled(center, radius + thickness, outline, 24);
            dl->AddCircleFilled(center, radius, color, 24);
        } else {
            const float radius = ImMax(3.0f, length);
            dl->AddCircle(center, radius, outline, 32, thickness + 2.0f);
            dl->AddCircle(center, radius, color, 32, thickness);
        }
    }
    static void DibujarCrosshair() {
        if (!g_crosshairActivado || !ImGui::GetCurrentContext()) return;
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (display.x <= 0.0f || display.y <= 0.0f) return;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl) return;
        const ImVec2 center(floorf(display.x * 0.5f) + 0.5f,
                            floorf(display.y * 0.5f) + 0.5f);
        const ImU32 color = ImGui::ColorConvertFloat4ToU32(ColorCrosshair(GetTickCount64()));
        DibujarFormaCrosshair(dl, center, TipoCrosshair(GetTickCount64()),
            g_crosshairTamano, g_crosshairGrosor, color);
    }

    #include "native_pages.inl"

    void Dibujar(){
        static bool estilo=false;
        if(!estilo){ ApplyModernStyle(); estilo=true; }
        if(!tieneFoco()){
            if(g_visible || g_cursorBloqueado || g_capturando || g_tpCapturando || g_vmCapturando) PerderFoco();
            g_teclaAntes=true;
            g_tpTeclaAntes=true;
            g_vmTeclaAntes=true;
            DibujarCrosshair();
            return;
        }
        const bool capturando = g_capturando || g_tpCapturando || g_vmCapturando;
        if(capturando && (GetAsyncKeyState(VK_ESCAPE)&0x8000)) CancelarCaptura();
        bool now = (g_hotkey!=0) && ((GetAsyncKeyState(g_hotkey) & 0x8000)!=0);
        if(now && !g_teclaAntes && !capturando && !ImGui::GetIO().WantTextInput){
            if(g_visible) Cerrar();
            else {
                CURSORINFO ci={sizeof(ci)};
                g_menuJuego = GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING);
                g_visible=true; g_animInicio=GetTickCount64(); g_render=true;
            }
        }
        g_teclaAntes=now;
        bool tpAhora = g_tpHotkey!=0 && (GetAsyncKeyState(g_tpHotkey)&0x8000)!=0;
        if(tpAhora && !g_tpTeclaAntes && !capturando && !ImGui::GetIO().WantTextInput){
            tp::g_activado=!tp::g_activado;
            NotificarCambio("Tercera persona",tp::g_activado);
            g_dirty=true; g_saveError=false;
        }
        g_tpTeclaAntes=tpAhora;
        bool vmAhora = g_vmHotkey!=0 && (GetAsyncKeyState(g_vmHotkey)&0x8000)!=0;
        if(vmAhora && !g_vmTeclaAntes && !capturando && !ImGui::GetIO().WantTextInput){
            g_vmActivado=!g_vmActivado;
            vmh::g_factor=g_vmActivado ? 1.0f : 0.0f;
            NotificarCambio("Viewmodel",g_vmActivado);
            g_dirty=true; g_saveError=false;
        }
        g_vmTeclaAntes=vmAhora;
        if(g_capturando){
            if(g_capturaInicio==0) g_capturaInicio=GetTickCount64();
            bool alguna=false;
            for(int vk=0x08; vk<=0xFE; ++vk){
                if(vk==g_hotkey || vk==g_tpHotkey || vk==g_vmHotkey || !TeclaAsignable(vk)) continue;
                if(GetAsyncKeyState(vk)&0x8000){ alguna=true; break; }
            }
            if((GetTickCount64()-g_capturaInicio)>150 && alguna && !g_capturaAbajo){
                for(int vk=0x08; vk<=0xFE; ++vk){
                    if(vk==g_hotkey || vk==g_tpHotkey || vk==g_vmHotkey || !TeclaAsignable(vk)) continue;
                    if(GetAsyncKeyState(vk)&0x8000){ g_hotkey=vk; g_capturando=false; g_capturaInicio=0; g_teclaAntes=true; GuardarConfig(); break; }
                }
            }
            g_capturaAbajo=alguna;
        } else { g_capturaInicio=0; g_capturaAbajo=false; }
        if(g_tpCapturando){
            if(!g_tpCapturaInicio) g_tpCapturaInicio=GetTickCount64();
            int tecla=0;
            for(int vk=0x08;vk<=0xFE;++vk){
                if(vk!=g_hotkey && vk!=g_vmHotkey && TeclaAsignable(vk) && (GetAsyncKeyState(vk)&0x8000)){ tecla=vk; break; }
            }
            if(GetTickCount64()-g_tpCapturaInicio>150 && tecla && !g_tpCapturaAbajo){
                g_tpHotkey=tecla; g_tpCapturando=false; g_tpCapturaInicio=0;
                g_tpTeclaAntes=true; GuardarConfig();
            }
            g_tpCapturaAbajo=tecla!=0;
        } else { g_tpCapturaInicio=0; g_tpCapturaAbajo=false; }
        if(g_vmCapturando){
            if(!g_vmCapturaInicio) g_vmCapturaInicio=GetTickCount64();
            int tecla=0;
            for(int vk=0x08;vk<=0xFE;++vk){
                if(vk==g_hotkey || vk==g_tpHotkey || !TeclaAsignable(vk)) continue;
                if(GetAsyncKeyState(vk)&0x8000){ tecla=vk; break; }
            }
            if(GetTickCount64()-g_vmCapturaInicio>150 && tecla && !g_vmCapturaAbajo){
                g_vmHotkey=tecla; g_vmCapturando=false; g_vmCapturaInicio=0;
                g_vmTeclaAntes=true; GuardarConfig();
            }
            g_vmCapturaAbajo=tecla!=0;
        } else { g_vmCapturaInicio=0; g_vmCapturaAbajo=false; }
        const float DUR_ANIM=200.0f;
        float t_anim=(float)(GetTickCount64()-g_animInicio)/DUR_ANIM;
        if(t_anim>1.0f) t_anim=1.0f;
        float e_anim=t_anim*t_anim*(3.0f-2.0f*t_anim);
        float alpha=g_visible ? e_anim : (1.0f - e_anim);
        float slide=g_visible ? (1.0f-e_anim)*10.0f : -e_anim*10.0f;
        if(!g_visible && t_anim>=1.0f) g_render=false;
        if(!g_render) {
            LiberarBloqueo();
            if(ImGui::GetCurrentContext()) ImGui::GetIO().MouseDrawCursor = false;
            DibujarCrosshair();
            g_notices.Draw(GetTickCount64());
            return;
        }
        // Cursor por frame — negro total, sin glows
        if(g_visible){
            if(g_menuJuego){
                CURSORINFO e={sizeof(e)};
                if(!GetCursorInfo(&e) || !(e.flags & CURSOR_SHOWING)) g_menuJuego=false;
            }
            if(g_menuJuego){
                LiberarBloqueo();
                if(ImGui::GetCurrentContext()) ImGui::GetIO().MouseDrawCursor=false;
            } else {
                ReleaseCapture();
                if(!g_cursorBloqueado){ GetCursorPos(&g_cursorAntes); g_cursorBloqueado=true; }
                SetCursorPos(g_cursorAntes.x, g_cursorAntes.y);
                RECT limite={g_cursorAntes.x, g_cursorAntes.y, g_cursorAntes.x+1, g_cursorAntes.y+1};
                ClipCursor(&limite);
                CURSORINFO ci={sizeof(ci)};
                bool physVisible = GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING);
                if(physVisible) ::ShowCursor(FALSE);
                if(ImGui::GetCurrentContext()) ImGui::GetIO().MouseDrawCursor=true;
            }
        } else {
            LiberarBloqueo();
            if(ImGui::GetCurrentContext()) ImGui::GetIO().MouseDrawCursor=false;
            CURSORINFO ci={sizeof(ci)};
            if(GetCursorInfo(&ci) && !(ci.flags & CURSOR_SHOWING)) ::ShowCursor(TRUE);
        }
        DrawNative(alpha,slide);
        DibujarCrosshair();
        g_notices.Draw(GetTickCount64());
    }
}
