#include <iostream>
#include <windows.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

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
    ImGui::SetNextWindowSize(ImVec4(420, 380), ImGuiCond_FirstUseEver);
    
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
