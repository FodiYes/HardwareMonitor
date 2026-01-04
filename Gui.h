#pragma once
#include "imgui.h"
#include "Hardware.h"
#include "Theme.h"

void SetWindowMode(HWND hwnd, bool pinned);

class Gui {
private:
    Hardware hw;
    bool isPinned = false; 
    bool shouldClose = false;
    HWND hwnd = nullptr;


    float appOpacity = 1.0f;

public:
    void SetHandle(HWND h) { hwnd = h; }
    bool ShouldClose() const { return shouldClose; }

    void SetPinnedState(bool pinned) { isPinned = pinned; }

    void Render() {
        hw.Update(ImGui::GetIO().DeltaTime);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        Theme::GlobalOpacity = appOpacity;

        ImGui::Begin("Widget", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize);

        ImVec2 size = ImGui::GetIO().DisplaySize;
        Theme::DrawGlassBackground(size);

        ImGui::SetCursorPos(ImVec2(16, 8));
        ImGui::TextColored(Theme::Col_TextDim, "SYSTEM MONITOR");

        if (!isPinned) {
            ImGui::SetCursorPos(ImVec2(size.x - 60, 4));
            if (Theme::IconButton("##Pin", "O", isPinned)) {
                TogglePin();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pin to Desktop (Click-through mode)");

            ImGui::SetCursorPos(ImVec2(size.x - 32, 4));
            if (Theme::IconButton("##Close", "X", false)) {
                shouldClose = true;
            }
        }
        else {
            ImGui::SetCursorPos(ImVec2(size.x - 120, 8));
            ImGui::TextColored(ImVec4(1, 1, 1, 0.3f), "Controls in Tray");
        }

        float radius = 38.0f;
        float contentY = 55.0f;
        float spacing = size.x / 3.0f;

        ImGui::SetCursorPos(ImVec2(spacing * 0.5f - radius, contentY));
        char cpuBuf[32]; sprintf(cpuBuf, "%.0f%%", hw.cpu_load);
        Theme::DrawGradientMetric("CPU", cpuBuf, hw.cpu_load, Theme::Col_CPU_Start, Theme::Col_CPU_End, radius);

        ImGui::SetCursorPos(ImVec2(spacing * 1.5f - radius, contentY));
        char gpuBuf[32]; sprintf(gpuBuf, "%.0f%%", hw.gpu_load);
        Theme::DrawGradientMetric("GPU", gpuBuf, hw.gpu_load, Theme::Col_GPU_Start, Theme::Col_GPU_End, radius);

        ImGui::SetCursorPos(ImVec2(spacing * 2.5f - radius, contentY));
        char ramBuf[32]; sprintf(ramBuf, "%.1f", hw.ram_usage);
        Theme::DrawGradientMetric("RAM", ramBuf, hw.ram_percent, Theme::Col_RAM_Start, Theme::Col_RAM_End, radius);

        if (!isPinned) {
            float sliderWidth = 140.0f;
            ImGui::SetCursorPos(ImVec2((size.x - sliderWidth) / 2, size.y - 25));
            Theme::SlimSlider("##Opacity", &appOpacity, 0.2f, 1.0f, sliderWidth);
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

private:
    void TogglePin() {
        isPinned = !isPinned;
        SetWindowMode(hwnd, isPinned);
    }
};