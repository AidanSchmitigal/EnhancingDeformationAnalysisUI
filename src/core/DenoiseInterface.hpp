#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <future>

class DenoiseInterface {
public:
	// Synchronous methods
	static bool Denoise(std::vector<uint32_t*>& images, int width, int height, const std::string& model_name, const int tile_size, const int overlap);
	static bool Blur(std::vector<uint32_t*>& images, int width, int height, int kernel_size, float sigma);
	
	// Asynchronous methods with callback
	static std::future<bool> DenoiseAsync(std::vector<uint32_t*>& images, int width, int height, const std::string& model_name, 
		const int tile_size, const int overlap, std::function<void(bool)> callback = nullptr);
	static std::future<bool> BlurAsync(std::vector<uint32_t*>& images, int width, int height, int kernel_size, 
		float sigma, std::function<void(bool)> callback = nullptr);
	
	// Status checking
	static bool IsProcessing();
	static float GetProgress();

private:
	static float m_progress;
	static bool m_is_processing;
};
