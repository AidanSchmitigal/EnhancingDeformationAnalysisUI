#include <vector>
#include <cstdint>

class Stabilizer {
public:
	static void stabilize(std::vector<uint32_t*>& frames, int width, int height);
};
