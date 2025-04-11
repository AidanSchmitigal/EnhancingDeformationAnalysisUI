#pragma once

#include <vector>
#include <cstdint>
#include <future>
#include <functional>

class Stabilizer {
public:
	static bool Stabilize(std::vector<uint32_t*>& frames, int width, int height);
	static std::future<bool> StabilizeAsync(std::vector<uint32_t*>& frames, int width, int height, std::function<void(bool)> callback = nullptr);

	static bool IsProcessing() { return m_is_processing; }
	static float GetProgress() { return m_progress; }
private:
	static float m_progress;
	static bool m_is_processing;
};
