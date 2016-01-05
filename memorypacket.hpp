#ifndef __MPACKET_HPP_
#define __MPACKET_HPP_

class Processor;

class MemoryPacket {
private:
	const uint64_t requestSize;
	uint64_t fulfilSize;
	Processor *processorIndex;
	const uint64_t remoteAddress;
	const uint64_t localAddress;
	std::vector<uint8_t> payload;
	enum direction{OUT, IN} pd;

public:
	MemoryPacket(Processor *processor, const uint64_t& remoteAddr,
		const uint64_t& localAddr, const uint64_t& sz):
		processorIndex(processor), remoteAddress(remoteAddr),
		localAddress(localAddr), requestSize(sz), pd(OUT)
	{}

	void switchDirection()
	{
		if (pd == OUT) {
			pd = IN;
		}
	}

	const bool goingUp() const {
		return (pd == OUT);
	}

	void fillBuffer(const uint8_t byte);
	const uint64_t getRequestSize() const
	{ return requestSize; }
	const uint64_t getfulfilSize() const
	{ return fulfilSize; }
	const uint64_t getRemoteAddress() const
	{ return remoteAddress; }
	Processor* getProcessor() const
	{ return processorIndex; }
	const std::vector<uint8_t> getMemory() const { return payload; }
};

#endif

		 
