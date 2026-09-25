#include <windows.h>
#include <d3d9.h>
#include <gdiplus.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include "imgui.h"
#include "imgui_impl_dx9.h"

// Exercise the real menu without a game process or OS mouse/keyboard changes.
static ULONGLONG previewTime = 1000;
static int pressedKey = 0;
static SHORT PreviewKey(int key) { return key == pressedKey ? SHORT(0x8000) : 0; }
static ULONGLONG PreviewTime() { return previewTime; }
static BOOL PreviewCursor(CURSORINFO* ci) { ci->flags = CURSOR_SHOWING; return TRUE; }
static BOOL PreviewClip(const RECT*) { return TRUE; }
static BOOL PreviewSetCursor(int, int) { return TRUE; }
static BOOL PreviewGetCursor(POINT* p) { *p = POINT{0, 0}; return TRUE; }
static BOOL PreviewRelease() { return TRUE; }
static int PreviewShow(BOOL) { return 0; }
namespace ImGui { static void PreviewSetCursor(const ImVec2& position) { SetCursorPos(position); } }
#define GetAsyncKeyState PreviewKey
#define GetTickCount64 PreviewTime
#define GetCursorInfo PreviewCursor
#define ClipCursor PreviewClip
#define SetCursorPos PreviewSetCursor
#define GetCursorPos PreviewGetCursor
#define ReleaseCapture PreviewRelease
#define ShowCursor PreviewShow
#include "../menu.cpp"
#undef GetAsyncKeyState
#undef GetTickCount64
#undef GetCursorInfo
#undef ClipCursor
#undef SetCursorPos
#undef GetCursorPos
#undef ReleaseCapture
#undef ShowCursor

namespace vmh { volatile float g_offx = 0, g_offy = 0, g_offz = 0; }
static void Check(bool ok, const char* name) {
    if (!ok) { fprintf(stderr, "FAIL: %s\n", name); exit(1); }
}
static void Frame(int width, int height) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(float(width), float(height));
    io.DeltaTime = 1.0f / 60;
    ImGui_ImplDX9_NewFrame();
    ImGui::NewFrame();
    menu::Dibujar();
    ImGui::Render();
}
static void Click(float x, float y) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(x,y); Frame(900,600);
    io.AddMouseButtonEvent(0,true); Frame(900,600);
    io.AddMouseButtonEvent(0,false); Frame(900,600);
}
static void Capture(IDirect3DDevice9* dev, const std::wstring& name, int width, int height) {
    dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(35, 37, 40), 1, 0);
    Check(SUCCEEDED(dev->BeginScene()), "begin scene");
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    dev->EndScene();
    IDirect3DSurface9* rt = nullptr; IDirect3DSurface9* copy = nullptr;
    Check(SUCCEEDED(dev->GetRenderTarget(0, &rt)), "render target");
    Check(SUCCEEDED(dev->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &copy, nullptr)), "readback surface");
    Check(SUCCEEDED(dev->GetRenderTargetData(rt, copy)), "readback");
    D3DLOCKED_RECT locked = {};
    Check(SUCCEEDED(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY)), "lock image");
    {
        Gdiplus::Bitmap bitmap(width, height, locked.Pitch, PixelFormat32bppRGB, (BYTE*)locked.pBits);
        const CLSID png = {0x557cf406, 0x1a04, 0x11d3, {0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
        Check(bitmap.Save(name.c_str(), &png, nullptr) == Gdiplus::Ok, "save PNG");
    }
    copy->UnlockRect(); copy->Release(); rt->Release();
}
int main(int argc, char** argv) {
    Gdiplus::GdiplusStartupInput gdiplus; ULONG_PTR token;
    Check(Gdiplus::GdiplusStartup(&token, &gdiplus, nullptr) == Gdiplus::Ok, "GDI+");
    HWND window = CreateWindowA("STATIC", "Viewmodel UI test", WS_OVERLAPPEDWINDOW, 0, 0, 900, 600, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    Check(window != nullptr, "hidden preview window");
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    Check(d3d != nullptr, "D3D9");
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow = window;
    pp.BackBufferWidth = 900; pp.BackBufferHeight = 600; pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    IDirect3DDevice9* dev = nullptr;
    Check(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev)), "D3D9 device");
    ImGui::CreateContext(); ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplDX9_Init(dev); menu::Inicializar(window);
    // Deterministic defaults regardless of leftover preview config
    menu::g_crosshairActivado = false;
    menu::g_crosshairTipo = 0;
    menu::g_crosshairTamano = 8.0f;
    menu::g_crosshairColor = ImVec4(0.95f, 0.16f, 0.14f, 1.0f);
    vmh::g_offx = 3.25f; vmh::g_offy = -1.5f; vmh::g_offz = -4;
    menu::g_hotkey = VK_INSERT;
    Check(strcmp(menu::NombreTecla(VK_INSERT), "Insert") == 0, "Insert label is not confused with numpad zero");
    menu::g_visible = menu::g_render = menu::g_menuJuego = true; menu::g_animInicio = 0;
    menu::g_panelPos = ImVec2(40, 45);
    std::wstring prefix = argc > 1 && std::string(argv[1]) == "before" ? L"before" : L"after";
    for (int i = 0; i < 3; ++i) Frame(900, 600);
    Capture(dev, prefix + L"_normal.png", 900, 600);
    auto* dd = ImGui::GetDrawData();
    printf("UI geometry: vertices=%d indices=%d lists=%d\n", dd->TotalVtxCount, dd->TotalIdxCount, dd->CmdListsCount);
    LARGE_INTEGER freq, begin, end; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&begin);
    for (int i = 0; i < 2000; ++i) Frame(900, 600);
    QueryPerformanceCounter(&end);
    printf("Menu CPU average (2000 frames, no GPU timing): %.4f ms\n", (end.QuadPart-begin.QuadPart)*1000.0/freq.QuadPart/2000);
    Check(vmh::g_offx == 3.25f && vmh::g_offy == -1.5f && vmh::g_offz == -4, "idle render preserves offsets");
    menu::g_panelPos = ImVec2(8, 8);
    for (int i = 0; i < 3; ++i) Frame(320, 560);
    Capture(dev, prefix + L"_compact.png", 900, 600);
    auto* panel = ImGui::FindWindowByName("VM");
    Check(panel && panel->Size.x <= 304 && panel->ScrollMax.y == 0, "compact panel fits with all controls visible");
    for (int i = 0; i < 3; ++i) Frame(320, 300);
    Check(panel->ScrollMax.y > 0, "short viewports can scroll instead of clipping controls");
    ImGui::SetScrollY(panel, panel->ScrollMax.y);
    for (int i = 0; i < 3; ++i) Frame(320, 300);
    Capture(dev, prefix + L"_short.png", 900, 600);
    ImGui::SetScrollY(panel, 0);
    menu::g_panelPos = ImVec2(200,80);
    for (int i = 0; i < 3; ++i) Frame(900,600);
    Click(503,239);
    Check(vmh::g_offx > 5 && vmh::g_offx < 15 && menu::g_dirty, "slider updates X live and marks dirty");
    Check(vmh::g_offy == -1.5f && vmh::g_offz == -4, "X leaves Y/Z unchanged");
    Capture(dev, prefix + L"_edited.png", 900,600);
    Click(239,485);
    Check(menu::g_saved && !menu::g_dirty && !menu::g_saveError, "save reports success");
    const float savedX = vmh::g_offx;
    vmh::g_offx = 0; menu::CargarConfig();
    Check(fabsf(vmh::g_offx-savedX) < .0001f, "saved value reloads with existing %g config precision");
    auto& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl,true);
    Click(550,210);
    io.AddKeyEvent(ImGuiMod_Ctrl,false);
    io.AddInputCharactersUTF8("7.50"); Frame(900,600);
    io.AddKeyEvent(ImGuiKey_Enter,true); Frame(900,600);
    io.AddKeyEvent(ImGuiKey_Enter,false); Frame(900,600);
    Check(fabsf(vmh::g_offx-7.5f) < .01f, "numeric edit works");
    Click(283,485);
    Check(vmh::g_offx == 0 && vmh::g_offy == 0 && vmh::g_offz == 0 && !menu::g_dirty,
          "reset preserves original reset-and-save behavior");
    Click(510,535);
    Check(menu::g_capturando, "hotkey capture starts");
    Frame(900,600);
    previewTime += 200; pressedKey = VK_F6; Frame(900,600); pressedKey = 0; Frame(900,600);
    printf("Hotkey fixture: key=%d capture=%d start=%llu now=%llu\n", menu::g_hotkey, menu::g_capturando,
        (unsigned long long)menu::g_capturaInicio, (unsigned long long)previewTime);
    Check(menu::g_hotkey == VK_F6 && !menu::g_capturando, "hotkey capture saves chosen key");
    // Clear (X) must restore Insert, never leave the menu without a reopen key
    Click(581, 535);
    Check(menu::g_hotkey == VK_INSERT, "clear button restores Insert instead of leaving no key");
    io.AddMousePosEvent(239,485); Frame(900,600); Frame(900,600);
    Capture(dev, prefix + L"_tooltip.png", 900,600);
    Click(582,116);
    Check(!menu::g_visible, "close button works");
    menu::g_visible = false; menu::g_render = false;
    Frame(900, 600);
    Check(ImGui::GetDrawData()->TotalVtxCount == 0, "closed panel has no UI geometry");
    // --- Crosshair defaults ---
    Check(!menu::g_crosshairActivado, "crosshair defaults off");
    Check(menu::g_crosshairTipo == 0, "crosshair type defaults to plus");
    Check(menu::g_crosshairTamano == 8.0f, "crosshair size defaults to 8");
    Check(menu::g_crosshairColor.x > 0.9f && menu::g_crosshairColor.y < 0.2f,
          "crosshair color defaults to red");
    menu::g_crosshairActivado = true;
    Frame(900, 600);
    {
        auto* dd2 = ImGui::GetDrawData();
        Check(dd2->TotalVtxCount > 0 && dd2->TotalVtxCount < 256,
              "crosshair on with closed panel draws only the cross");
    }
    menu::g_crosshairTipo = 1;
    Frame(900, 600);
    Check(ImGui::GetDrawData()->TotalVtxCount > 0, "crosshair type X draws when closed");
    menu::g_crosshairTipo = 0;
    menu::g_crosshairTamano = 12.0f;
    menu::g_crosshairColor = ImVec4(0.2f, 0.8f, 1.0f, 1.0f);
    menu::GuardarConfig();
    menu::g_crosshairTamano = 8.0f;
    menu::g_crosshairColor = ImVec4(0.95f, 0.16f, 0.14f, 1.0f);
    menu::g_crosshairTipo = 0;
    menu::CargarConfig();
    Check(menu::g_crosshairTamano == 12.0f, "crosshair size persists through save/reload");
    Check(menu::g_crosshairTipo == 0, "crosshair type persists as 0");
    Check(fabsf(menu::g_crosshairColor.x - 0.2f) < .001f && fabsf(menu::g_crosshairColor.y - 0.8f) < .001f,
          "crosshair color persists through save/reload");
    Check(menu::g_crosshairActivado, "crosshair persists through save/reload");
    // Legacy config without crosshair keys -> defaults
    {
        FILE* f = fopen(menu::RutaConfig().c_str(), "w");
        Check(f != nullptr, "write legacy config");
        fprintf(f, "vmOffx = 1\nvmOffy = 2\nvmOffz = 3\nhotkey = 45\n");
        fclose(f);
        menu::g_crosshairActivado = true;
        menu::g_crosshairTipo = 1;
        menu::g_crosshairTamano = 20.0f;
        menu::g_crosshairColor = ImVec4(0,1,0,1);
        menu::CargarConfig();
        Check(!menu::g_crosshairActivado, "legacy config without keys leaves crosshair off");
        Check(menu::g_crosshairTipo == 0, "legacy config resets type to plus");
        Check(menu::g_crosshairTamano == 8.0f, "legacy config resets size to 8");
        Check(menu::g_crosshairColor.y < 0.2f, "legacy config resets color to red");
        Check(vmh::g_offx == 1 && vmh::g_offy == 2 && vmh::g_offz == 3, "legacy config still loads offsets");
    }
    // Panel open: layout + checkbox + type buttons
    menu::g_visible = menu::g_render = true; menu::g_menuJuego = true;
    menu::g_animInicio = 0; previewTime = 1000;
    menu::g_panelPos = ImVec2(200,80);
    for (int i = 0; i < 3; ++i) Frame(900,600);
    {
        auto* panel2 = ImGui::FindWindowByName("VM");
        Check(panel2 && panel2->Size.y <= 510.01f && panel2->ScrollMax.y == 0,
              "normal panel fits with crosshair option row and credit");
    }
    Click(232, 394);
    Check(menu::g_crosshairActivado, "crosshair checkbox toggles on");
    Check(menu::g_dirty, "crosshair change marks dirty");
    Frame(900,600);
    Check(ImGui::GetDrawData()->TotalVtxCount > 0, "crosshair visible with panel open");
    // Type buttons at Y=337 local -> screen y = 80+337+13 = 430
    Click(300, 430);
    Check(menu::g_crosshairTipo == 1, "type X button selects X");
    Frame(900,600);
    Check(ImGui::GetDrawData()->TotalVtxCount > 0, "type X still draws");
    Click(260, 430);
    Check(menu::g_crosshairTipo == 0, "type + button selects plus");
    // Size stays in range after bump
    const float tam0 = menu::g_crosshairTamano;
    if (menu::g_crosshairTamano < 40.0f) menu::g_crosshairTamano += 1.0f;
    Check(menu::g_crosshairTamano >= 2.0f && menu::g_crosshairTamano <= 40.0f && menu::g_crosshairTamano > tam0,
          "size increases within range");
    // Reset XYZ does not disable crosshair
    Click(283,485);
    Check(menu::g_crosshairActivado, "reset XYZ does not disable crosshair");
    Check(vmh::g_offx == 0 && vmh::g_offy == 0 && vmh::g_offz == 0, "reset still zeros offsets");
    Click(582,116);
    Check(!menu::g_visible && menu::g_crosshairActivado, "closing panel keeps crosshair state");
    menu::g_visible = false; menu::g_render = false;
    Frame(900,600);
    Check(ImGui::GetDrawData()->TotalVtxCount > 0, "crosshair still draws after panel closed");
    Frame(901, 601);
    Check(ImGui::GetDrawData()->TotalVtxCount > 0, "crosshair draws on odd dimensions");
    menu::g_crosshairActivado = false;
    Frame(900,600);
    Check(ImGui::GetDrawData()->TotalVtxCount == 0, "crosshair off returns to zero geometry");
    // --- Panel smooth open/close (smoothstep + slide ~200 ms) ---
    menu::g_crosshairActivado = false;
    menu::g_visible = false; menu::g_render = false;
    menu::g_animInicio = previewTime;
    menu::g_visible = true; menu::g_render = true;
    Frame(900,600);
    Check(menu::g_render, "panel keeps rendering during open animation");
    previewTime += 100;
    Frame(900,600);
    Check(menu::g_render && menu::g_visible, "panel still mid open animation at 100 ms");
    previewTime += 150;
    Frame(900,600);
    Check(menu::g_render && menu::g_visible, "panel fully open after 200 ms");
    Check(ImGui::GetDrawData()->TotalVtxCount > 0, "open panel draws geometry");
    menu::Cerrar();
    Check(!menu::g_visible && menu::g_render, "close starts fade-out with render still on");
    previewTime += 100;
    Frame(900,600);
    Check(menu::g_render, "panel still mid close animation at 100 ms");
    previewTime += 150;
    Frame(900,600);
    Check(!menu::g_render, "panel stops rendering after close animation");
    Check(ImGui::GetDrawData()->TotalVtxCount == 0, "closed panel has zero geometry after smooth close");
    puts("PASS: layout, sliders, save/reload, hotkey, crosshair type/size/color, panel smooth open/close, @tokyossz");
    ImGui_ImplDX9_Shutdown(); ImGui::DestroyContext();
    dev->Release(); d3d->Release(); DestroyWindow(window); Gdiplus::GdiplusShutdown(token);
}
