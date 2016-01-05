#include <iostream>
#include <map>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include "tree.hpp"
#include "memorypacket.hpp"
#include "mux.hpp"
#include "memory.hpp"

using namespace std;

Memory::Memory(const uint64_t& startAddress, const uint64_t& size):
	start(startAddress), memorySize(size)
{}

const uint8_t Memory::readByte(const uint64_t& address)
{
	uint8_t retVal = 0;

	if (address < start || address > start + memorySize) {
		cout << "Memory::readByte out of range" << endl;
		throw "Memory class range error";
	}

	try {
		retVal = contents.at(address);
	}
	catch (const out_of_range& err)
	{
		contents[address] = 0;
	}

	return retVal;
}

const uint64_t Memory::readLong(const uint64_t& address)
{
	uint64_t retVal = 0;

	if (address < start || address + sizeof(uint64_t) > start + memorySize)
	{
		cout << "Memory::readLong out of range" << endl;
		throw "Memory class range error";
	}

	uint8_t in[sizeof(uint64_t)];

	for (int i = 0; i < sizeof(uint64_t); i++)
	{	
		try {
			in[i] = (uint8_t)contents.at(address + i);
		}
		catch (const out_of_range& err)
		{
			contents[address] = 0;
		}
	}
	memcpy(&retVal, in, sizeof(uint64_t));
	return retVal;
}

void Memory::writeByte(const uint64_t& address, const uint8_t& value)
{
	if (address < start || address > start + memorySize) {
		cout << "Memory::writeByte out of range" << endl;
		throw "Memory class range error";
	}

	contents[address] = value;
}

void Memory::writeLong(const uint64_t& address, const uint64_t& value)
{
	if (address < start || address + sizeof(uint64_t) > start + memorySize)
	{
		cout << "Memory::writeLong out of range" << endl;
		throw "Memory class range error";
	}

	uint8_t *valRep = (uint8_t *) &value;
	for (int i = 0; i < sizeof(uint64_t); i++)
	{
		contents[address + i] = *(valRep + i);
	}
}

const uint32_t Memory::readWord32(const uint64_t& address)
{
	uint32_t result = 0;
	for (int i = 3; i >= 0; i--) {
		char nextByte = readByte(address + i) << (i * 8);
		result = result | nextByte;
	}
	return result;
}

void Memory::writeWord32(const uint64_t& address, const uint32_t& data)
{
	char mask = 0xFF;
	for (int i = 0; i < 4; i++) {
		char byteToWrite = (data >> (i * 8)) & mask;
		writeByte(address + i, byteToWrite);
	}
}

const uint64_t Memory::getSize() const
{
	return memorySize;
}

const bool Memory::inRange(const uint64_t& address) const
{
	return (address <= (start + memorySize - 1) && address >= start);
}

void Memory::attachTree(Mux* root)
{
	rootMux = root;
}
