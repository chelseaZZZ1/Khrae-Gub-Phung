#include <iostream>
#include <windows.h>
#include <d3d11.h>
#include <cmath>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Direct3D Global Globals
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
// Global App States & Keybind Management
// ---------------------------------------------------------
enum BoostMode {
    MODE_OFF = 0,
    MODE_STEALTH,
    MODE_ROLEPLAY,
    MODE_BALANCED,
    MODE_OVERDRIVE,
    MODE_UNRESTRICTED,
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
    { "DISABLED",     "Standard Mode",        "Zero engine modifications applied.",                                1.0f,  1.0f, ImVec4(0.40f, 0.45f, 0.52f, 1.0f) },
    { "STEALTH",      "Safe Stream Profile",  "Low profile boost suitable for stealth gameplay and streaming.",    1.25f, 1.10f, ImVec4(0.20f, 0.85f, 0.45f, 1.0f) },
    { "ROLEPLAY",     "Balanced Physics",     "Optimized for smooth roleplay interactions with minor boosts.",      1.50f, 1.15f, ImVec4(0.15f, 0.70f, 1.00f, 1.0f) },
    { "BALANCED",     "High Impact Mode",     "Noticeable boost to movement speed and impulse forces.",             2.00f, 1.30f, ImVec4(1.00f, 0.70f, 0.00f, 1.0f) },
    { "OVERDRIVE",    "Aggressive Engine",    "Extreme physics amplification for intense situations.",              3.50f, 1.60f, ImVec4(1.00f, 0.25f, 0.35f, 1.0f) },
    { "UNRESTRICTED", "Maximum Output",       "Uncapped power output. Complete physics displacement.",              10.0f, 2.20f, ImVec4(0.85f, 0.15f, 1.00f, 1.0f) },
    { "FLUSH HOOKS",  "Restore System",       "Flushes active memory hooks and reverts to stock engine state.",    1.0f,  1.0f, ImVec4(0.00f, 0.85f, 1.00f, 1.0f) }
};

BoostMode g_CurrentMode = MODE_OFF;
bool g_IsActive = false;
bool g_IsWindowVisible = true;
bool g_IsFullscreen = false;
float g_CustomForce = 1.0f;
float g_CustomSpeed = 1.0f;
int g_CurrentTab = 0;

// Window Geometry Cache
RECT g_WindowRect = { 200, 200, 700, 680 };

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

// Fullscreen Switcher
void ToggleFullscreen(HWND hwnd) {
    g_IsFullscreen = !g_IsFullscreen;
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);

    if (g_IsFullscreen) {
        GetWindowRect(hwnd, &g_WindowRect);
        SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(hwnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_POPUP);
        SetWindowPos(hwnd, nullptr, g_WindowRect.left, g_WindowRect.top, g_WindowRect.right - g_WindowRect.left, g_WindowRect.bottom - g_WindowRect.top, SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

// ---------------------------------------------------------
// Custom Ultra Modern UI Widgets
// ---------------------------------------------------------

// Smooth Circular Window Control Buttons (macOS / Cyber Style)
bool RenderCircleButton(const char* id_str, ImVec4 col, ImVec4 hoverCol, float radius = 7.0f) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImVec2 pos = window->DC.CursorPos;
    ImRect bb(pos, ImVec2(pos.x + radius * 2.0f, pos.y + radius * 2.0f));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, window->GetID(id_str))) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, window->GetID(id_str), &hovered, &held);

    ImVec4 finalCol = hovered ? hoverCol : col;
    window->DrawList->AddCircleFilled(ImVec2(pos.x + radius, pos.y + radius), radius, ImGui::GetColorU32(finalCol));
    if (hovered) {
        window->DrawList->AddCircle(ImVec2(pos.x + radius, pos.y + radius), radius + 2.0f, ImGui::GetColorU32(hoverCol), 0, 1.5f);
    }

    return pressed;
}

// Animated Glow Toggle Switch
bool RenderToggleSwitch(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    
    ImVec2 pos = window->DC.CursorPos;
    float height = 24.0f;
    float width = 48.0f;

    ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) {
        *v = !(*v);
        ImGui::MarkItemEdited(id);
    }

    ImU32 bg_color = *v ? IM_COL32(0, 230, 120, 255) : IM_COL32(35, 38, 48, 255);
    window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_color, height * 0.5f);

    if (*v) {
        window->DrawList->AddRect(bb.Min, bb.Max, IM_COL32(0, 255, 140, 180), height * 0.5f, 0, 2.0f);
    }
    
    float knob_pos_x = *v ? (bb.Max.x - height * 0.5f) : (bb.Min.x + height * 0.5f);
    window->DrawList->AddCircleFilled(ImVec2(knob_pos_x, bb.Min.y + height * 0.5f), (height * 0.5f) - 3.0f, IM_COL32(255, 255, 255, 255));

    return pressed;
}

// Ultra Card with Dynamic Hover Glow
bool RenderProfileCard(int id, const char* name, const char* subtitle, const char* desc, ImVec4 accentColor, ImVec2 size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    ImVec2 pos = window->DC.CursorPos;
    
    char str_id[32];
    sprintf(str_id, "##prof_card_%d", id);

    bool pressed = ImGui::InvisibleButton(str_id, size);
    bool hovered = ImGui::IsItemHovered();
    
    ModeAnimState& anim = g_AnimStates[id];
    bool isSelected = (g_CurrentMode == id);

    float delta = g.IO.DeltaTime * 14.0f;
    anim.hoverAnim = ImLerp(anim.hoverAnim, (hovered || isSelected) ? 1.0f : 0.0f, ImClamp(delta, 0.0f, 1.0f));

    if (pressed && !anim.isApplying) {
        anim.isApplying = true;
        anim.applyTimer = 0.20f; 
    }

    if (anim.isApplying) {
        anim.applyTimer -= g.IO.DeltaTime;
        if (anim.applyTimer <= 0.0f) {
            anim.isApplying = false;
            anim.justSuccess = true;
            anim.successTimer = 0.6f; 
            ApplyPoolCueBoost((BoostMode)id);
        }
    }

    if (anim.justSuccess) {
        anim.successTimer -= g.IO.DeltaTime;
        if (anim.successTimer <= 0.0f) anim.justSuccess = false;
    }

    ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImVec4 baseColor = isSelected 
        ? ImVec4(accentColor.x * 0.18f, accentColor.y * 0.18f, accentColor.z * 0.18f, 0.95f)
        : ImLerp(ImVec4(0.08f, 0.09f, 0.12f, 0.90f), ImVec4(0.12f, 0.14f, 0.18f, 0.95f), anim.hoverAnim);

    window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(baseColor), 12.0f);

    if (anim.hoverAnim > 0.01f || isSelected) {
        float alpha = isSelected ? 0.9f : anim.hoverAnim * 0.5f;
        window->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ImVec4(accentColor.x, accentColor.y, accentColor.z, alpha)), 12.0f, 0, 1.5f);
    }

    ImVec2 textPos = ImVec2(bb.Min.x + 18.0f, bb.Min.y + 10.0f);

    if (anim.isApplying) {
        float radius = 8.0f;
        ImVec2 center = ImVec2(bb.Max.x - 28.0f, bb.Min.y + 24.0f);
        float time = (float)g.Time * 12.0f;
        window->DrawList->PathClear();
        for (int i = 0; i < 16; i++) {
            float a = time + ((float)i / 16.0f) * (IM_PI * 1.5f);
            window->DrawList->PathLineTo(ImVec2(center.x + cosf(a) * radius, center.y + sinf(a) * radius));
        }
        window->DrawList->PathStroke(ImGui::GetColorU32(accentColor), false, 2.5f);
    } 
    else if (anim.justSuccess) {
        ImVec2 center = ImVec2(bb.Max.x - 28.0f, bb.Min.y + 24.0f);
        window->DrawList->AddCircleFilled(center, 9.0f, IM_COL32(0, 230, 120, 255));
        window->DrawList->AddLine(ImVec2(center.x - 4, center.y), ImVec2(center.x - 1, center.y + 3), IM_COL32(255, 255, 255, 255), 2.0f);
        window->DrawList->AddLine(ImVec2(center.x - 1, center.y + 3), ImVec2(center.x + 4, center.y - 3), IM_COL32(255, 255, 255, 255), 2.0f);
    }

    ImU32 titleColor = isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(215, 220, 230, 255);
    window->DrawList->AddText(textPos, titleColor, name);
    window->DrawList->AddText(ImVec2(textPos.x + ImGui::CalcTextSize(name).x + 12.0f, textPos.y + 1.0f), IM_COL32(130, 135, 150, 255), subtitle);
    window->DrawList->AddText(ImVec2(textPos.x, textPos.y + 22.0f), IM_COL32(110, 115, 130, 255), desc);

    return pressed;
}

// ---------------------------------------------------------
// Global UI Theme Configuration
// ---------------------------------------------------------
void SetupCyberStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding    = 16.0f;
    style.ChildRounding     = 12.0f;
    style.FrameRounding     = 8.0f;
    style.PopupRounding     = 10.0f;
    style.WindowPadding     = ImVec2(20, 20);
    style.ItemSpacing       = ImVec2(12, 12);
    style.WindowBorderSize  = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]           = ImVec4(0.05f, 0.06f, 0.08f, 0.98f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.08f, 0.09f, 0.12f, 0.75f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.00f, 0.85f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]   = ImVec4(0.30f, 0.95f, 1.00f, 1.00f);
    colors[ImGuiCol_FrameBg]            = ImVec4(0.10f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.15f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_Button]             = ImVec4(0.10f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.16f, 0.19f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.22f, 0.26f, 0.35f, 1.00f);
    colors[ImGuiCol_Text]               = ImVec4(0.92f, 0.94f, 0.98f, 1.00f);
}

// ---------------------------------------------------------
// Main UI Rendering Engine
// ---------------------------------------------------------
void RenderPoolCueBoosterGUI(HWND hwnd) {
    if (!g_IsWindowVisible) return;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("##CyberMainWindow", nullptr, 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse);

    // --- TOP BAR: Header & Window Controls ---
    ImGui::BeginGroup();
    
    // Smooth Round Window Action Buttons (Red: Exit, Yellow: Minimize, Green: Fullscreen)
    if (RenderCircleButton("btn_close", ImVec4(0.95f, 0.25f, 0.25f, 1.0f), ImVec4(1.00f, 0.40f, 0.40f, 1.0f))) {
        ::PostQuitMessage(0); // 🔴 กดปิดโปรแกรมได้จริง!
    }
    ImGui::SameLine();
    if (RenderCircleButton("btn_min", ImVec4(0.95f, 0.70f, 0.20f, 1.0f), ImVec4(1.00f, 0.82f, 0.35f, 1.0f))) {
        ::ShowWindow(hwnd, SW_MINIMIZE); // 🟡 พับเก็บหน้าต่าง
    }
    ImGui::SameLine();
    if (RenderCircleButton("btn_full", ImVec4(0.20f, 0.85f, 0.35f, 1.0f), ImVec4(0.40f, 0.95f, 0.55f, 1.0f))) {
        ToggleFullscreen(hwnd); // 🟢 สลับโหมดเต็มจอ Fullscreen
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(110);
    ImGui::TextColored(ImVec4(0.00f, 0.85f, 1.00f, 1.0f), "⚡ POOL CUE ULTRA");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.50f, 1.0f), "|  PRESS [DEL] TO HIDE");

    ImGui::EndGroup();

    // Smooth Dragging Area
    if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !g_IsFullscreen) {
        ::ReleaseCapture();
        ::SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }

    ImGui::Spacing();

    // --- NAVIGATION TABS ---
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));
    const char* tabs[] = { " DASHBOARD ", " POWER PROFILES ", " FINE TUNING " };
    for (int i = 0; i < 3; i++) {
        bool isTabActive = (g_CurrentTab == i);
        if (isTabActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.80f, 0.95f, 1.00f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.02f, 0.04f, 0.08f, 1.00f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f, 0.10f, 0.14f, 0.80f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.65f, 0.75f, 1.00f));
        }

        if (ImGui::Button(tabs[i], ImVec2(130, 36))) {
            g_CurrentTab = i;
        }
        ImGui::PopStyleColor(2);
        if (i < 2) ImGui::SameLine();
    }
    ImGui::PopStyleVar();

    ImGui::Separator();
    ImGui::Spacing();

    // --- TAB 0: DASHBOARD ---
    if (g_CurrentTab == 0) {
        ImGui::BeginChild("DashboardTab", ImVec2(0, 0), false);

        // Active Status Overview Box
        ImGui::BeginChild("StatusCard", ImVec2(0, 90), true);
        {
            ImGui::TextColored(ImVec4(0.45f, 0.50f, 0.60f, 1.0f), "ENGINE STATUS");
            ImGui::SameLine(ImGui::GetWindowWidth() - 70);
            
            bool tempActive = g_IsActive;
            if (RenderToggleSwitch("##MasterSwitch", &tempActive)) {
                if (!tempActive) ApplyPoolCueBoost(MODE_OFF);
                else ApplyPoolCueBoost(MODE_STEALTH);
            }

            if (g_IsActive) {
                ImGui::TextColored(g_BoostConfigs[g_CurrentMode].color, "● %s ACTIVE", g_BoostConfigs[g_CurrentMode].name);
                ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.85f, 1.0f), "Force Impact: %.2fx  |  Move Velocity: %.2fx", g_CustomForce, g_CustomSpeed);
            } else {
                ImGui::TextColored(ImVec4(0.40f, 0.45f, 0.55f, 1.0f), "○ STANDBY / INACTIVE");
                ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.50f, 1.0f), "Toggle switch or choose a profile below.");
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.00f, 0.85f, 1.00f, 1.00f), "QUICK SELECT PROFILES:");
        ImGui::Spacing();
        
        for (int i = 1; i <= 3; ++i) {
            RenderProfileCard(
                i, 
                g_BoostConfigs[i].name, 
                g_BoostConfigs[i].subtitle, 
                g_BoostConfigs[i].description,
                g_BoostConfigs[i].color, 
                ImVec2(-1, 56)
            );
            ImGui::Spacing();
        }

        ImGui::EndChild();
    }

    // --- TAB 1: ALL PROFILES ---
    else if (g_CurrentTab == 1) {
        ImGui::BeginChild("ProfilesTab", ImVec2(0, 0), false);
        ImGui::TextColored(ImVec4(0.00f, 0.85f, 1.00f, 1.00f), "ALL ENGINE POWER PROFILES:");
        ImGui::Spacing();

        for (int i = 1; i <= 5; ++i) {
            RenderProfileCard(
                i, 
                g_BoostConfigs[i].name, 
                g_BoostConfigs[i].subtitle, 
                g_BoostConfigs[i].description,
                g_BoostConfigs[i].color, 
                ImVec2(-1, 56)
            );
            ImGui::Spacing();
        }

        ImGui::EndChild();
    }

    // --- TAB 2: FINE TUNING ---
    else if (g_CurrentTab == 2) {
        ImGui::BeginChild("TuningTab", ImVec2(0, 0), false);
        
        ImGui::TextColored(ImVec4(0.00f, 0.85f, 1.00f, 1.00f), "MANUAL PHYSICS FINE TUNING:");
        ImGui::Spacing();

        ImGui::BeginChild("SliderGroup", ImVec2(0, 160), true);
        {
            ImGui::Text("Force Impact Multiplier");
            ImGui::SliderFloat("##ForceSlider", &g_CustomForce, 1.0f, 10.0f, "%.2fx Force");
            ImGui::TextColored(ImVec4(0.40f, 0.45f, 0.55f, 1.0f), "Amplifies collision impulse forces.");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Move Speed Multiplier");
            ImGui::SliderFloat("##SpeedSlider", &g_CustomSpeed, 1.0f, 3.0f, "%.2fx Speed");
            ImGui::TextColored(ImVec4(0.40f, 0.45f, 0.55f, 1.0f), "Boosts directional travel dynamics.");
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(1.00f, 0.25f, 0.25f, 1.00f), "SYSTEM RESET:");
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.10f, 0.15f, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.15f, 0.20f, 1.00f));
        if (ImGui::Button("FLUSH MEMORY HOOKS & REVERT DEFAULTS", ImVec2(-1, 44))) {
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
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr, L"CyberBoosterClass", nullptr };
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_TOPMOST,
        wc.lpszClassName, 
        L"POOL CUE ULTRA BOOSTER", 
        WS_POPUP | WS_VISIBLE, 
        g_WindowRect.left, g_WindowRect.top, 
        g_WindowRect.right, g_WindowRect.bottom, 
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
    SetupCyberStyle();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool done = false;
    bool delKeyPressedLastFrame = false;

    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        // ---------------------------------------------------------
        // Global Keybind Detection: Toggle Hide/Show with [DELETE]
        // ---------------------------------------------------------
        bool delKeyPressedCurrent = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;
        if (delKeyPressedCurrent && !delKeyPressedLastFrame) {
            g_IsWindowVisible = !g_IsWindowVisible;
            if (g_IsWindowVisible) {
                ::ShowWindow(hwnd, SW_SHOW);
            } else {
                ::ShowWindow(hwnd, SW_HIDE);
            }
        }
        delKeyPressedLastFrame = delKeyPressedCurrent;

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
