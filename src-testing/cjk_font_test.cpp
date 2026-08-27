#include "imgui/imgui.h"

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
}
