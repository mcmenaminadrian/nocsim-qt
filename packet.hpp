#ifndef __MPACKET_HPP_
#define __MPACKET_HPP_

class MemoryPacket {
private:
	const uint64_t requestSize;
	uint64_t fulfilSize;
	const uint64_t processorIndex;
	const uint64_t remoteAddress;
	const uint64_t localAddress;
	std::vector<uint8_t> payload
	enum direction{OUT, IN} pd;

public:
	MemoryPacket(const uint64_t& processor, const uint64_t& remoteAddr,
		const uint64_t& localAddr, const uint64_t sz):
		processorIndex(processor), remoteAddress(remoteAddr),
		localAddress(localAddr), requestSize(sz), pd(OUT)
	{}

	void switchDirection()
	{
		if (pd == OUT) {
			pd = IN;
		}
	}

	void fillBuffer();
	const uint64_t getRequestSize() const
	{ return requestSize; }
	const uint64_t getfulfilSize() const
	{ return fulfilSize; }
};

#endif

		 
