
#ifndef _MUX_CLASS_
#define _MUX_CLASS_

static const uint64_t DDR_DELAY = 30;

class Memory;

class Mux {
private:
	Memory* globalMemory;
	std::pair<uint64_t, uint64_t> lowerLeft;
	std::pair<uint64_t, uint64_t> lowerRight;
	bool topBuffer;
	bool leftBuffer;
	bool rightBuffer;
	std::mutex *bottomLeftMutex;
	std::mutex *bottomRightMutex;
	std::mutex *topMutex;
	void disarmMutex();

public:
	Mux* upstreamMux;
	Mux* downstreamMuxLow;
	Mux* downstreamMuxHigh;
	Mux():upstreamMux(nullptr), downstreamMuxLow(nullptr),
		downstreamMuxHigh(nullptr), bottomLeftMutex(nullptr),
		bottomRightMutex(nullptr), topMutex(nullptr), topBuffer(false),
		leftBuffer(false), rightBuffer(false) {};
	Mux(Memory *gMem): globalMemory(gMem) {};
	~Mux();
	void initialiseMutex();
	void fillBottomBuffer(bool& buffer,
		std::mutex *botMutex, Mux* muxBelow, MemoryPacket& packet);
	void routeDown(MemoryPacket& packet);
	void fillTopBuffer(bool& bottomBuffer,
		std::mutex *botMutex, MemoryPacket& packet);
	void assignGlobalMemory(Memory *gMem){ globalMemory = gMem; }
	void joinUpMux(const Mux& left, const Mux& right);
	void assignNumbers(const uint64_t& ll, const uint64_t& ul,
		const uint64_t& lr, const uint64_t& ur);
	const std::tuple<const uint64_t, const uint64_t,
		const uint64_t, const uint64_t> fetchNumbers() const;
	void routePacket(MemoryPacket& pack);
	const bool acceptPacketUp(const MemoryPacket& mPack) const;
};	
#endif
