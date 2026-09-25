#include "dx9_hook.h"
#include "menu.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <d3d9.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    typedef HRESULT(WINAPI* Present_t)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
    typedef HRESULT(WINAPI* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
    Present_t g_origPresent = nullptr;
    Reset_t   g_origReset = nullptr;
    HWND      g_hwnd = nullptr;
    WNDPROC   g_origWndProc = nullptr;
    bool      g_imguiInit = false;
    volatile bool g_setupListo = false;
    volatile bool g_cerrando = false;
    DWORD g_parchadas[64][2];
    int   g_numParchadas = 0;
    static HWND g_hooked[32];
    static WNDPROC g_hookedOrig[32];
    static int g_hookedCount = 0;

    void Log(const char* s) {
        char ruta[MAX_PATH];
        DWORD n = GetTempPathA(MAX_PATH, ruta);
        FILE* f = (n && n < MAX_PATH) ? (strcat_s(ruta, "vm_standalone_log.txt"), fopen(ruta, "a")) : nullptr;
        if (f) { fprintf(f, "%s\n", s); fclose(f); }
    }

    static bool RegionLegible(const void* p, size_t bytes) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!p || !bytes) return false;
        if (VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
        if (mbi.State != MEM_COMMIT) return false;
        if (mbi.Protect & PAGE_GUARD) return false;
        DWORD prot = mbi.Protect & 0xFF;
        bool legible = (prot == PAGE_READONLY || prot == PAGE_READWRITE || prot == PAGE_WRITECOPY ||
                        prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_READWRITE ||
                        prot == PAGE_EXECUTE_WRITECOPY);
        if (!legible) return false;
        const BYTE* base = (const BYTE*)mbi.BaseAddress;
        const BYTE* ini = (const BYTE*)p;
        return ini >= base && (ini + bytes) <= (base + mbi.RegionSize);
    }

    static WNDPROC OrigForHwnd(HWND h){
        for(int i=0;i<g_hookedCount;i++) if(g_hooked[i]==h) return g_hookedOrig[i];
        return g_origWndProc ? g_origWndProc : (WNDPROC)DefWindowProcW;
    }
    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (g_cerrando) {
            WNDPROC o = OrigForHwnd(hWnd);
            return CallWindowProc(o, hWnd, msg, wParam, lParam);
        }
        if (msg == WM_KILLFOCUS || (msg == WM_ACTIVATEAPP && wParam == FALSE))
            menu::PerderFoco();

        if (menu::estaVisible()) {
            if (msg == WM_INPUT) {
                if (!menu::estaEnMenuJuego()) {
                    UINT size = 0;
                    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == 0 && size) {
                        std::vector<BYTE> data(size);
                        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, data.data(), &size, sizeof(RAWINPUTHEADER)) == size) {
                            RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(data.data());
                            if (raw->header.dwType == RIM_TYPEMOUSE) {
                                dx9h::g_mouseX += static_cast<float>(raw->data.mouse.lLastX);
                                dx9h::g_mouseY += static_cast<float>(raw->data.mouse.lLastY);
                            }
                        }
                    }
                }
                return 0;
            }
            if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
                menu::Cerrar();
                WNDPROC o = OrigForHwnd(hWnd);
                return CallWindowProc(o, hWnd, msg, wParam, lParam);
            }
            if (g_imguiInit) {
                if (menu::estaEnMenuJuego()) ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
                else if (msg != WM_MOUSEMOVE) ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            }
            switch (msg) {
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDOWN: case WM_XBUTTONUP:
            case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
            case WM_MOUSEACTIVATE:
            case WM_SETCURSOR:
            case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
            case WM_CHAR:
                return 0;
            }
        }
        {
            WNDPROC o = OrigForHwnd(hWnd);
            return CallWindowProc(o, hWnd, msg, wParam, lParam);
        }
    }

    static bool g_freezeActive=false;
    static float g_savedPitch=0, g_savedYaw=0;
    HRESULT WINAPI Present(IDirect3DDevice9* dev, const RECT* a, const RECT* b, HWND c, const RGNDATA* d) {
        if (g_setupListo && !g_cerrando) {
            if (!g_imguiInit) {
                ImGui_ImplDX9_Init(dev);
                g_imguiInit = true;
                Log("[dx9] ImGui DX9 init OK");
            }
            if (menu::estaVisible()) {
                ImGui::GetIO().MouseDrawCursor = !menu::estaEnMenuJuego();
            } else {
                ImGui::GetIO().MouseDrawCursor = false;
            }
            // FIX cámara engine: congelar viewangles en memoria cuando menú visible (respaldo si WM_INPUT se escapa)
            // engine.dll -> ptr -> +offset pitch/yaw. Offsets NO incluidos en este repo (a 0 = se omite el freeze).
            const DWORD OFF_ENGINE_PTR = 0x00000000;
            const DWORD OFF_ANGULOS = 0x00000000;
            if (menu::estaVisible() && !menu::estaEnMenuJuego() && OFF_ENGINE_PTR && OFF_ANGULOS) {
                HMODULE eng = GetModuleHandleA("engine.dll");
                if (eng) {
                    __try {
                        DWORD base = (DWORD)eng;
                        DWORD ptr = 0;
                        if (RegionLegible((const void*)(base + OFF_ENGINE_PTR), sizeof(DWORD)))
                            ptr = *(DWORD*)(base + OFF_ENGINE_PTR);
                        float* ang = (ptr && RegionLegible((const void*)(ptr + OFF_ANGULOS), 2 * sizeof(float)))
                            ? (float*)(ptr + OFF_ANGULOS) : nullptr;
                        if (ang) {
                            float pitch = ang[0], yaw = ang[1];
                            bool sane = (pitch >= -90.0f && pitch <= 90.0f) &&
                                        (yaw > -1000000.0f && yaw < 1000000.0f);
                            if (sane) {
                                if (!g_freezeActive) { g_savedPitch = pitch; g_savedYaw = yaw; g_freezeActive = true; }
                                ang[0] = g_savedPitch;
                                ang[1] = g_savedYaw;
                            } else {
                                g_freezeActive = false;
                            }
                        } else {
                            g_freezeActive = false;
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) { g_freezeActive = false; }
                }
            } else {
                g_freezeActive = false;
            }
            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            if (menu::estaVisible() && !menu::estaEnMenuJuego()) {
                ImVec2 display = ImGui::GetIO().DisplaySize;
                if (dx9h::g_mouseX < 0.0f) {
                    dx9h::g_mouseX = display.x * 0.5f;
                    dx9h::g_mouseY = display.y * 0.5f;
                }
                if (dx9h::g_mouseY < 0.0f) dx9h::g_mouseY = 0.0f;
                if (dx9h::g_mouseX > display.x) dx9h::g_mouseX = display.x;
                if (dx9h::g_mouseY > display.y) dx9h::g_mouseY = display.y;
                ImGui::GetIO().AddMousePosEvent(dx9h::g_mouseX, dx9h::g_mouseY);
            }
            ImGui::NewFrame();
            menu::Dibujar();
            ImGui::EndFrame();
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        }
        return g_origPresent(dev, a, b, c, d);
    }

    HRESULT WINAPI Reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
        if (g_imguiInit) ImGui_ImplDX9_InvalidateDeviceObjects();
        HRESULT hr = g_origReset(dev, pp);
        if (SUCCEEDED(hr) && g_imguiInit) ImGui_ImplDX9_CreateDeviceObjects();
        return hr;
    }
}

namespace dx9h {
    float g_mouseX = -1.0f;
    float g_mouseY = -1.0f;

    void ParchearSlot(DWORD* slot, DWORD nuevo) {
        DWORD old;
        VirtualProtect(slot, sizeof(DWORD), PAGE_READWRITE, &old);
        DWORD original = *slot;
        *slot = nuevo;
        VirtualProtect(slot, sizeof(DWORD), old, &old);
        if (g_numParchadas < 64) {
            g_parchadas[g_numParchadas][0] = (DWORD)slot;
            g_parchadas[g_numParchadas][1] = original;
            g_numParchadas++;
        }
    }

    static BOOL CALLBACK EnumChildHook(HWND h, LPARAM lp){
        if(g_hookedCount>=32) return FALSE;
        // Hookear hijos también (raw input a veces va a child)
        for(int i=0;i<g_hookedCount;i++) if(g_hooked[i]==h) return TRUE;
        WNDPROC orig = (WNDPROC)SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)WndProc);
        if(orig){
            g_hooked[g_hookedCount]=h;
            g_hookedOrig[g_hookedCount]=orig;
            g_hookedCount++;
            char l2[128]; sprintf_s(l2, "[dx9] WndProc child hookeado hwnd=0x%p", h); Log(l2);
        }
        return TRUE;
    }
    static BOOL CALLBACK EnumHookWindows(HWND h, LPARAM lp){
        if(g_hookedCount>=32) return FALSE;
        DWORD pid=0; GetWindowThreadProcessId(h,&pid);
        if(pid!=GetCurrentProcessId()) return TRUE;
        if(h==g_hwnd) return TRUE; // ya hookeada principal, evitar doble hook
        char title[256]={0}; GetWindowTextA(h,title,sizeof(title));
        char cls[128]={0}; GetClassNameA(h,cls,sizeof(cls));
        char log[512]; sprintf_s(log, "[dx9] candidato hwnd=0x%p title='%s' class='%s' vis=%d", h, title, cls, IsWindowVisible(h)); Log(log);
        WNDPROC orig = (WNDPROC)SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)WndProc);
        if(orig){
            g_hooked[g_hookedCount]=h;
            g_hookedOrig[g_hookedCount]=orig;
            g_hookedCount++;
            char l2[128]; sprintf_s(l2, "[dx9] WndProc hookeado hwnd=0x%p", h); Log(l2);
        }
        EnumChildWindows(h, EnumChildHook, 0);
        return TRUE;
    }
    bool Instalar(HWND hwnd) {
        g_hwnd = hwnd;
        if (!g_hwnd) { Log("[dx9] sin hwnd"); return false; }
        HMODULE d3d9mod = GetModuleHandleA("d3d9.dll");
        if (!d3d9mod) { Log("[dx9] no d3d9.dll"); return false; }
        // Hookear ventana principal + todas las visibles del proceso (fix cámara)
        g_origWndProc = (WNDPROC)SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
        g_hooked[0]=g_hwnd; g_hookedOrig[0]=g_origWndProc; g_hookedCount=1;
        char lg[128]; sprintf_s(lg, "[dx9] WndProc principal hwnd=0x%p orig=0x%p", g_hwnd, g_origWndProc); Log(lg);
        EnumWindows(EnumHookWindows, 0);
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        ImGui_ImplWin32_Init(g_hwnd);
        menu::Inicializar(g_hwnd);
        Log("[dx9] ImGui base iniciado");

        typedef IDirect3D9* (WINAPI* Create9_t)(UINT);
        Create9_t Create9 = (Create9_t)GetProcAddress(d3d9mod, "Direct3DCreate9");
        if (!Create9) { Log("[dx9] no Direct3DCreate9"); return false; }
        IDirect3D9* d3d = Create9(D3D_SDK_VERSION);
        if (!d3d) { Log("[dx9] Direct3DCreate9 fallo"); return false; }
        const char* CLASE_DUMMY = "vm_standalone_dummy_dx9";
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = CLASE_DUMMY;
        RegisterClassExA(&wc);
        HWND hDummyWin = CreateWindowExA(0, CLASE_DUMMY, "", WS_OVERLAPPEDWINDOW, 0, 0, 8, 8, nullptr, nullptr, wc.hInstance, nullptr);
        IDirect3DDevice9* dummy = nullptr;
        HRESULT hr = E_FAIL;
        char bufE[128];
        const DWORD MODO_VP[3] = { D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DCREATE_SOFTWARE_VERTEXPROCESSING, D3DCREATE_MIXED_VERTEXPROCESSING };
        for (int tipoDev = 0; tipoDev < 2 && !dummy; tipoDev++) {
            for (int vp = 0; vp < 3 && !dummy; vp++) {
                D3DPRESENT_PARAMETERS pp = {};
                pp.Windowed = TRUE;
                pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
                pp.BackBufferFormat = D3DFMT_UNKNOWN;
                pp.BackBufferWidth = 8;
                pp.BackBufferHeight = 8;
                pp.hDeviceWindow = hDummyWin;
                hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, tipoDev == 0 ? D3DDEVTYPE_HAL : D3DDEVTYPE_REF, hDummyWin, MODO_VP[vp], &pp, &dummy);
                if (FAILED(hr)) {
                    sprintf_s(bufE, "[dx9] dummy %s VP%d fallo hr=0x%08X", tipoDev == 0 ? "HAL" : "REF", vp, (unsigned)hr);
                    Log(bufE);
                    dummy = nullptr;
                }
            }
        }
        if (FAILED(hr) || !dummy) {
            d3d->Release();
            if (hDummyWin) DestroyWindow(hDummyWin);
            Log("[dx9] CreateDevice fallo");
            return false;
        }
        DWORD* vtableDummy = *(DWORD**)dummy;
        g_origPresent = (Present_t)vtableDummy[17];
        g_origReset = (Reset_t)vtableDummy[16];
        DWORD origEndScene = vtableDummy[42];
        dummy->Release();
        d3d->Release();
        if (hDummyWin) DestroyWindow(hDummyWin);
        if (!g_origPresent || !g_origReset) { Log("[dx9] Present/Reset nulos"); return false; }
        Log("[dx9] Present/Reset obtenidos");
        int parcheadas = 0;
        MEMORY_BASIC_INFORMATION mbi;
        for (BYTE* addr = nullptr; VirtualQuery(addr, &mbi, sizeof(mbi)); addr = (BYTE*)mbi.BaseAddress + mbi.RegionSize) {
            if (mbi.State != MEM_COMMIT) continue;
            if (mbi.Protect == PAGE_NOACCESS || (mbi.Protect & PAGE_GUARD)) continue;
            if (mbi.Protect & (PAGE_NOCACHE | PAGE_WRITECOMBINE)) continue;
            if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY))) continue;
            if (mbi.Type != MEM_IMAGE && mbi.RegionSize > 0x1000000) continue;
            for (DWORD p = (DWORD)mbi.BaseAddress; p + 4 * 43 <= (DWORD)mbi.BaseAddress + mbi.RegionSize; p += 4) {
                DWORD* table = (DWORD*)p;
                if (table[16] == (DWORD)g_origReset && table[17] == (DWORD)g_origPresent && table[42] == origEndScene) {
                    ParchearSlot(&table[17], (DWORD)&Present);
                    ParchearSlot(&table[16], (DWORD)&Reset);
                    parcheadas++;
                }
            }
        }
        char buf[128];
        sprintf_s(buf, "[dx9] vtables verificadas: %d", parcheadas);
        Log(buf);
        g_setupListo = true;
        return true;
    }

    void Desinstalar() {
        g_setupListo = false;
        g_cerrando = true;
        Sleep(80);
        menu::Apagar();
        for (int i = 0; i < g_numParchadas; i++) {
            DWORD old;
            VirtualProtect((void*)g_parchadas[i][0], sizeof(DWORD), PAGE_READWRITE, &old);
            *(DWORD*)g_parchadas[i][0] = g_parchadas[i][1];
            VirtualProtect((void*)g_parchadas[i][0], sizeof(DWORD), old, &old);
        }
        g_numParchadas = 0;
        for(int i=0;i<g_hookedCount;i++){
            if(g_hooked[i] && g_hookedOrig[i]) SetWindowLongPtrW(g_hooked[i], GWLP_WNDPROC, (LONG_PTR)g_hookedOrig[i]);
        }
        g_hookedCount=0;
        if (g_hwnd && g_origWndProc) SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
        if (g_imguiInit) {
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            g_imguiInit = false;
        }
    }
}
