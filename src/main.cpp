#include <iostream>
#include <windows.h>
#include <d3d11.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// Forward Declarations & Win32 Event Handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// สมมติฐานโครงสร้าง FiveM Memory & Native Calls
enum BoostMode {
    MODE_OFF = 0,
    MODE_LOW,
    MODE_ROLEPLAY,
    MODE_MEDIUM,
    MODE_HIGH,
    MODE_FULL,
    MODE_SYSTEM_RESET
};

struct BoostSettings {
    const char* name;
    float forceMultiplier;      // แรงตีไม้พลู
    float moveSpeedMultiplier;   // ความเร็วการเดิน/วิ่ง
    ImVec4 color;                // สี UI แสดงผล
};

// ค่า Setting ของแต่ละโหมด
BoostSettings g_BoostConfigs[] = {
    { "DISABLED",        1.0f,  1.0f, ImVec4(0.5f, 0.5f, 0.5f, 1.0f) }, // OFF
    { "LOW BOOST",       1.25f, 1.10f, ImVec4(0.2f, 0.8f, 0.2f, 1.0f) }, // LOW
    { "ROLEPLAY REAL",   1.50f, 1.15f, ImVec4(0.2f, 0.6f, 1.0f, 1.0f) }, // ROLEPLAY
    { "MEDIUM POWER",    2.00f, 1.30f, ImVec4(1.0f, 0.8f, 0.0f, 1.0f) }, // MEDIUM
    { "HIGH BEAST",      3.50f, 1.60f, ImVec4(1.0f, 0.4f, 0.0f, 1.0f) }, // HIGH
    { "FULL GOD MODE",   10.0f, 2.20f, ImVec4(0.9f, 0.1f, 0.1f, 1.0f) }, // FULL
    { "SYSTEM RESET",    1.0f,  1.0f, ImVec4(0.0f, 1.0f, 1.0f, 1.0f) }  // RESET
};

// Global Variables
BoostMode g_CurrentMode = MODE_OFF;
bool g_IsActive = false;
uint32_t POOL_CUE_HASH = 0x94F28797; // Hash Code ของ Weapon PoolCue ใน GTA V / FiveM

// ฟังก์ชันจำลองการ Apply ค่าเข้าเกม (FiveM Native Memory Writer)
void ApplyPoolCueBoost(BoostMode mode) {
    if (mode == MODE_SYSTEM_RESET) {
        g_CurrentMode = MODE_OFF;
        g_IsActive = false;
        // Reset Native Engine Multipliers
        // PLAYER::SET_RUN_SPRINT_MULTIPLIER_FOR_PLAYER(PlayerId(), 1.0f);
        // MemoryWrite(PoolCueDamageAddress, OriginalDamage);
        return;
    }

    g_CurrentMode = mode;
    g_IsActive = (mode != MODE_OFF);

    float force = g_BoostConfigs[mode].forceMultiplier;
    float speed = g_BoostConfigs[mode].moveSpeedMultiplier;

    // --- Core Engine Modification Logic ---
    // Check if player is holding PoolCue:
    // if (GetCurrentPedWeapon(PlayerPedId()) == POOL_CUE_HASH) {
    //     PLAYER::SET_RUN_SPRINT_MULTIPLIER_FOR_PLAYER(PlayerId(), speed);
    //     WriteMemory(WeaponDamageOffset, force);
    // }
}

// GUI Rendering Logic (Dear ImGui)
void RenderPoolCueBoosterGUI() {
    ImGui::SetNextWindowSize(ImVec2(420.0f, 380.0f), ImGuiCond_FirstUseEver);
    
    // Styling Modern Dark Gold
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.FrameRounding = 6.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 0.95f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.15f, 0.16f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.27f, 0.36f, 1.00f);

    ImGui::Begin("⚡ POOL CUE ULTRA BOOSTER v2.0", nullptr, ImGuiWindowFlags_NoResize);

    // Header Title
    ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "FIVEM POOL CUE POWER MOD");
    ImGui::Separator();
    ImGui::Spacing();

    // Current Status Display
    ImGui::Text("STATUS: ");
    ImGui::SameLine();
    if (g_IsActive) {
        ImGui::TextColored(g_BoostConfigs[g_CurrentMode].color, "[ ACTIVE - %s ]", g_BoostConfigs[g_CurrentMode].name);
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ STANDBY / OFF ]");
    }

    ImGui::Spacing();
    ImGui::Text("SELECT BOOST MODE:");

    // Mode 1: Low
    if (ImGui::Button("🟢 LOW MODE (เนียนสตรีม)", ImVec2(-1, 35))) {
        ApplyPoolCueBoost(MODE_LOW);
    }

    // Mode 2: RolePlay
    if (ImGui::Button("🔵 ROLEPLAY MODE (เน้นสมจริง+เพิ่มแรง)", ImVec2(-1, 35))) {
        ApplyPoolCueBoost(MODE_ROLEPLAY);
    }

    // Mode 3: Medium
    if (ImGui::Button("🟡 MEDIUM MODE (หวดลอยตึงๆ)", ImVec2(-1, 35))) {
        ApplyPoolCueBoost(MODE_MEDIUM);
    }

    // Mode 4: High
    if (ImGui::Button("🟠 HIGH MODE (สายเดือด / พลังแรงสูง)", ImVec2(-1, 35))) {
        ApplyPoolCueBoost(MODE_HIGH);
    }

    // Mode 5: Full
    if (ImGui::Button("🔴 FULL MODE (มหาโหด / GOD POWER)", ImVec2(-1, 35))) {
        ApplyPoolCueBoost(MODE_FULL);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // System Reset Button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("🔄 SYSTEM RESET (ล้างค่าคืนระบบเดิม)", ImVec2(-1, 40))) {
        ApplyPoolCueBoost(MODE_SYSTEM_RESET);
    }
    ImGui::PopStyleColor(2);

    ImGui::End();
}

// Main Entry Point
int main(int, char**) {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"PoolCueBoosterClass", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"⚡ POOL CUE ULTRA BOOSTER v2.0", WS_OVERLAPPEDWINDOW, 100, 100, 480, 440, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderPoolCueBoosterGUI();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.1f, 0.1f, 0.1f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper Functions สำหรับ Direct3D 11
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
