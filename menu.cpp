#include "menu.h"
#include "viewmodel_hook.h"
#include "dx9_hook.h"
#include "imgui.h"
#include "imgui_internal.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "inter_font.h"
#include "fa_icons.h"
#include <windows.h>
#include <cstdio>
#include <string>
#include <math.h>

#define ICON_MIN_FA 0xf000
#define ICON_MAX_FA 0xf950
#define ICON_FA_GAMEPAD "\xef\x84\x9b"
#define ICON_FA_SLIDERS "\xef\x87\x9e"
#define ICON_FA_CLOSE "\xef\x80\x8d"
#define ICON_FA_SAVE "\xef\x83\x87"
#define ICON_FA_RESET "\xef\x83\xa2"
#define ICON_FA_KEYBOARD "\xef\x84\x9c"

namespace menu {
    static bool g_visible = false;
    static bool g_menuJuego = false;
    static bool g_cursorBloqueado = false;
    static POINT g_cursorAntes = {};
    static bool g_cursorEraVisible = false;
    static bool g_render = false;
    static uintptr_t g_animInicio = 0;
    static HWND g_hwnd = nullptr;
    static void* g_surface = nullptr;
    static ImFont* g_fontMain = nullptr;
    static ImFont* g_fontMono = nullptr;
    static ImFont* g_fontTitle = nullptr;
    static ImFont* g_fontLogo = nullptr;
    static ImFont* g_fontHeader = nullptr;

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
    static int g_crosshairTipo = 0;       // 0 = cruz (+), 1 = X
    static float g_crosshairTamano = 8.0f;
    static ImVec4 g_crosshairColor(0.95f, 0.16f, 0.14f, 1.0f);
    static ImVec2 g_panelPos(-1.0f, -1.0f);

    static int g_hotkey = VK_INSERT;
    static bool g_capturando = false;
    static bool g_teclaAntes = false;
    static bool g_capturaAbajo = false;
    static uintptr_t g_capturaInicio = 0;

    static std::string RutaConfig() {
        HMODULE dll = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&RutaConfig, &dll);
        char buf[MAX_PATH];
        DWORD n = dll ? GetModuleFileNameA(dll, buf, MAX_PATH) : 0;
        if (!n) { n = GetModuleFileNameA(NULL, buf, MAX_PATH); if (!n) return std::string("vmconfig.ini"); }
        std::string dir(buf, n);
        size_t pos = dir.find_last_of("\\/");
        if (pos != std::string::npos) dir = dir.substr(0, pos);
        return dir + "\\vmconfig.ini";
    }
    static void CargarConfig() {
        FILE* f = fopen(RutaConfig().c_str(), "r");
        if (!f) return;
        g_crosshairActivado = false;
        g_crosshairTipo = 0;
        g_crosshairTamano = 8.0f;
        g_crosshairColor = ImVec4(0.95f, 0.16f, 0.14f, 1.0f);
        char linea[256];
        while (fgets(linea, sizeof(linea), f)) {
            char clave[64]; char valor[64];
            if (sscanf(linea, "%63s = %63s", clave, valor)==2) {
                if (!strcmp(clave,"vmOffx")) vmh::g_offx=(float)atof(valor);
                else if (!strcmp(clave,"vmOffy")) vmh::g_offy=(float)atof(valor);
                else if (!strcmp(clave,"vmOffz")) vmh::g_offz=(float)atof(valor);
                else if (!strcmp(clave,"hotkey")) { int v=atoi(valor); if(v>=8&&v<=254) g_hotkey=v; }
                else if (!strcmp(clave,"crosshairActivado")) g_crosshairActivado = atoi(valor)!=0;
                else if (!strcmp(clave,"crosshairTipo")) { int v=atoi(valor); g_crosshairTipo = (v==1)?1:0; }
                else if (!strcmp(clave,"crosshairTamano")) {
                    float v=(float)atof(valor);
                    if(v>=2.0f && v<=40.0f) g_crosshairTamano=v;
                }
                else if (!strcmp(clave,"crosshairR")) g_crosshairColor.x=(float)atof(valor);
                else if (!strcmp(clave,"crosshairG")) g_crosshairColor.y=(float)atof(valor);
                else if (!strcmp(clave,"crosshairB")) g_crosshairColor.z=(float)atof(valor);
            }
        }
        fclose(f);
    }
    static void GuardarConfig() {
        FILE* f = fopen(RutaConfig().c_str(), "w");
        if (!f) { g_saveError = true; return; }
        bool ok = fprintf(f,
            "vmOffx = %g\nvmOffy = %g\nvmOffz = %g\nhotkey = %d\n"
            "crosshairActivado = %d\ncrosshairTipo = %d\ncrosshairTamano = %g\n"
            "crosshairR = %g\ncrosshairG = %g\ncrosshairB = %g\n",
            (float)vmh::g_offx, (float)vmh::g_offy, (float)vmh::g_offz, g_hotkey,
            g_crosshairActivado ? 1 : 0, g_crosshairTipo, g_crosshairTamano,
            g_crosshairColor.x, g_crosshairColor.y, g_crosshairColor.z) > 0;
        if (fclose(f) != 0) ok = false;
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
        ImFontConfig cfg; cfg.SizePixels=16.0f; cfg.FontDataOwnedByAtlas=false;
        g_fontTitle = io.Fonts->AddFontFromMemoryTTF((void*)inter_font, sizeof(inter_font), 16.0f, &cfg);
        {
            static const ImWchar fa_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
            ImFontConfig fa_cfg; fa_cfg.MergeMode=true; fa_cfg.FontDataOwnedByAtlas=false; fa_cfg.GlyphMinAdvanceX=16.0f;
            io.Fonts->AddFontFromMemoryTTF((void*)fa_icons, sizeof(fa_icons), 16.0f, &fa_cfg, fa_ranges);
        }
        g_fontMono = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 13.0f, nullptr, io.Fonts->GetGlyphRangesDefault());
        g_fontLogo = io.Fonts->AddFontFromMemoryTTF((void*)inter_font, sizeof(inter_font), 20.0f, &cfg);
        g_fontHeader = io.Fonts->AddFontFromMemoryTTF((void*)inter_font, sizeof(inter_font), 18.0f, &cfg);
        CargarConfig();
        HMODULE vms = GetModuleHandleA("vguimatsurface.dll");
        if (vms) {
            typedef void* (__cdecl* CIFn)(const char*, int*);
            CIFn ci = (CIFn)GetProcAddress(vms, "CreateInterface");
            if (ci) {
                const char* vers[] = {"VGUI_Surface031","VGUI_Surface030","VGUI_Surface029","VGUI_Surface", nullptr};
                for(int i=0; vers[i]; ++i){ g_surface=ci(vers[i],nullptr); if(g_surface) break; }
            }
        }
    }
    bool estaVisible(){ return g_visible; }
    bool estaEnMenuJuego(){ return g_visible && g_menuJuego; }
    static void LiberarBloqueo(){
        if(!g_cursorBloqueado) return;
        ClipCursor(nullptr);
        SetCursorPos(g_cursorAntes.x, g_cursorAntes.y);
        g_cursorBloqueado=false;
    }
    void PerderFoco(){
        LiberarBloqueo();
        CURSORINFO ci={sizeof(ci)};
        if(GetCursorInfo(&ci) && !(ci.flags & CURSOR_SHOWING)) ShowCursor(TRUE);
        if(ImGui::GetCurrentContext()) ImGui::GetIO().MouseDrawCursor=false;
    }
    void Cerrar(){ if(!g_visible) return; g_visible=false; g_menuJuego=false; g_animInicio=GetTickCount64(); g_render=true; LiberarBloqueo(); ClipCursor(nullptr); g_cursorBloqueado=false; }
    void Apagar(){ GuardarConfig(); LiberarBloqueo(); ClipCursor(nullptr); g_cursorBloqueado=false; g_visible=false; g_menuJuego=false; }

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
    static bool SliderAxis(float* value, const ImVec4& color) {
        ImGui::SetNextItemWidth(-1);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,4));
        const ImGuiCol hidden[] = {ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered,
            ImGuiCol_FrameBgActive, ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive};
        for (auto item : hidden) ImGui::PushStyleColor(item, ImVec4(0,0,0,0));
        bool changed = ImGui::SliderFloat("##slider", value, -20, 20, "", ImGuiSliderFlags_NoInput);
        ImGui::PopStyleColor(5); ImGui::PopStyleVar();
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        const float left = a.x + 7, right = b.x - 7, mid = (left + right) * .5f;
        const float y = (a.y + b.y) * .5f;
        const float x = left + ImClamp((*value + 20) / 40, 0.0f, 1.0f) * (right - left);
        auto* dl = ImGui::GetWindowDrawList();
        dl->AddLine(ImVec2(left,y), ImVec2(right,y), ImGui::GetColorU32(C_BORDER), 3);
        dl->AddLine(ImVec2(mid,y-5), ImVec2(mid,y+5), ImGui::GetColorU32(C_DIM), 1);
        dl->AddLine(ImVec2(mid,y), ImVec2(x,y), ImGui::GetColorU32(color), 3);
        dl->AddCircleFilled(ImVec2(x,y), 4.5f, ImGui::GetColorU32(color), 12);
        return changed;
    }
    static bool IconButton(const char* id, const char* icon, const char* tooltip, float size=34) {
        ImGui::PushID(id);
        bool clicked = ImGui::Button(icon, ImVec2(size,size));
        if (ImGui::IsItemHovered()) {
            ImGui::PushStyleColor(ImGuiCol_Text, C_TXT);
            ImGui::SetTooltip("%s", tooltip);
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
        return clicked;
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

    static void DibujarCrosshair() {
        if (!g_crosshairActivado || !ImGui::GetCurrentContext()) return;
        ImVec2 display = ImGui::GetIO().DisplaySize;
        if (display.x <= 0.0f || display.y <= 0.0f) return;
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl) return;
        const ImVec2 centro(floorf(display.x * 0.5f) + 0.5f,
                            floorf(display.y * 0.5f) + 0.5f);
        const float inicio = 3.0f;
        const float longitud = ImMax(0.0f, floorf(g_crosshairTamano + 0.5f));
        const float fin = inicio + longitud;
        const float grosor = 2.0f;
        const float contornoGrosor = 2.0f;
        const ImU32 color = ImGui::ColorConvertFloat4ToU32(
            ImVec4(g_crosshairColor.x, g_crosshairColor.y, g_crosshairColor.z, 1.0f));
        const ImU32 contorno = IM_COL32(0, 0, 0, 220);
        auto linea = [&](const ImVec2& a, const ImVec2& b) {
            dl->AddLine(a, b, contorno, grosor + contornoGrosor * 2.0f);
            dl->AddLine(a, b, color, grosor);
        };
        if (g_crosshairTipo == 0) {
            linea(ImVec2(centro.x - fin, centro.y), ImVec2(centro.x - inicio, centro.y));
            linea(ImVec2(centro.x + inicio, centro.y), ImVec2(centro.x + fin, centro.y));
            linea(ImVec2(centro.x, centro.y - fin), ImVec2(centro.x, centro.y - inicio));
            linea(ImVec2(centro.x, centro.y + inicio), ImVec2(centro.x, centro.y + fin));
        } else {
            linea(ImVec2(centro.x - fin, centro.y - fin), ImVec2(centro.x - inicio, centro.y - inicio));
            linea(ImVec2(centro.x + inicio, centro.y + inicio), ImVec2(centro.x + fin, centro.y + fin));
            linea(ImVec2(centro.x + fin, centro.y - fin), ImVec2(centro.x + inicio, centro.y - inicio));
            linea(ImVec2(centro.x - inicio, centro.y + inicio), ImVec2(centro.x - fin, centro.y + fin));
        }
    }

    static bool BtnTipo(const char* id, const char* label, bool activo) {
        if (activo) {
            ImGui::PushStyleColor(ImGuiCol_Button, C_ACCENT);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, C_ACCENT_H);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.42f,.72f,.59f,1));
            ImGui::PushStyleColor(ImGuiCol_Text, C_BG);
        }
        ImGui::PushID(id);
        bool clicked = ImGui::Button(label, ImVec2(30,26));
        ImGui::PopID();
        if (activo) ImGui::PopStyleColor(4);
        return clicked;
    }

    void Dibujar(){
        static bool estilo=false;
        if(!estilo){ ApplyModernStyle(); estilo=true; }
        static bool prev=false;
        bool now = (g_hotkey!=0) && ((GetAsyncKeyState(g_hotkey) & 0x8000)!=0);
        if(now && !prev){
            if(g_visible) Cerrar();
            else {
                CURSORINFO ci={sizeof(ci)};
                g_menuJuego = GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING);
                g_visible=true; g_animInicio=GetTickCount64(); g_render=true;
            }
        }
        prev=now;
        if(g_capturando){
            if(g_capturaInicio==0) g_capturaInicio=GetTickCount64();
            bool alguna=false;
            for(int vk=0x08; vk<=0xFE; ++vk){
                if(vk==g_hotkey) continue;
                if(vk==VK_CONTROL||vk==VK_MENU||vk==VK_SHIFT||vk==VK_LWIN||vk==VK_RWIN) continue;
                if(GetAsyncKeyState(vk)&0x8000){ alguna=true; break; }
            }
            if((GetTickCount64()-g_capturaInicio)>150 && alguna && !g_capturaAbajo){
                for(int vk=0x08; vk<=0xFE; ++vk){
                    if(vk==VK_CONTROL||vk==VK_MENU||vk==VK_SHIFT||vk==VK_LWIN||vk==VK_RWIN) continue;
                    if(GetAsyncKeyState(vk)&0x8000){ g_hotkey=vk; g_capturando=false; g_capturaInicio=0; g_teclaAntes=true; GuardarConfig(); break; }
                }
            }
            g_capturaAbajo=alguna;
        } else { g_capturaInicio=0; g_capturaAbajo=false; }
        const float DUR_ANIM=200.0f;
        float t_anim=(float)(GetTickCount64()-g_animInicio)/DUR_ANIM;
        if(t_anim>1.0f) t_anim=1.0f;
        float e_anim=t_anim*t_anim*(3.0f-2.0f*t_anim);
        float alpha=g_visible ? e_anim : (1.0f - e_anim);
        float slide=g_visible ? (1.0f-e_anim)*10.0f : -e_anim*10.0f;
        if(!g_visible && t_anim>=1.0f) g_render=false;
        if(!g_render) {
            LiberarBloqueo();
            ClipCursor(nullptr);
            g_cursorBloqueado=false;
            if(ImGui::GetCurrentContext()) ImGui::GetIO().MouseDrawCursor = false;
            DibujarCrosshair();
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

        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImVec2 winSize(ImMin(420.0f, ImMax(280.0f, display.x-16)), ImMin(510.0f, ImMax(240.0f, display.y-16)));
        if(g_panelPos.x < 0.0f){
            g_panelPos = ImVec2((display.x - winSize.x)*0.5f, (display.y - winSize.y)*0.5f);
            if(g_panelPos.x<8) g_panelPos.x=8; if(g_panelPos.y<8) g_panelPos.y=8;
        }
        ImVec2 winPos(g_panelPos.x, g_panelPos.y + slide);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::SetNextWindowSize(winSize, ImGuiCond_Always);
        ImGui::SetNextWindowPos(winPos, ImGuiCond_Always);
        ImGui::Begin("VM", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
        const float pad = 22, right = ImGui::GetWindowContentRegionMax().x;
        ImVec2 wp = ImGui::GetWindowPos();
        ImGui::SetCursorPos(ImVec2(0,0));
        ImGui::InvisibleButton("##drag", ImVec2(winSize.x-60,72));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            g_panelPos = ImVec2(
                ImClamp(g_panelPos.x+delta.x,0.0f,ImMax(0.0f,display.x-winSize.x)),
                ImClamp(g_panelPos.y+delta.y,0.0f,ImMax(0.0f,display.y-winSize.y)));
        }
        ImGui::SetCursorPos(ImVec2(pad,24));
        ImGui::TextColored(C_ACCENT, ICON_FA_SLIDERS);
        ImGui::SetCursorPos(ImVec2(pad+30,18));
        if(g_fontLogo) ImGui::PushFont(g_fontLogo);
        ImGui::TextUnformatted("Viewmodel");
        if(g_fontLogo) ImGui::PopFont();
        ImGui::SetCursorPos(ImVec2(pad+30,45));
        ImGui::TextColored(C_DIM, "L4D2");
        ImGui::SetCursorPos(ImVec2(right-32,20));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, C_DIM);
        if(IconButton("close", ICON_FA_CLOSE, "Cerrar", 32)) Cerrar();
        ImGui::PopStyleColor(2);
        ImGui::SetCursorPos(ImVec2(pad,75)); ImGui::Separator();
        ImGui::SetCursorPos(ImVec2(pad,91));
        ImGui::TextColored(C_DIM, "POSICION");
        const char* letters[] = {"X", "Y", "Z"};
        const char* labels[] = {"Profundidad", "Lateral", "Altura"};
        volatile float* offsets[] = {&vmh::g_offx, &vmh::g_offy, &vmh::g_offz};
        for (int i=0; i<3; ++i) {
            const float y = 118.0f + i*64.0f;
            ImGui::PushID(letters[i]);
            ImGui::SetCursorPos(ImVec2(pad,y));
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImVec4 tint = C_AXES[i]; tint.w = .12f;
            auto* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p, ImVec2(p.x+24,p.y+24), ImGui::GetColorU32(tint), 4);
            dl->AddText(ImVec2(p.x+7,p.y+5), ImGui::GetColorU32(C_AXES[i]), letters[i]);
            ImGui::SetCursorPos(ImVec2(pad+34,y+4));
            ImGui::TextUnformatted(labels[i]);
            float value = *offsets[i];
            ImGui::SetCursorPos(ImVec2(right-78,y));
            ImGui::SetNextItemWidth(78);
            if (g_fontMono) ImGui::PushFont(g_fontMono);
            bool changed = ImGui::DragFloat("##value", &value, .05f, -20, 20, "%+.2f", ImGuiSliderFlags_AlwaysClamp);
            if (g_fontMono) ImGui::PopFont();
            ImGui::SetCursorPos(ImVec2(pad,y+30));
            changed |= SliderAxis(&value, C_AXES[i]);
            if (changed) { *offsets[i] = value; g_dirty = true; g_saveError = false; }
            ImGui::PopID();
        }
        ImGui::SetCursorPos(ImVec2(pad,305));
        if (ImGui::Checkbox("Crosshair", &g_crosshairActivado)) { g_dirty = true; g_saveError = false; }
        ImGui::SetCursorPos(ImVec2(pad,337));
        ImGui::TextColored(C_DIM, "Tipo");
        ImGui::SameLine(0,8);
        if (BtnTipo("xht0", "+", g_crosshairTipo == 0)) {
            g_crosshairTipo = 0; g_dirty = true; g_saveError = false;
        }
        ImGui::SameLine(0,4);
        if (BtnTipo("xht1", "X", g_crosshairTipo == 1)) {
            g_crosshairTipo = 1; g_dirty = true; g_saveError = false;
        }
        ImGui::SameLine(0,16);
        ImGui::TextColored(C_DIM, "Tam");
        ImGui::SameLine(0,8);
        if (ImGui::Button("-", ImVec2(26,26))) {
            g_crosshairTamano = ImClamp(g_crosshairTamano - 1.0f, 2.0f, 40.0f);
            g_dirty = true; g_saveError = false;
        }
        ImGui::SameLine(0,4);
        {
            char sz[16];
            sprintf(sz, "%d", (int)g_crosshairTamano);
            ImGui::PushID("xhtam");
            ImGui::Button(sz, ImVec2(30,26));
            ImGui::PopID();
        }
        ImGui::SameLine(0,4);
        if (ImGui::Button("+", ImVec2(26,26))) {
            g_crosshairTamano = ImClamp(g_crosshairTamano + 1.0f, 2.0f, 40.0f);
            g_dirty = true; g_saveError = false;
        }
        ImGui::SameLine(0,14);
        if (ImGui::ColorEdit3("##xhcolor", (float*)&g_crosshairColor,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel |
                ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoTooltip)) {
            g_dirty = true; g_saveError = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color del crosshair");
        ImGui::SetCursorPos(ImVec2(pad,375)); ImGui::Separator();
        ImGui::SetCursorPos(ImVec2(pad,388));
        ImGui::PushStyleColor(ImGuiCol_Button,C_ACCENT);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,C_ACCENT_H);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(.42f,.72f,.59f,1));
        ImGui::PushStyleColor(ImGuiCol_Text,C_BG);
        if (IconButton("save", ICON_FA_SAVE, "Guardar preset")) GuardarConfig();
        ImGui::PopStyleColor(4);
        ImGui::SameLine(0,8);
        if (IconButton("reset", ICON_FA_RESET, "Restaurar X, Y, Z")) {
            vmh::g_offx=0; vmh::g_offy=0; vmh::g_offz=0; GuardarConfig();
        }
        ImGui::SetCursorPos(ImVec2(pad+92,398));
        ImGui::TextColored(g_saveError ? C_AXES[0] : (g_dirty ? C_AXES[1] : C_DIM), "%s",
            g_saveError ? "Error al guardar" : (g_dirty ? "Sin guardar" : (g_saved ? "Guardado" : "Preset actual")));
        ImGui::SetCursorPos(ImVec2(pad,449));
        ImGui::TextColored(C_DIM, ICON_FA_KEYBOARD "  Menu");
        ImGui::SetCursorPos(ImVec2(right-132,440));
        if (ImGui::Button(g_capturando ? "..." : NombreTecla(g_hotkey), ImVec2(92,30)))
            g_capturando = !g_capturando;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", g_capturando ? "Cancelar asignacion" : "Cambiar tecla del menu");
        ImGui::SameLine(0,8);
        if (IconButton("clear", ICON_FA_CLOSE, "Restaurar tecla (Insert)", 30)) {
            g_hotkey=VK_INSERT; g_capturando=false; g_dirty=true;
        }
        ImGui::End();
        ImGui::PopStyleVar();
        {
            auto* dl = ImGui::GetForegroundDrawList();
            if (dl) {
                const ImU32 credit = IM_COL32(140, 145, 150, (int)(200.0f * alpha));
                dl->AddText(ImVec2(winPos.x + 10.0f, winPos.y + winSize.y + 6.0f), credit, "@tokyossz");
            }
        }
        DibujarCrosshair();
    }
}
