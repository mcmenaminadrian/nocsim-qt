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

#define WRITE_FACTOR 2


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
    delete gateMutex;
    gateMutex = nullptr;
    if (mmuMutex) {
        delete mmuMutex;
        mmuMutex = nullptr;
	delete acceptedMutex;
	acceptedMutex = nullptr;
    }
}

void Mux::initialiseMutex()
{
	bottomLeftMutex = new mutex();
	bottomRightMutex = new mutex();
    gateMutex = new mutex();
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
	MemoryPacket& packet)
{
	while (true) {
		packet.getProcessor()->waitGlobalTick();
		botMutex->lock();
		if (buffer == false) {
			buffer = true;
			botMutex->unlock();
			return;
		}
		botMutex->unlock();
		packet.getProcessor()->incrementBlocks();
	}
}

void Mux::routeDown(MemoryPacket& packet)
{
	// - this is the alternating implementation
	// are we left or right?
	bool packetOnLeft = false;
	bool *bufferToUnblock = nullptr;
	const uint64_t processorIndex = packet.getProcessor()->
		getTile()->getOrder();
	if (processorIndex < lowerRight.first) {
		packetOnLeft = true;
	}
	bool bothBuffers = false;
	while (true) {
		packet.getProcessor()->waitGlobalTick();
                acceptedMutex->lock();
                if (acceptedPackets < 4) {
	 	    bottomLeftMutex->lock();
		    bottomRightMutex->lock();
		    if (leftBuffer && rightBuffer) {
			bothBuffers = true;
		    } else {
			bothBuffers = false;
		    }
		    if (!bothBuffers) {
			if (packetOnLeft) {
        	    		bufferToUnblock = &leftBuffer;
				bottomRightMutex->unlock();
				bottomLeftMutex->unlock();
				gateMutex->lock();
                                gate = !gate;
				gateMutex->unlock();
				goto fillDDR;
			} else {
                		bufferToUnblock = &rightBuffer;
				bottomRightMutex->unlock();
				bottomLeftMutex->unlock();
				gateMutex->lock();
                                gate = !gate;
				gateMutex->unlock();
				goto fillDDR;
			}
		    }
		    else {
			gateMutex->lock();
			if (gate) {
				gateMutex->unlock();
				//prioritise right
				if (!packetOnLeft) {
					bufferToUnblock = &rightBuffer;
					bottomRightMutex->unlock();
					bottomLeftMutex->unlock();
					gateMutex->lock();
					gate = false;
					gateMutex->unlock();
					goto fillDDR;
				}
			} else {
				gateMutex->unlock();
				if (packetOnLeft) {
					bufferToUnblock = &leftBuffer;
					bottomRightMutex->unlock();
					bottomLeftMutex->unlock();
					gateMutex->lock();
					gate = true;
					gateMutex->unlock();
					goto fillDDR;
				}
			}
		}
		bottomRightMutex->unlock();
		bottomLeftMutex->unlock();
                }
                acceptedMutex->unlock();
		packet.getProcessor()->incrementBlocks();
	}

fillDDR:
        bottomLeftMutex->lock();
        bottomRightMutex->lock();
        *bufferToUnblock = false;
        bottomRightMutex->unlock();
        bottomLeftMutex->unlock();
	acceptedPackets++;
        acceptedMutex->unlock();
    uint64_t serviceDelay = MMU_DELAY;
    if (packet.getWrite()) {
        serviceDelay *= WRITE_FACTOR;
    }
    for (unsigned int i = 0; i < serviceDelay; i++) {
        packet.getProcessor()->incrementServiceTime();
        packet.getProcessor()->waitGlobalTick();
    }
    acceptedMutex->lock();
    acceptedPackets--;
    acceptedMutex->unlock();
    //cross to tree
	for (unsigned int i = 0; i < DDR_DELAY; i++) {
		packet.getProcessor()->waitGlobalTick();
	}
	//get memory
    if (packet.getRequestSize() > 0) {
        for (unsigned int i = 0; i < packet.getRequestSize(); i++) {
            packet.fillBuffer(packet.getProcessor()->
                getTile()->readByte(packet.getRemoteAddress() + i));
        }
    }
    return;
}	

void Mux::keepRoutingPacket(MemoryPacket& packet)
{
	if (upstreamMux == nullptr) {
		return routeDown(packet);
	} else {
		return postPacketUp(packet);
	}
}

void Mux::postPacketUp(MemoryPacket& packet)
{
	//one method here allows us to vary priorities between left and right
	//first step - what is the buffer we are targetting
	const uint64_t processorIndex = packet.getProcessor()->
		getTile()->getOrder();
	mutex *targetMutex = upstreamMux->bottomRightMutex;
	bool targetOnRight = true;
	if (processorIndex <= upstreamMux->lowerLeft.second) {
		targetOnRight = false;
		targetMutex = upstreamMux->bottomLeftMutex;
	}

	bool bothBuffers = false;
	while (true) {
		packet.getProcessor()->waitGlobalTick();
        	//in this implementation we alternate 
		bottomLeftMutex->lock();
		bottomRightMutex->lock();
        	if (leftBuffer && rightBuffer) {
            		bothBuffers = true;
        	} else {
			bothBuffers = false;
		}
        	if (!bothBuffers) {
            		//which are we, left or right?
			//only one buffer in use...so our packet has to be there
            		if (leftBuffer) {
                		targetMutex->lock();
                		if (targetOnRight && upstreamMux->rightBuffer == false)	
                		{
                    			leftBuffer = false;
                    			upstreamMux->rightBuffer = true;
                    			targetMutex->unlock();
                    			bottomRightMutex->unlock();
                    			bottomLeftMutex->unlock();
                    			gateMutex->lock();
                                        gate = !gate;
                    			gateMutex->unlock();
                    			return upstreamMux->keepRoutingPacket(packet);
                		}
                		else if (!targetOnRight && upstreamMux->leftBuffer == false)
                		{
                    			leftBuffer = false;
                    			upstreamMux->leftBuffer = true;
                    			targetMutex->unlock();
                    			bottomRightMutex->unlock();
                    			bottomLeftMutex->unlock();
                  	  		gateMutex->lock();
                    			gate = !gate;
                    			gateMutex->unlock();
                    			return upstreamMux->keepRoutingPacket(packet);
                		}
                		targetMutex->unlock();
            		} else {
                    		targetMutex->lock();
                   	 	if (targetOnRight && upstreamMux->rightBuffer == false)
                    		{
                        		rightBuffer = false;
                        		upstreamMux->rightBuffer = true;
         	       	        	targetMutex->unlock();
                	        	bottomRightMutex->unlock();
                        		bottomLeftMutex->unlock();
                        		gateMutex->lock();
                                        gate = !gate;
                        		gateMutex->unlock();
                        		return upstreamMux->keepRoutingPacket(packet);
                    		}
                    		else if (!targetOnRight && upstreamMux->leftBuffer == false)
                    		{
                        		rightBuffer = false;
                       			upstreamMux->leftBuffer = true;
                        		targetMutex->unlock();
                        		bottomRightMutex->unlock();
                        		bottomLeftMutex->unlock();
                        		gateMutex->lock();
                                        gate = !gate;
                        		gateMutex->unlock();
                        		return upstreamMux->keepRoutingPacket(packet);
                    		}
                    		targetMutex->unlock();
	    		}
		} else {
			//two packets here so which one are we?
			gateMutex->lock();
                	if (gate == true) {
                		gateMutex->unlock();
                    		//prioritise right
				if (processorIndex > lowerLeft.second) {
                    			targetMutex->lock();
                    			if (targetOnRight &&
                        			upstreamMux->rightBuffer == false)
                    			{
                        			rightBuffer = false;
                        			upstreamMux->rightBuffer = true;
                        			targetMutex->unlock();
        	                		bottomRightMutex->unlock();
                	        		bottomLeftMutex->unlock();
                        			gateMutex->lock();
                        			gate = false;
 		                       		gateMutex->unlock();
                	        		return upstreamMux->keepRoutingPacket(packet);
                	    		}
                    			else if (!targetOnRight &&
                        			upstreamMux->leftBuffer == false)
    	                		{
        	                		rightBuffer = false;
                	        		upstreamMux->leftBuffer = true;
                        			targetMutex->unlock();
                        			bottomRightMutex->unlock();
	                        		bottomLeftMutex->unlock();
        	               			gateMutex->lock();
                	        		gate = false;
                        			gateMutex->unlock();
                        			return upstreamMux->keepRoutingPacket(packet);
                    			}
                    			targetMutex->unlock();
				}
			} else {
        	            	gateMutex->unlock();
                	    	//target left - if we are on the left
				if (processorIndex < lowerRight.first) {
					targetMutex->lock();
		    			if (targetOnRight && upstreamMux->rightBuffer == false)
                    			{
 		                       		leftBuffer = false;
        	                		upstreamMux->rightBuffer = true;
                	        		targetMutex->unlock();
                        			bottomRightMutex->unlock();
                        			bottomLeftMutex->unlock();
 		                       		gateMutex->lock();
                	        		gate = !gate;
                        			gateMutex->unlock();
                        			return upstreamMux->keepRoutingPacket(packet);
                    			}
                    			else if (!targetOnRight &&
                        			upstreamMux->leftBuffer == false)
  	                  		{
        	                		leftBuffer = false;
                	        		upstreamMux->leftBuffer = true;
                        			targetMutex->unlock();
	                        		bottomRightMutex->unlock();
        	                		bottomLeftMutex->unlock();
                	        		gateMutex->lock();
                        			gate = !gate;
                       				gateMutex->unlock();
                        			return upstreamMux->keepRoutingPacket(packet);
                    			}
					targetMutex->unlock();
				}
                	}
		}
		bottomRightMutex->unlock();
		bottomLeftMutex->unlock();
		packet.getProcessor()->incrementBlocks();
	}
}

void Mux::routePacket(MemoryPacket& packet)
{
	const uint64_t processorIndex = packet.getProcessor()->
		getTile()->getOrder();
	if (processorIndex >= lowerLeft.first &&
		processorIndex <= lowerLeft.second) {
		fillBottomBuffer(leftBuffer, bottomLeftMutex,
			packet);
	} else {
		fillBottomBuffer(rightBuffer, bottomRightMutex,
			packet);
	}
	return postPacketUp(packet);
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

void Mux::addMMUMutex()
{
    mmuMutex = new mutex();
    acceptedMutex = new mutex();
    mmuLock =  unique_lock<mutex>(*mmuMutex);
    mmuLock.unlock();
}
