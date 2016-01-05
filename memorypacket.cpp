#include <cstdint>
#include <vector>
#include "memorypacket.hpp"

void MemoryPacket::fillBuffer(const uint8_t byte)
{
	payload.push_back(byte);
}
