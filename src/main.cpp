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
// Core Engine Enums & State Management
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
    const char* description;
    float forceMultiplier;
    float moveSpeedMultiplier;
    ImVec4 color;
};

BoostSettings g_BoostConfigs[] = {
    { "OFF",            "Engine Disabled",      "Standard game physics with zero modifications.",                     1.0f,  1.0f, ImVec4(0.45f, 0.48f, 0.55f, 1.0f) },
    { "STEALTH",        "Safe Stream Profile",  "Low profile boost suitable for stealth gameplay and streaming.",    1.25f, 1.10f, ImVec4(0.20f, 0.80f, 0.40f, 1.0f) },
    { "ROLEPLAY",       "Balanced Physics",     "Optimized for smooth roleplay interactions with minor boosts.",      1.50f, 1.15f, ImVec4(0.15f, 0.65f, 1.00f, 1.0f) },
    { "BALANCED",       "High Impact Mode",     "Noticeable boost to movement speed and impulse forces.",             2.00f, 1.30f, ImVec4(1.00f, 0.75f, 0.00f, 1.0f) },
    { "OVERDRIVE",      "Aggressive Engine",    "Extreme physics amplification for intense situations.",              3.50f, 1.60f, ImVec4(1.00f, 0.35f, 0.00f, 1.0f) },
    { "UNRESTRICTED",   "Maximum Output",       "Uncapped power output. Complete physics displacement.",              10.0f, 2.20f, ImVec4(0.95f, 0.15f, 0.20f, 1.0f) },
    { "SYSTEM RESET",   "Restore Defaults",     "Flushes active memory hooks and reverts to stock engine state.",    1.0f,  1.0f, ImVec4(0.00f, 0.85f, 1.00f, 1.0f) }
};

// Global App States
BoostMode g_CurrentMode = MODE_OFF;
bool g_IsActive = false;
float g_CustomForce = 1.0f;
float g_CustomSpeed = 1.0f;
int g_CurrentTab = 0; // 0: Overview, 1: Profiles, 2: Fine Tuning

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
// Custom Animated UI Widgets
// ---------------------------------------------------------

// Custom Modern Toggle Switch
bool RenderToggleSwitch(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    
    ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    ImVec2 pos = window->DC.CursorPos;
    float height = 22.0f;
    float width = 42.0f;

    ImRect bb(pos, ImVec2(pos.x + width + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), pos.y + height));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) {
        *v = !(*v);
        ImGui::MarkItemEdited(id);
    }

    float t = *v ? 1.0f : 0.0f;
    ImU32 bg_color = *v ? IM_COL32(40, 200, 100, 255) : IM_COL32(50, 55, 65, 255);
    
    ImRect switch_bb(pos, ImVec2(pos.x + width, pos.y + height));
    window->DrawList->AddRectFilled(switch_bb.Min, switch_bb.Max, bg_color, height * 0.5f);
    
    float knob_pos_x = *v ? (switch_bb.Max.x - height * 0.5f) : (switch_bb.Min.x + height * 0.5f);
    window->DrawList->AddCircleFilled(ImVec2(knob_pos_x, switch_bb.Min.y + height * 0.5f), (height * 0.5f) - 3.0f, IM_COL32(255, 255, 255, 255));

    if (label_size.x > 0.0f) {
        ImGui::RenderText(ImVec2(switch_bb.Max.x + style.ItemInnerSpacing.x, pos.y + (height - label_size.y) * 0.5f), label);
    }

    return pressed;
}

// Custom Profile Selection Card
bool RenderProfileCard(int id, const char* name, const char* subtitle, const char* desc, ImVec4 accentColor, ImVec2 size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    ImVec2 pos = window->DC.CursorPos;
    
    char str_id[32];
    sprintf(str_id, "##profile_card_%d", id);

    bool pressed = ImGui::InvisibleButton(str_id, size);
    bool hovered = ImGui::IsItemHovered();
    
    ModeAnimState& anim = g_AnimStates[id];
    bool isSelected = (g_CurrentMode == id);

    float delta = g.IO.DeltaTime * 12.0f;
    anim.hoverAnim = ImLerp(anim.hoverAnim, (hovered || isSelected) ? 1.0f : 0.0f, ImClamp(delta, 0.0f, 1.0f));

    if (pressed && !anim.isApplying) {
        anim.isApplying = true;
        anim.applyTimer = 0.30f; 
    }

    if (anim.isApplying) {
        anim.applyTimer -= g.IO.DeltaTime;
        if (anim.applyTimer <= 0.0f) {
            anim.isApplying = false;
            anim.justSuccess = true;
            anim.successTimer = 0.8f; 
            ApplyPoolCueBoost((BoostMode)id);
        }
    }

    if (anim.justSuccess) {
        anim.successTimer -= g.IO.DeltaTime;
        if (anim.successTimer <= 0.0f) {
            anim.justSuccess = false;
        }
    }

    ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImVec4 baseColor = isSelected 
        ? ImVec4(accentColor.x * 0.22f, accentColor.y * 0.22f, accentColor.z * 0.22f, 0.90f)
        : ImLerp(ImVec4(0.10f, 0.11f, 0.14f, 0.85f), ImVec4(0.15f, 0.17f, 0.22f, 0.95f), anim.hoverAnim);

    window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(baseColor), 10.0f);

    if (anim.hoverAnim > 0.01f || isSelected) {
        float barWidth = isSelected ? 5.0f : ImLerp(2.0f, 4.0f, anim.hoverAnim);
        ImU32 glowColor = ImGui::GetColorU32(accentColor);
        
        window->DrawList->AddRectFilled(
            bb.Min, 
            ImVec2(bb.Min.x + barWidth, bb.Max.y), 
            glowColor, 10.0f, ImDrawFlags_RoundCornersLeft);

        float alpha = isSelected ? 0.8f : anim.hoverAnim * 0.4f;
        window->DrawList->AddRect(
            bb.Min, bb.Max, 
            ImGui::GetColorU32(ImVec4(accentColor.x, accentColor.y, accentColor.z, alpha)), 
            10.0f, 0, 1.2f);
    }

    ImVec2 textPos = ImVec2(bb.Min.x + 16.0f, bb.Min.y + 8.0f);

    if (anim.isApplying) {
        float radius = 8.0f;
        ImVec2 center = ImVec2(bb.Max.x - 24.0f, bb.Min.y + 20.0f);
        float time = (float)g.Time * 10.0f;
        window->DrawList->PathClear();
        for (int i = 0; i < 16; i++) {
            float a = time + ((float)i / 16.0f) * (IM_PI * 1.5f);
            window->DrawList->PathLineTo(ImVec2(center.x + cosf(a) * radius, center.y + sinf(a) * radius));
        }
        window->DrawList->PathStroke(ImGui::GetColorU32(accentColor), false, 2.5f);
    } 
    else if (anim.justSuccess) {
        ImVec2 center = ImVec2(bb.Max.x - 24.0f, bb.Min.y + 20.0f);
        ImU32 greenColor = IM_COL32(40, 210, 90, 255);
        window->DrawList->AddCircleFilled(center, 9.0f, greenColor);
        window->DrawList->AddLine(ImVec2(center.x - 4, center.y), ImVec2(center.x - 1, center.y + 3), IM_COL32(255, 255, 255, 255), 2.0f);
        window->DrawList->AddLine(ImVec2(center.x - 1, center.y + 3), ImVec2(center.x + 4, center.y - 3), IM_COL32(255, 255, 255, 255), 2.0f);
    }

    ImU32 titleColor = isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(220, 225, 235, 255);
    window->DrawList->AddText(textPos, titleColor, name);
    window->DrawList->AddText(ImVec2(textPos.x + ImGui::CalcTextSize(name).x + 10.0f, textPos.y + 1.0f), IM_COL32(140, 145, 160, 255), subtitle);
    window->DrawList->AddText(ImVec2(textPos.x, textPos.y + 20.0f), IM_COL32(120, 125, 140, 255), desc);

    return pressed;
}

// ---------------------------------------------------------
// Global UI Theme Configuration
// ---------------------------------------------------------
void SetupModernStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding    = 14.0f;
    style.ChildRounding     = 10.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.WindowPadding     = ImVec2(16, 16);
    style.ItemSpacing       = ImVec2(10, 10);
    style.WindowBorderSize  = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]           = ImVec4(0.07f, 0.08f, 0.10f, 0.96f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.09f, 0.10f, 0.13f, 0.70f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.95f, 0.75f, 0.18f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]   = ImVec4(1.00f, 0.85f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.12f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.18f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.12f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.18f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.24f, 0.27f, 0.35f, 1.00f);
    colors[ImGuiCol_Text]               = ImVec4(0.92f, 0.93f, 0.96f, 1.00f);
}

// ---------------------------------------------------------
// Main UI Rendering Engine
// ---------------------------------------------------------
void RenderPoolCueBoosterGUI(HWND hwnd) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("##MainFramelessWindow", nullptr, 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse);

    // --- TOP BAR: Header & Window Controls ---
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.82f, 0.20f, 1.0f));
    ImGui::TextUnformatted("⚡ POOL CUE ENGINE");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.40f, 0.43f, 0.50f, 1.0f), "v2.0");

    // Right-aligned Window Buttons
    float winWidth = ImGui::GetWindowWidth();
    ImGui::SameLine(winWidth - 60);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.25f, 0.8f));
    if (ImGui::Button("-", ImVec2(24, 24))) {
        ::ShowWindow(hwnd, SW_MINIMIZE);
    }
    ImGui::SameLine();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 0.8f));
    if (ImGui::Button("X", ImVec2(24, 24))) {
        ::PostQuitMessage(0);
    }
    ImGui::PopStyleColor(2);
    ImGui::EndGroup();

    // Drag Window Behavior
    if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ::ReleaseCapture();
        ::SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }

    ImGui::Spacing();

    // --- NAVIGATION TABS ---
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
    const char* tabs[] = { " Dashboard ", " Profiles ", " Fine Tuning " };
    for (int i = 0; i < 3; i++) {
        bool isTabActive = (g_CurrentTab == i);
        if (isTabActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.68f, 0.15f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05f, 0.05f, 0.05f, 1.00f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.12f, 0.15f, 0.80f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.72f, 0.78f, 1.00f));
        }

        if (ImGui::Button(tabs[i], ImVec2(120, 32))) {
            g_CurrentTab = i;
        }
        ImGui::PopStyleColor(2);
        if (i < 2) ImGui::SameLine();
    }
    ImGui::PopStyleVar();

    ImGui::Separator();
    ImGui::Spacing();

    // --- TAB 0: DASHBOARD OVERVIEW ---
    if (g_CurrentTab == 0) {
        ImGui::BeginChild("DashboardTab", ImVec2(0, 0), false);

        // Status Card
        ImGui::BeginChild("StatusCard", ImVec2(0, 80), true);
        {
            ImGui::TextColored(ImVec4(0.5f, 0.53f, 0.6f, 1.0f), "SYSTEM STATE");
            ImGui::SameLine(ImGui::GetWindowWidth() - 110);
            
            bool tempActive = g_IsActive;
            if (RenderToggleSwitch("##MasterSwitch", &tempActive)) {
                if (!tempActive) ApplyPoolCueBoost(MODE_OFF);
                else ApplyPoolCueBoost(MODE_LOW);
            }

            if (g_IsActive) {
                ImGui::TextColored(g_BoostConfigs[g_CurrentMode].color, "● %s PROFILE ACTIVE", g_BoostConfigs[g_CurrentMode].name);
                ImGui::TextColored(ImVec4(0.7f, 0.73f, 0.8f, 1.0f), "Active Force: %.2fx | Active Speed: %.2fx", g_CustomForce, g_CustomSpeed);
            } else {
                ImGui::TextColored(ImVec4(0.45f, 0.48f, 0.55f, 1.0f), "○ ENGINE STANDBY / INACTIVE");
                ImGui::TextColored(ImVec4(0.40f, 0.43f, 0.50f, 1.0f), "Select a profile or toggle switch to enable.");
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.18f, 1.00f), "QUICK PROFILE SELECTOR:");
        
        // Show Top 3 Popular Profiles
        for (int i = 1; i <= 3; ++i) {
            RenderProfileCard(
                i, 
                g_BoostConfigs[i].name, 
                g_BoostConfigs[i].subtitle, 
                g_BoostConfigs[i].description,
                g_BoostConfigs[i].color, 
                ImVec2(-1, 50)
            );
            ImGui::Spacing();
        }

        ImGui::EndChild();
    }

    // --- TAB 1: ALL PROFILES ---
    else if (g_CurrentTab == 1) {
        ImGui::BeginChild("ProfilesTab", ImVec2(0, 0), false);
        ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.18f, 1.00f), "ALL POWER PROFILES:");
        ImGui::Spacing();

        for (int i = 1; i <= 5; ++i) {
            RenderProfileCard(
                i, 
                g_BoostConfigs[i].name, 
                g_BoostConfigs[i].subtitle, 
                g_BoostConfigs[i].description,
                g_BoostConfigs[i].color, 
                ImVec2(-1, 52)
            );
            ImGui::Spacing();
        }

        ImGui::EndChild();
    }

    // --- TAB 2: FINE TUNING & RESET ---
    else if (g_CurrentTab == 2) {
        ImGui::BeginChild("TuningTab", ImVec2(0, 0), false);
        
        ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.18f, 1.00f), "MANUAL MULTIPLIER TUNING:");
        ImGui::Spacing();

        ImGui::BeginChild("SliderGroup", ImVec2(0, 140), true);
        {
            ImGui::Text("Force Impact Multiplier");
            ImGui::SliderFloat("##ForceSlider", &g_CustomForce, 1.0f, 10.0f, "%.2fx Force");
            ImGui::TextColored(ImVec4(0.45f, 0.48f, 0.55f, 1.0f), "Controls physical impulse strength on collision.");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Movement Speed Multiplier");
            ImGui::SliderFloat("##SpeedSlider", &g_CustomSpeed, 1.0f, 3.0f, "%.2fx Speed");
            ImGui::TextColored(ImVec4(0.45f, 0.48f, 0.55f, 1.0f), "Amplifies directional velocity dynamics.");
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.95f, 0.30f, 0.30f, 1.00f), "DANGER ZONE:");
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.12f, 0.15f, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.18f, 0.22f, 1.00f));
        if (ImGui::Button("SYSTEM RESET & FLUSH MEMORY HOOKS", ImVec2(-1, 42))) {
            ApplyPoolCueBoost(MODE_SYSTEM_RESET);
        }
        ImGui::PopStyleColor(2);

        ImGui::EndChild();
    }

    ImGui::End();
}

void SetupSmoothFonts(ImGuiIO& io) {
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 4;
    font_cfg.OversampleV = 4;

    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &font_cfg);
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
        200, 200, 480, 440, 
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
