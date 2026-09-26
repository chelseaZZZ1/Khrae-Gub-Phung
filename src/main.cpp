#include <iostream>
#include <windows.h>
#include <d3d11.h>
#include <cmath>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

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
    float forceMultiplier;
    float moveSpeedMultiplier;
    ImVec4 color;
};

BoostSettings g_BoostConfigs[] = {
    { "DISABLED",       "Default Engine Profile",          1.0f,  1.0f, ImVec4(0.45f, 0.48f, 0.55f, 1.0f) },
    { "LOW BOOST",      "Stealth Mode / Safe Stream",      1.25f, 1.10f, ImVec4(0.20f, 0.80f, 0.40f, 1.0f) },
    { "ROLEPLAY REAL",  "Balanced Physics & Speed",       1.50f, 1.15f, ImVec4(0.15f, 0.65f, 1.00f, 1.0f) },
    { "MEDIUM POWER",   "High Impact Multiplier",         2.00f, 1.30f, ImVec4(1.00f, 0.75f, 0.00f, 1.0f) },
    { "HIGH BEAST",     "Aggressive Engine Overdrive",    3.50f, 1.60f, ImVec4(1.00f, 0.35f, 0.00f, 1.0f) },
    { "FULL GOD MODE",  "Maximum Power / Unrestricted",   10.0f, 2.20f, ImVec4(0.95f, 0.15f, 0.20f, 1.0f) },
    { "SYSTEM RESET",   "Restore Native Defaults",        1.0f,  1.0f, ImVec4(0.00f, 0.85f, 1.00f, 1.0f) }
};

// Global States & Animation Memory
BoostMode g_CurrentMode = MODE_OFF;
bool g_IsActive = false;
float g_CustomForce = 1.0f;
float g_CustomSpeed = 1.0f;

struct ModeAnimState {
    float hoverAnim = 0.0f;     
    bool  isApplying = false;   
    float applyTimer = 0.0f;    
    bool  justSuccess = false;  
    float successTimer = 0.0f;
};
ModeAnimState g_AnimStates[7];

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
// Custom Animated Components (Fixed Hitbox & Hold Selection)
// ---------------------------------------------------------
bool AnimatedModeButton(int id, const char* label, const char* sublabel, ImVec4 accentColor, ImVec2 size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    ImVec2 pos = window->DC.CursorPos;
    
    char str_id[32];
    sprintf(str_id, "##mode_btn_%d", id);

    // 1. Invisible Button เพื่อทำ Hitbox รับการคลิกที่ถูกต้อง 100%
    bool pressed = ImGui::InvisibleButton(str_id, size);
    bool hovered = ImGui::IsItemHovered();
    
    ModeAnimState& anim = g_AnimStates[id];
    bool isSelected = (g_CurrentMode == id); // ตรวจสอบว่าโหมดนี้กำลังถูกเลือกค้างอยู่หรือไม่

    // 2. Smooth Hover Interpolation
    float delta = g.IO.DeltaTime * 12.0f;
    anim.hoverAnim = ImLerp(anim.hoverAnim, (hovered || isSelected) ? 1.0f : 0.0f, ImClamp(delta, 0.0f, 1.0f));

    // 3. Logic เมื่อมีการกดปุ่ม
    if (pressed && !anim.isApplying) {
        anim.isApplying = true;
        anim.applyTimer = 0.35f; // เวลาหมุน Spinner (0.35 วินาที ลื่นกำลังดี)
    }

    if (anim.isApplying) {
        anim.applyTimer -= g.IO.DeltaTime;
        if (anim.applyTimer <= 0.0f) {
            anim.isApplying = false;
            anim.justSuccess = true;
            anim.successTimer = 1.0f; // แสดงเครื่องหมายติ๊กถูก 1 วินาที
            ApplyPoolCueBoost((BoostMode)id);
        }
    }

    if (anim.justSuccess) {
        anim.successTimer -= g.IO.DeltaTime;
        if (anim.successTimer <= 0.0f) {
            anim.justSuccess = false;
        }
    }

    // 4. Render Visual Highlights (Background, Hold Glow, Border)
    ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    // สีพื้นหลัง (ถ้าถูกเลือกจะสว่างค้างเข้มขึ้น)
    ImVec4 baseColor = isSelected 
        ? ImVec4(accentColor.x * 0.25f, accentColor.y * 0.25f, accentColor.z * 0.25f, 0.90f)
        : ImLerp(ImVec4(0.10f, 0.11f, 0.14f, 0.85f), ImVec4(0.16f, 0.18f, 0.23f, 0.95f), anim.hoverAnim);

    window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(baseColor), 8.0f);

    // แถบ Glow ด้านข้าง + เส้นขอบ (จะสว่างค้างเมื่อถูก Hold Selection)
    if (anim.hoverAnim > 0.01f || isSelected) {
        float barWidth = isSelected ? 6.0f : ImLerp(2.0f, 5.0f, anim.hoverAnim);
        ImU32 glowColor = ImGui::GetColorU32(accentColor);
        
        // Left Selection Bar
        window->DrawList->AddRectFilled(
            bb.Min, 
            ImVec2(bb.Min.x + barWidth, bb.Max.y), 
            glowColor, 8.0f, ImDrawFlags_RoundCornersLeft);

        // Border Glow
        float alpha = isSelected ? 0.8f : anim.hoverAnim * 0.5f;
        window->DrawList->AddRect(
            bb.Min, bb.Max, 
            ImGui::GetColorU32(ImVec4(accentColor.x, accentColor.y, accentColor.z, alpha)), 
            8.0f, 0, 1.5f);
    }

    // 5. Render Spinner / Checkmark / Text
    ImVec2 textPos = ImVec2(bb.Min.x + 18.0f, bb.Min.y + 8.0f);

    if (anim.isApplying) {
        // --- SPINNER ---
        float radius = 9.0f;
        ImVec2 center = ImVec2(bb.Max.x - 28.0f, bb.Min.y + size.y * 0.5f);
        float time = (float)g.Time * 10.0f;
        window->DrawList->PathClear();
        for (int i = 0; i < 16; i++) {
            float a = time + ((float)i / 16.0f) * (IM_PI * 1.5f);
            window->DrawList->PathLineTo(ImVec2(center.x + cosf(a) * radius, center.y + sinf(a) * radius));
        }
        window->DrawList->PathStroke(ImGui::GetColorU32(accentColor), false, 2.5f);

        window->DrawList->AddText(textPos, IM_COL32(240, 240, 245, 255), label);
        window->DrawList->AddText(ImVec2(textPos.x, textPos.y + 18.0f), IM_COL32(130, 135, 150, 255), sublabel);
    } 
    else if (anim.justSuccess) {
        // --- CHECKMARK ---
        ImVec2 center = ImVec2(bb.Max.x - 28.0f, bb.Min.y + size.y * 0.5f);
        ImU32 greenColor = IM_COL32(40, 210, 90, 255);
        window->DrawList->AddCircleFilled(center, 10.0f, greenColor);
        window->DrawList->AddLine(ImVec2(center.x - 4, center.y), ImVec2(center.x - 1, center.y + 3), IM_COL32(255, 255, 255, 255), 2.0f);
        window->DrawList->AddLine(ImVec2(center.x - 1, center.y + 3), ImVec2(center.x + 4, center.y - 3), IM_COL32(255, 255, 255, 255), 2.0f);

        window->DrawList->AddText(textPos, IM_COL32(240, 240, 245, 255), label);
        window->DrawList->AddText(ImVec2(textPos.x, textPos.y + 18.0f), IM_COL32(130, 135, 150, 255), sublabel);
    } 
    else {
        // --- NORMAL / HOLD TEXT ---
        ImU32 titleColor = isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(220, 222, 230, 255);
        window->DrawList->AddText(textPos, titleColor, label);
        window->DrawList->AddText(ImVec2(textPos.x, textPos.y + 18.0f), IM_COL32(130, 135, 150, 255), sublabel);
    }

    return pressed;
}

// ---------------------------------------------------------
// Style Config
// ---------------------------------------------------------
void SetupModernStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding    = 16.0f;
    style.ChildRounding     = 12.0f;
    style.FrameRounding     = 8.0f;
    style.WindowPadding     = ImVec2(20, 20);
    style.ItemSpacing       = ImVec2(10, 10);
    style.WindowBorderSize  = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]           = ImVec4(0.06f, 0.06f, 0.08f, 0.96f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.09f, 0.10f, 0.13f, 0.80f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.95f, 0.75f, 0.18f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]   = ImVec4(1.00f, 0.85f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.12f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.18f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_Text]               = ImVec4(0.92f, 0.93f, 0.96f, 1.00f);
}

// ---------------------------------------------------------
// Custom UI Renderer
// ---------------------------------------------------------
void RenderPoolCueBoosterGUI(HWND hwnd) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("##MainFramelessWindow", nullptr, 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse);

    // Header Title & Window Drag Area
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.82f, 0.20f, 1.0f));
    ImGui::TextUnformatted("⚡ POOL CUE ULTRA BOOSTER v2.0");
    ImGui::PopStyleColor();
    ImGui::SameLine(ImGui::GetWindowWidth() - 35);

    // Exit Button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
    if (ImGui::Button("X", ImVec2(25, 25))) {
        ::PostQuitMessage(0);
    }
    ImGui::PopStyleColor(2);
    ImGui::EndGroup();

    if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ::ReleaseCapture();
        ::SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }

    ImGui::TextColored(ImVec4(0.45f, 0.48f, 0.55f, 1.0f), "Memory Native Engine Overdrive");
    ImGui::Separator();
    ImGui::Spacing();

    // Engine Status
    ImGui::BeginChild("StatusPanel", ImVec2(0, 56), true);
    {
        ImGui::Text("STATUS:");
        ImGui::SameLine();
        if (g_IsActive) {
            ImGui::TextColored(g_BoostConfigs[g_CurrentMode].color, "[ ACTIVE - %s ]", g_BoostConfigs[g_CurrentMode].name);
            ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 1.0f), "Profile: %s", g_BoostConfigs[g_CurrentMode].subtitle);
        } else {
            ImGui::TextColored(ImVec4(0.45f, 0.48f, 0.55f, 1.0f), "[ STANDBY / INACTIVE ]");
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.18f, 1.00f), "SELECT BOOST PROFILE:");

    // Render Mode Buttons (Now Work 100% & Hold Selection)
    for (int i = 1; i <= 5; ++i) {
        AnimatedModeButton(
            i, 
            g_BoostConfigs[i].name, 
            g_BoostConfigs[i].subtitle, 
            g_BoostConfigs[i].color, 
            ImVec2(-1, 46)
        );
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Live Adjustment Sliders
    ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.18f, 1.00f), "LIVE TUNING SLIDERS:");
    ImGui::SliderFloat("Force Multiplier", &g_CustomForce, 1.0f, 10.0f, "%.2fx Force");
    ImGui::SliderFloat("Speed Multiplier", &g_CustomSpeed, 1.0f, 3.0f, "%.2fx Speed");

    ImGui::Spacing();

    // System Reset
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.12f, 0.15f, 0.70f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.18f, 0.22f, 1.00f));
    if (ImGui::Button("SYSTEM RESET & FLUSH MEMORY", ImVec2(-1, 40))) {
        ApplyPoolCueBoost(MODE_SYSTEM_RESET);
    }
    ImGui::PopStyleColor(2);

    ImGui::End();
}

void SetupSmoothFonts(ImGuiIO& io) {
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 4;
    font_cfg.OversampleV = 4;

    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f, &font_cfg);
    if (!font) io.Fonts->AddFontDefault(&font_cfg);
}

// ---------------------------------------------------------
// WinMain Entry Point
// ---------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr, L"PoolCueBoosterClass", nullptr };
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName, 
        L"POOL CUE ULTRA BOOSTER v2.0", 
        WS_POPUP | WS_VISIBLE, 
        200, 200, 480, 580, 
        nullptr, nullptr, wc.hInstance, nullptr
    );

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

        RenderPoolCueBoosterGUI(hwnd);

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
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
