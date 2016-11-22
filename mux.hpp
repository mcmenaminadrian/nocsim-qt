
#ifndef _MUX_CLASS_
#define _MUX_CLASS_

static const uint64_t DDR_DELAY = 30;

class Memory;

class Mux {
private:
	Memory* globalMemory;
	std::pair<uint64_t, uint64_t> lowerLeft;
	std::pair<uint64_t, uint64_t> lowerRight;
	bool leftBuffer;
	bool rightBuffer;
	std::mutex *bottomLeftMutex;
	std::mutex *bottomRightMutex;
	void disarmMutex();

public:
	Mux* upstreamMux;
	Mux* downstreamMuxLow;
	Mux* downstreamMuxHigh;
	Mux():  leftBuffer(false), rightBuffer(false), 
	        bottomLeftMutex(nullptr), bottomRightMutex(nullptr),
	        upstreamMux(nullptr), downstreamMuxLow(nullptr),
		downstreamMuxHigh(nullptr) {};
	Mux(Memory *gMem): globalMemory(gMem) {};
	~Mux();
	void initialiseMutex();
	void fillBottomBuffer(bool& buffer,
		std::mutex *botMutex, MemoryPacket& packet);
	void routeDown(MemoryPacket& packet);
	void assignGlobalMemory(Memory *gMem){ globalMemory = gMem; }
	void joinUpMux(const Mux& left, const Mux& right);
	void assignNumbers(const uint64_t& ll, const uint64_t& ul,
		const uint64_t& lr, const uint64_t& ur);
	const std::tuple<const uint64_t, const uint64_t,
		const uint64_t, const uint64_t> fetchNumbers() const;
	void routePacket(MemoryPacket& pack);
    	bool acceptPacketUp(const MemoryPacket& mPack) const;
	void postPacketUp(MemoryPacket& packet);
	void keepRoutingPacket(MemoryPacket& packet);

};	
#endif
