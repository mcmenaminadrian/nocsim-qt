#include <QObject>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <bitset>
#include <mutex>
#include <tuple>
#include <condition_variable>
#include <climits>
#include <cstdlib>
#include "mainwindow.h"
#include "ControlThread.hpp"
#include "memorypacket.hpp"
#include "mux.hpp"
#include "tile.hpp"
#include "memory.hpp"


#ifndef _PROCESSOR_CLASS_
#define _PROCESSOR_CLASS_

//Page table entries - physical addr, virtual addr, frame no, flags

#define PAGETABLEENTRY (8 + 8 + 8 + 4)
#define VOFFSET 0
#define POFFSET 8
#define FRAMEOFFSET 16
#define FLAGOFFSET 24
#define ENDOFFSET 28

static const uint64_t REGISTER_FILE_SIZE = 32;
static const uint64_t BITMAP_BYTES = 16;
static const uint64_t BITMAP_SHIFT = 4;
static const uint64_t BITMAP_MASK = 0xFFFFFFFFFFFFFFF0;
//page mappings
static const uint64_t PAGETABLESLOCAL = 0xA000000000000000;
static const uint64_t GLOBALCLOCKSLOW = 1;
static const uint64_t TOTAL_LOCAL_PAGES = TILE_MEM_SIZE >> PAGE_SHIFT;
static const uint64_t BITS_PER_BYTE = 8;

class Tile;

class Processor: public QObject {
    Q_OBJECT

signals:
    void hardFault();
    void smallFault();

private:
	std::mutex interruptLock;
	std::mutex waitMutex;
	std::vector<uint64_t> registerFile;
	std::vector<std::tuple<uint64_t, uint64_t, bool>> tlbs;
	bool carryBit;
	uint64_t programCounter;
	Tile *masterTile;
	enum ProcessorMode { REAL, VIRTUAL };
	ProcessorMode mode;
	Memory *localMemory;
	MainWindow *mainWindow;
	long pageShift;
	uint64_t stackPointer;
	uint64_t stackPointerOver;
	uint64_t stackPointerUnder;
	uint64_t pageMask;
	uint64_t bitMask;
	uint64_t memoryAvailable;
	uint64_t pagesAvailable;
	uint64_t processorNumber;
	uint64_t randomPage;
	bool inInterrupt;
	bool inClock;
	bool clockDue;
	void markUpBasicPageEntries(const uint64_t& reqPTEPages,
	const uint64_t& reqBitmapPages);
	void writeOutBasicPageEntries(const uint64_t& reqPTEPages);
	void writeOutPageAndBitmapLengths(const uint64_t& reqPTESize,
	const uint64_t& reqBitmapPages);
	void zeroOutTLBs(const uint64_t& reqPTEPages);
	uint64_t fetchAddressRead(const uint64_t& address,
        const bool& readOnly = false, const bool& write = false);
    	uint64_t fetchAddressWrite(const uint64_t& address);
	bool isBitmapValid(const uint64_t& address,
	const uint64_t& physAddress) const;
	uint64_t generateAddress(const uint64_t& frame,
		const uint64_t& address);
    	uint64_t triggerSmallFault(
        const std::tuple<uint64_t, uint64_t, bool>& tlbEntry,
        const uint64_t& address, const bool& write);
	void interruptBegin();
	void interruptEnd();
	void transferGlobalToLocal(const uint64_t& address,
	const std::tuple<uint64_t, uint64_t, bool>& tlbEntry,
    	const uint64_t& size, const bool& write);
    	uint64_t triggerHardFault(const uint64_t& address, const bool& readOnly,
        	const bool& write);
	const std::pair<const uint64_t, bool> getRandomFrame();
	const std::pair<const uint64_t, bool> getFreeFrame();
	void loadMemory(const uint64_t& frameNo,
	const uint64_t& address);
	void fixPageMap(const uint64_t& frameNo,
        const uint64_t& address, const bool& readOnly);
	void fixPageMapStart(const uint64_t& frameNo,
	const uint64_t& address);
	void fixBitmap(const uint64_t& frameNo);
	void markBitmapStart(const uint64_t& frameNo,
	const uint64_t& address);
	void markBitmapInit(const uint64_t& frameNo,
        const uint64_t& address);
	void markBitmap(const uint64_t& frameNo,
        const uint64_t& address);
	void fixTLB(const uint64_t& frameNo,
	const uint64_t& address);
	const std::vector<uint8_t>
		requestRemoteMemory(
		const uint64_t& size, const uint64_t& remoteAddress,
        const uint64_t& localAddress, const bool& write);
    	const std::pair<uint64_t, uint8_t>
        mapToGlobalAddress(const uint64_t& address);
    void fetchAddressToRegister();
	void activateClock();
	//adjust numbers below to change how CLOCK fuctions
    const uint8_t clockWipe = 1;
    const uint16_t clockTicks = 1000;
	uint64_t totalTicks;
	uint64_t currentTLB;

public:
	std::bitset<16> statusWord;
    	Processor(Tile* parent, MainWindow *mW, uint64_t numb);
	void loadMem(const long regNo, const uint64_t memAddr);
	void switchModeReal();
	void switchModeVirtual();
	void setMode();
	void createMemoryMap(Memory *local, long pShift);
	void setPCNull();
	void start();
	void pcAdvance(const long count = sizeof(long));
    	uint64_t getRegister(const uint64_t& regNumber) const;
    	void setRegister(const uint64_t& regNumber,
        	const uint64_t& value);
	uint8_t getAddress(const uint64_t& address);
    	uint64_t multiplyWithCarry(const uint64_t& A,
        	const uint64_t& B);
    	uint64_t subtractWithCarry(const uint64_t& A,
        	const uint64_t& B);
	uint64_t getLongAddress(const uint64_t& address);
	void writeAddress(const uint64_t& addr,
		const uint64_t& value);
    void writeAddress64(const uint64_t& addr);
    void writeAddress32(const uint64_t& addr);
    void writeAddress16(const uint64_t& addr);
    void writeAddress8(const uint64_t& addr);
	void pushStackPointer();
	void popStackPointer();
    	uint64_t getStackPointer() const;
	void setStackPointer(const uint64_t& address) { 
		stackPointer = address; }
    	uint64_t getProgramCounter() const {
        	return programCounter;
    	}
    	void setProgramCounter(const uint64_t& address) {
        	programCounter = address;
        	fetchAddressRead(address);
    	}
    	void checkCarryBit();
    	void writeBackMemory(const uint64_t& frameNo);
    	void transferLocalToGlobal(const uint64_t& address,
        const std::tuple<uint64_t, uint64_t, bool>& tlbEntry,
       		const uint64_t& size);
	void waitATick();
	void waitGlobalTick();
	Tile* getTile() const { return masterTile; }
   	uint64_t getNumber() { return processorNumber; }
   	void flushPagesStart();
    	void flushPagesEnd();
    	void dropPage(const uint64_t& frameNo);
    	void dumpPageFromTLB(const uint64_t& address);
    	const uint64_t& getTicks() const { return totalTicks; }
	void incrementBlocks();
        void incrementServiceTime();
	bool tryCheatLock() const;
	void cheatUnlock() const;
        void resetCounters();
    uint64_t hardFaultCount;
    uint64_t smallFaultCount;
    uint64_t blocks;
    uint64_t serviceTime;
};
#endif
