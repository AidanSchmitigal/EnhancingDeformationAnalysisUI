#pragma once

#include <vector>
#include <cstdint>
#include <string>

class DenoiseInterface {
public:
	static bool Denoise(std::vector<uint32_t*>& images, int width, int height, const std::string& model_name, int kernel_size, float sigma);
private:
	static bool initialized;
};
