#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <string>
#include <cstdio>
#include <algorithm>
#include <cmath>

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

class Theme {
public:
    static void* GlowTexture;

    static float GlobalOpacity;

    static constexpr ImVec4 Col_Bg = ImVec4(0.05f, 0.05f, 0.05f, 0.85f);
    static constexpr ImVec4 Col_RingBg = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    static constexpr ImVec4 Col_Text = ImVec4(1.00f, 1.00f, 1.00f, 0.95f);
    static constexpr ImVec4 Col_TextDim = ImVec4(1.00f, 1.00f, 1.00f, 0.45f);

    static constexpr ImVec4 Col_CPU_Start = ImVec4(0.0f, 0.6f, 1.0f, 1.0f);
    static constexpr ImVec4 Col_CPU_End = ImVec4(0.0f, 0.9f, 0.9f, 1.0f);
    static constexpr ImVec4 Col_GPU_Start = ImVec4(0.6f, 0.2f, 1.0f, 1.0f);
    static constexpr ImVec4 Col_GPU_End = ImVec4(0.9f, 0.2f, 0.6f, 1.0f);
    static constexpr ImVec4 Col_RAM_Start = ImVec4(1.0f, 0.5f, 0.2f, 1.0f);
    static constexpr ImVec4 Col_RAM_End = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);

    static void Setup() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 20.0f;
        style.FrameRounding = 12.0f;
        style.ItemSpacing = ImVec2(10, 10);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
        style.AntiAliasedFill = true;
        style.AntiAliasedLines = true;
    }

    static void DrawGlassBackground(ImVec2 size) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();

        ImVec4 bgCol = Col_Bg;
        bgCol.w *= GlobalOpacity; 

        ImVec4 borderCol = ImVec4(1.0f, 1.0f, 1.0f, 0.12f * GlobalOpacity);

        dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), ImColor(bgCol), 20.0f);
        dl->AddRect(ImVec2(p.x + 1, p.y + 1), ImVec2(p.x + size.x - 1, p.y + size.y - 1),
            ImColor(borderCol), 20.0f, 0, 1.0f);
    }

    static bool SlimSlider(const char* label, float* v, float v_min, float v_max, float width) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImVec2 p = ImGui::GetCursorScreenPos();
        float height = 20.0f;

        ImGui::PushID(label);
        bool pressed = ImGui::InvisibleButton("##SliderHit", ImVec2(width, height));
        bool active = ImGui::IsItemActive();
        bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        if (active) {
            float mouseX = ImGui::GetIO().MousePos.x;
            float t = (mouseX - p.x) / width;
            t = std::max(0.0f, std::min(1.0f, t));
            *v = v_min + (v_max - v_min) * t;
        }

        ImDrawList* dl = window->DrawList;
        float fraction = (*v - v_min) / (v_max - v_min);

        float railY = p.y + height * 0.5f;
        float railThickness = 2.0f;

        dl->AddRectFilled(
            ImVec2(p.x, railY - railThickness / 2),
            ImVec2(p.x + width, railY + railThickness / 2),
            ImColor(1.0f, 1.0f, 1.0f, 0.1f * GlobalOpacity),
            railThickness
        );

        float knobX = p.x + width * fraction;
        if (fraction > 0.0f) {
            ImVec4 c1 = Col_CPU_Start; c1.w *= GlobalOpacity;
            ImVec4 c2 = Col_GPU_End;   c2.w *= GlobalOpacity;

            dl->AddRectFilledMultiColor(
                ImVec2(p.x, railY - railThickness / 2),
                ImVec2(knobX, railY + railThickness / 2),
                ImColor(c1), ImColor(c2),
                ImColor(c2), ImColor(c1)
            );
        }

        float kR = (active || hovered) ? 6.0f : 5.0f;
        ImVec2 knobPos = ImVec2(knobX, railY);
        ImVec4 knobCol = ImVec4(1, 1, 1, 1.0f * GlobalOpacity);

        if (GlowTexture) {
            dl->AddImage(GlowTexture,
                ImVec2(knobPos.x - kR * 3, knobPos.y - kR * 3),
                ImVec2(knobPos.x + kR * 3, knobPos.y + kR * 3),
                ImVec2(0, 0), ImVec2(1, 1), ImColor(1.0f, 1.0f, 1.0f, 0.4f * GlobalOpacity));
        }

        dl->AddCircleFilled(knobPos, kR, ImColor(knobCol));
        return active || pressed;
    }

    static void DrawGradientMetric(const char* label, const char* valueStr, float percent, ImVec4 colStart, ImVec4 colEnd, float radius) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 center = ImVec2(p.x + radius, p.y + radius);

        float thickness = 5.0f;
        float startAngle = -IM_PI / 2.0f;

        ImVec4 ringBg = Col_RingBg;
        ringBg.w *= GlobalOpacity;
        dl->AddCircle(center, radius, ImColor(ringBg), 128, thickness);

        float safePercent = std::max(0.0f, percent);
        float progressRad = (2.0f * IM_PI * (safePercent / 100.0f));
        float currentAngle = startAngle + progressRad;

        if (safePercent > 0.5f) {
            int segments = 60;
            float step = progressRad / (float)segments;

            for (int i = 0; i < segments; i++) {
                float a1 = startAngle + (i * step);
                float a2 = startAngle + ((i + 1) * step);
                float t = (float)i / (float)segments;

                ImVec4 current = ImVec4(
                    colStart.x + (colEnd.x - colStart.x) * t,
                    colStart.y + (colEnd.y - colStart.y) * t,
                    colStart.z + (colEnd.z - colStart.z) * t,
                    1.0f * GlobalOpacity 
                );

                dl->PathLineTo(ImVec2(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius));
                dl->PathLineTo(ImVec2(center.x + cosf(a2) * radius, center.y + sinf(a2) * radius));
                dl->PathStroke(ImColor(current), 0, thickness);
            }
        }

        float t = std::max(0.0f, std::min(1.0f, safePercent / 100.0f));
        ImVec4 knobColor = ImVec4(
            colStart.x + (colEnd.x - colStart.x) * t,
            colStart.y + (colEnd.y - colStart.y) * t,
            colStart.z + (colEnd.z - colStart.z) * t,
            1.0f * GlobalOpacity 
        );

        ImVec2 knobPos = ImVec2(center.x + cosf(currentAngle) * radius, center.y + sinf(currentAngle) * radius);
        float kR = thickness * 1.5f;

        if (GlowTexture) {
            ImVec4 glowColAmb = knobColor;
            glowColAmb.w = 0.15f * GlobalOpacity;
            float sizeAmb = kR * 6.0f;
            dl->AddImage(GlowTexture,
                ImVec2(knobPos.x - sizeAmb, knobPos.y - sizeAmb),
                ImVec2(knobPos.x + sizeAmb, knobPos.y + sizeAmb),
                ImVec2(0, 0), ImVec2(1, 1), ImColor(glowColAmb));

            ImVec4 glowColCore = knobColor;
            glowColCore.w = 0.50f * GlobalOpacity; 
            float sizeCore = kR * 2.5f;
            dl->AddImage(GlowTexture,
                ImVec2(knobPos.x - sizeCore, knobPos.y - sizeCore),
                ImVec2(knobPos.x + sizeCore, knobPos.y + sizeCore),
                ImVec2(0, 0), ImVec2(1, 1), ImColor(glowColCore));
        }
        else {
            dl->AddCircleFilled(knobPos, kR * 3.0f, ImColor(knobColor.x, knobColor.y, knobColor.z, 0.2f * GlobalOpacity));
        }

        dl->AddCircleFilled(knobPos, kR, ImColor(knobColor));
        dl->AddCircleFilled(knobPos, kR * 0.4f, ImColor(1.0f, 1.0f, 1.0f, 0.8f * GlobalOpacity));

        ImFont* fontBig = ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetIO().Fonts->Fonts[0];
        ImFont* fontSmall = ImGui::GetIO().Fonts->Fonts[0];

        ImVec4 txtCol = Col_Text; txtCol.w *= GlobalOpacity;
        ImVec4 txtDim = Col_TextDim; txtDim.w *= GlobalOpacity;

        ImGui::PushFont(fontBig);
        ImVec2 txtSz = ImGui::CalcTextSize(valueStr);
        dl->AddText(ImVec2(center.x - txtSz.x / 2, center.y - txtSz.y / 2 - 4), ImColor(txtCol), valueStr);
        ImGui::PopFont();

        ImGui::PushFont(fontSmall);
        ImVec2 lblSz = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(center.x - lblSz.x / 2, center.y + txtSz.y / 2 - 2), ImColor(txtDim), label);
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(radius * 2, radius * 2));
    }

    static bool IconButton(const char* id, const char* icon, bool active) {
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(1, 1, 1, 1 * GlobalOpacity) : ImVec4(1, 1, 1, 0.3f * GlobalOpacity));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f * GlobalOpacity));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f * GlobalOpacity));

        bool clicked = ImGui::Button(id, ImVec2(24, 24));

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 rectMin = ImGui::GetItemRectMin();
        ImVec2 txtSz = ImGui::CalcTextSize(icon);

        ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
        dl->AddText(ImVec2(rectMin.x + (24 - txtSz.x) / 2, rectMin.y + (24 - txtSz.y) / 2), textCol, icon);

        ImGui::PopStyleColor(4);
        return clicked;
    }
};

void* Theme::GlowTexture = nullptr;
float Theme::GlobalOpacity = 1.0f;