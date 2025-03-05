#pragma once

#include <vector>
#include <cstdint>

class ImageAnalysis {
public:
    static void AnalyzeImages(std::vector<uint32_t*>& frames, int width, int height, std::vector<std::vector<float>>& histograms, std::vector<float>& avg_histogram, std::vector<float>& snrs, float& avg_snr);
};