//Memory class
#include <QMap>
#ifndef _MEMORY_CLASS_
#define _MEMORY_CLASS_

const uint64_t PAGE_SHIFT = 10;

class Mux;

class Memory {

private:
	const uint64_t start;
	const uint64_t memorySize;
    QMap<uint64_t, uint8_t> contents;
	Mux* rootMux;

public:
	Memory(const uint64_t& start, const uint64_t& size);
    uint8_t readByte(const uint64_t& address);
    uint64_t readLong(const uint64_t& address);
    uint32_t readWord32(const uint64_t& address);
	void writeWord32(const uint64_t& address, const uint32_t& value);
	void writeByte(const uint64_t& address, const uint8_t& value);
	void writeLong(const uint64_t& address, const uint64_t& value);
	void attachTree(Mux* root);
    uint64_t getSize() const;
    bool inRange(const uint64_t& address) const;
};

#endif
