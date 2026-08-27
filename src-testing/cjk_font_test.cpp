#include "imgui/imgui.h"
#include "utils/imgui_context_wrapper.h"

#include <cassert>

int main()
{
    ImFontAtlas atlas;
    ImFont *base_font = atlas.AddFontDefault();
    assert(base_font != nullptr);

    ImFontConfig config;
    config.MergeMode = true;
    ImFont *merged_font = atlas.AddFontFromFileTTF(
        "resources/fonts/NotoSansSC-Regular.otf", 18.0f, &config,
        atlas.GetGlyphRangesChineseSimplifiedCommon());
    assert(merged_font == base_font);
    assert(atlas.Build());

    assert(base_font->IsGlyphInFont(0x8BBE));
    assert(base_font->IsGlyphInFont(0x7F6E));

    ImGuiContext *context = ImGui::CreateContext();
    ImGui::GetIO().BackendFlags &= ~ImGuiBackendFlags_RendererHasTextures;
    ContainedContext contained;
    contained.setFontDensity();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800.0f, 600.0f);
    io.DeltaTime = 1.0f / 60.0f;
    assert(io.Fonts->Build());
    ImGui::GetStyle().FontSizeBase = 16.0f;
    ImGui::GetStyle().FontScaleMain = 0.5f;
    ImGui::NewFrame();
    assert(ImGui::GetFontSize() == 8.0f);
    ImGui::EndFrame();
    ImGui::DestroyContext(context);
}
