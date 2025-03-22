#include <vector>
#include <cstdint>

class Stabilizer {
public:
	static void stabilize_old(std::vector<uint32_t*>& frames, int width, int height);
	static void Stabilize(std::vector<uint32_t*>& frames, int width, int height);
};
