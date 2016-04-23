#include <iostream>
#include <map>
#include <vector>
#include <utility>
#include <tuple>
#include <bitset>
#include <mutex>
#include <condition_variable>
#include "mainwindow.h"
#include "memorypacket.hpp"
#include "memory.hpp"
#include "ControlThread.hpp"
#include "tile.hpp"
#include "processor.hpp"
#include "mux.hpp"




using namespace std;

Mux::~Mux()
{
	disarmMutex();
}

void Mux::disarmMutex()
{
	delete bottomLeftMutex;
	bottomLeftMutex = nullptr;
	delete bottomRightMutex;
	bottomRightMutex = nullptr;
	delete topMutex;
	topMutex = nullptr;
}

void Mux::initialiseMutex()
{
	bottomLeftMutex = new mutex();
	bottomRightMutex = new mutex();
	topMutex = new mutex();
}

bool Mux::acceptPacketUp(const MemoryPacket& mPack) const
{
	if (!mPack.goingUp()) {
		cerr << "Routing memory packet in wrong direction" << endl;
		return false;
	}
	if (!globalMemory) {
		cerr << "Mux has no global memory assigned" << endl;
		return false;
	}
	return (globalMemory->inRange(mPack.getRemoteAddress()));
}

const tuple<const uint64_t, const uint64_t, const uint64_t,
	const uint64_t> Mux::fetchNumbers() const
{
	return make_tuple(get<0>(lowerLeft), get<1>(lowerLeft),
		get<0>(lowerRight), get<1>(lowerRight));
}

void Mux::fillBottomBuffer(bool& buffer, mutex *botMutex,
	Mux* muxBelow, MemoryPacket& packet)
{
	while (true) {
		packet.getProcessor()->waitGlobalTick();
		botMutex->lock();
		if (muxBelow) {
			muxBelow->topMutex->lock();
		}
		if (buffer == false) {
			if (muxBelow) {
				muxBelow->topBuffer = false;
				muxBelow->topMutex->unlock();
			}
			buffer = true;
			botMutex->unlock();
			return;
		}
		if (muxBelow) {
			muxBelow->topMutex->unlock();
		}
		botMutex->unlock();
        packet.getProcessor()->incrementBlocks();
	}
}

void Mux::routeDown(MemoryPacket& packet)
{
	//delay 1 tick
	packet.getProcessor()->waitGlobalTick();
	//release buffer
	topMutex->lock();
	topBuffer = false;
	topMutex->unlock();
	//cross to DDR
	for (unsigned int i = 0; i < DDR_DELAY; i++) {
		packet.getProcessor()->waitGlobalTick();
	}
	//get memory
	for (unsigned int i = 0; i < packet.getRequestSize(); i++) {
		packet.fillBuffer(packet.getProcessor()->
			getTile()->readByte(packet.getRemoteAddress() + i));
	}
	return;
}	


void Mux::fillTopBuffer(
	bool& bottomBuffer, mutex *botMutex,
	MemoryPacket& packet)
{
	while (true) {
		packet.getProcessor()->waitGlobalTick();
		topMutex->lock();
		if (topBuffer == false) {
			botMutex->lock();
			bottomBuffer = false;
			botMutex->unlock();
			topBuffer = true;
			topMutex->unlock();
			//if we are top layer, then route into memory
			if (upstreamMux == nullptr) {
				return routeDown(packet);
			} else {
				return upstreamMux->routePacket(packet);
			}
		} else {
			topMutex->unlock();
		}
        packet.getProcessor()->incrementBlocks();
	}
}				

void Mux::routePacket(MemoryPacket& packet)
{
	//is the buffer free?
	const uint64_t processorIndex = packet.getProcessor()->
		getTile()->getOrder();
	if (processorIndex >= lowerLeft.first &&
		processorIndex <= lowerLeft.second) {
		fillBottomBuffer(leftBuffer, bottomLeftMutex, downstreamMuxLow,
			packet);
		return fillTopBuffer(leftBuffer, bottomLeftMutex,
			packet);
	} else {
		fillBottomBuffer(rightBuffer, bottomRightMutex,
			downstreamMuxHigh, packet);
		return fillTopBuffer(rightBuffer, bottomRightMutex, packet);
	}
}

void Mux::joinUpMux(const Mux& left, const Mux& right)
{
	assignNumbers(left.lowerLeft.first, left.lowerRight.second,
		right.lowerLeft.first, right.lowerRight.second);
}

void Mux::assignNumbers(const uint64_t& ll, const uint64_t& ul,
	const uint64_t& lr, const uint64_t& ur)
{
	lowerLeft = pair<uint64_t, uint64_t>(ll, ul);
	lowerRight = pair<uint64_t, uint64_t>(lr, ur);
}
