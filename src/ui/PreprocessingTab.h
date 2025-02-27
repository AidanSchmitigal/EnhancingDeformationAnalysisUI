#pragma once

#include <vector>
#include <unordered_map>

#include <OpenGL/Texture.h>

class PreprocessingTab {
public:
    PreprocessingTab() = default;
    PreprocessingTab(std::vector<Texture*>& textures, std::vector<Texture*>& processed_textures);
    ~PreprocessingTab() {}
    void DisplayPreprocessingTab(bool& changed);
    void GetProcessedTextures(std::vector<Texture*>& processed_textures) { processed_textures = m_processed_textures; }

private:
    std::vector<Texture*> m_textures;
    std::vector<Texture*> m_processed_textures;
    std::unordered_map<int, int> m_selected_textures_map;
};