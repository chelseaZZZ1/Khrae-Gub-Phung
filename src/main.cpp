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

// ---------------------------------------------------------
// Engine Logic & Config Enums
// ---------------------------------------------------------
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
    const char* subtitle;
    float forceMultiplier;      // Force Multiplier
    float moveSpeedMultiplier;   // Movement Speed
    ImVec4 color;                // Color Accent
};

// Mode Configurations (English Only)
BoostSettings g_BoostConfigs[] = {
    { "DISABLED",       "Default Engine Profile",          1.0f,  1.0f, ImVec4(0.45f, 0.48f, 0.55f, 1.0f) },
    { "LOW BOOST",      "Stealth Mode / Safe Stream",      1.25f, 1.10f, ImVec4(0.20f, 0.80f, 0.40f, 1.0f) },
    { "ROLEPLAY REAL",  "Balanced Physics & Speed",       1.50f, 1.15f, ImVec4(0.15f, 0.65f, 1.00f, 1.0f) },
    { "MEDIUM POWER",   "High Impact Multiplier",         2.00f, 1.30f, ImVec4(1.00f, 0.75f, 0.00f, 1.0f) },
    { "HIGH BEAST",     "Aggressive Engine Overdrive",    3.50f, 1.60f, ImVec4(1.00f, 0.35f, 0.00f, 1.0f) },
    { "FULL GOD MODE",  "Maximum Power / Unrestricted",   10.0f, 2.20f, ImVec4(0.95f, 0.15f, 0.20f, 1.0f) },
    { "SYSTEM RESET",   "Restore Native Defaults",        1.0f,  1.0f, ImVec4(0.00f, 0.85f, 1.00f, 1.0f) }
};

// Global State Variables
BoostMode g_CurrentMode = MODE_OFF;
bool g_IsActive = false;
uint32_t POOL_CUE_HASH = 0x94F28797;
float g_CustomForce = 1.0f;
float g_CustomSpeed = 1.0f;

void ApplyPoolCueBoost(BoostMode mode) {
    if (mode == MODE_SYSTEM_RESET) {
        g_CurrentMode = MODE_OFF;
        g_IsActive = false;
        g_CustomForce = 1.0f;
        g_CustomSpeed = 1.0f;
        return;
    }

    g_CurrentMode = mode;
    g_IsActive = (mode != MODE_OFF);
    if (g_IsActive) {
        g_CustomForce = g_BoostConfigs[mode].forceMultiplier;
        g_CustomSpeed = g_BoostConfigs[mode].moveSpeedMultiplier;
    }
}

// ---------------------------------------------------------
// Ultra Modern Custom Styling (Dark Cyber Gold / Sleek UI)
// ---------------------------------------------------------
void SetupModernStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Smooth Curved Geometries
    style.WindowRounding    = 14.0f;
    style.ChildRounding     = 10.0f;
    style.FrameRounding     = 8.0f;
    style.PopupRounding     = 10.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 8.0f;

    // Comfort Padding & Spacing
    style.WindowPadding     = ImVec2(18, 18);
    style.FramePadding      = ImVec2(12, 8);
    style.ItemSpacing       = ImVec2(10, 10);
    style.ItemInnerSpacing  = ImVec2(8, 8);
    style.ScrollbarSize     = 10.0f;
    style.WindowBorderSize  = 1.0f;

    // Dark Cyber Gold Palette
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]           = ImVec4(0.06f, 0.06f, 0.08f, 0.98f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.09f, 0.09f, 0.12f, 0.80f);
    colors[ImGuiCol_PopupBg]            = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
    colors[ImGuiCol_Border]             = ImVec4(0.20f, 0.22f, 0.28f, 0.50f);
    colors[ImGuiCol_BorderShadow]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.12f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.18f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.22f, 0.25f, 0.32f, 1.00f);
    colors[ImGuiCol_TitleBg]            = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.08f, 0.08f, 0.11f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]   = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg]          = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);

    // Accent Colors - Gold / Amber / Cyan
    colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.06f, 0.06f, 0.08f, 0.50f);
    colors[ImGuiCol_ScrollbarGrab]      = ImVec4(0.22f, 0.24f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.38f, 0.48f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.85f, 0.68f, 0.15f, 1.00f);

    colors[ImGuiCol_CheckMark]          = ImVec4(0.95f, 0.75f, 0.18f, 1.00f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.85f, 0.68f, 0.15f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]   = ImVec4(1.00f, 0.82f, 0.25f, 1.00f);

    colors[ImGuiCol_Button]             = ImVec4(0.14f, 0.15f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.22f, 0.24f, 0.32f, 1.00f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.85f, 0.68f, 0.15f, 1.00f);

    colors[ImGuiCol_Header]             = ImGui::ColorConvertU32ToFloat4(0x40302518);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.25f, 0.27f, 0.36f, 1.00f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.85f, 0.68f, 0.15f, 1.00f);

    colors[ImGuiCol_Separator]          = ImVec4(0.20f, 0.22f, 0.28f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]   = ImVec4(0.85f, 0.68f, 0.15f, 0.80f);
    colors[ImGuiCol_SeparatorActive]    = ImVec4(0.95f, 0.75f, 0.18f, 1.00f);

    colors[ImGuiCol_Text]               = ImVec4(0.92f, 0.93f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]       = ImVec4(0.45f, 0.48f, 0.55f, 1.00f);
}

// ---------------------------------------------------------
// Custom UI Renderer
// ---------------------------------------------------------
void RenderPoolCueBoosterGUI() {
    ImGui::SetNextWindowSize(ImVec2(480.0f, 540.0f), ImGuiCond_FirstUseEver);

    ImGui::Begin("POOL CUE ULTRA BOOSTER v2.0", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    // Header Title Area
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.82f, 0.20f, 1.0f));
    ImGui::TextUnformatted("FIVEM POOL CUE POWER MODIFICATION");
    ImGui::PopStyleColor();

    ImGui::TextColored(ImVec4(0.50f, 0.53f, 0.60f, 1.0f), "Engine Core Memory & Native Controller");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Status Panel Child Window
    ImGui::BeginChild("StatusPanel", ImVec2(0, 65), true);
    {
        ImGui::Text("ENGINE STATUS:");
        ImGui::SameLine();
        if (g_IsActive) {
            ImGui::TextColored(g_BoostConfigs[g_CurrentMode].color, "[ ACTIVE - %s ]", g_BoostConfigs[g_CurrentMode].name);
            ImGui::TextColored(ImVec4(0.70f, 0.73f, 0.80f, 1.0f), "Profile Note: %s", g_BoostConfigs[g_CurrentMode].subtitle);
        } else {
            ImGui::TextColored(ImVec4(0.45f, 0.48f, 0.55f, 1.0f), "[ STANDBY / INACTIVE ]");
            ImGui::TextColored(ImVec4(0.40f, 0.43f, 0.50f, 1.0f), "Select an operational mode below to initialize.");
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.85f, 0.68f, 0.15f, 1.00f), "SELECT BOOST PROFILE:");

    // Mode Buttons Grid
    for (int i = 1; i <= 5; ++i) {
        ImGui::PushID(i);
        bool isSelected = (g_CurrentMode == i);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, g_BoostConfigs[i].color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_BoostConfigs[i].color);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.13f, 0.17f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.22f, 0.28f, 1.00f));
        }

        if (ImGui::Button(g_BoostConfigs[i].name, ImVec2(-1, 38))) {
            ApplyPoolCueBoost((BoostMode)i);
        }

        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Live Fine-Tuning Multipliers
    ImGui::TextColored(ImVec4(0.85f, 0.68f, 0.15f, 1.00f), "LIVE ADJUSTMENT TUNING:");
    ImGui::SliderFloat("Force Multiplier", &g_CustomForce, 1.0f, 10.0f, "%.2fx Force");
    ImGui::SliderFloat("Speed Multiplier", &g_CustomSpeed, 1.0f, 3.0f, "%.2fx Speed");

    ImGui::Spacing();
    ImGui::Spacing();

    // System Reset Action Button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.12f, 0.15f, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.18f, 0.22f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.00f, 0.25f, 0.30f, 1.00f));
    if (ImGui::Button("SYSTEM RESET & FLUSH MEMORY", ImVec2(-1, 42))) {
        ApplyPoolCueBoost(MODE_SYSTEM_RESET);
    }
    ImGui::PopStyleColor(3);

    ImGui::End();
}

// ---------------------------------------------------------
// Smooth Font Setup Function
// ---------------------------------------------------------
void SetupSmoothFonts(ImGuiIO& io) {
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 4; // High Horizontal Anti-Aliasing
    font_cfg.OversampleV = 4; // High Vertical Anti-Aliasing
    font_cfg.PixelSnapH = false;

    // Attempt to load clean Windows system font (Segoe UI / Tahoma)
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f, &font_cfg);
    if (font == nullptr) {
        font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\tahoma.ttf", 17.0f, &font_cfg);
    }
    if (font == nullptr) {
        io.Fonts->AddFontDefault(&font_cfg); // Fallback
    }
}

// ---------------------------------------------------------
// Main Entry Point (Win32 + DirectX 11)
// ---------------------------------------------------------
int main(int, char**) {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"PoolCueBoosterClass", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"POOL CUE ULTRA BOOSTER v2.0", WS_OVERLAPPEDWINDOW, 100, 100, 520, 580, nullptr, nullptr, wc.hInstance, nullptr);

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

    SetupSmoothFonts(io);
    SetupModernStyle();

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
        const float clear_color_with_alpha[4] = { 0.04f, 0.04f, 0.05f, 1.00f };
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

// ---------------------------------------------------------
// Direct3D 11 Helper Functions
// ---------------------------------------------------------
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
