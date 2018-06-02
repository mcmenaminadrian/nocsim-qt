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
#include "processor.hpp"

//page table flags
//bit 0 - 0 for invalid entry, 1 for valid
//bit 1 - 0 for moveable, 1 for fixed
//bit 2 - 0 for CLOCKed out, 1 for CLOCKed in
//bit 3 - 0 for read/write, 1 for read only

//TLB model
//first entry - virtual address 
//second entry - physical address
//third entry - bool for validity

//statusWord
//Bit 0 :   true = REAL, false = VIRTUAL
//Bit 1 :   CarryBit


const static uint64_t KERNELPAGES = 2;
const static uint64_t STACKPAGES = 2;
const static uint64_t BITMAPDELAY = 0;
const static uint64_t FREEPAGES = 27;
const static uint64_t BASEPAGES = 4; 

using namespace std;

Processor::Processor(Tile *parent, MainWindow *mW, uint64_t numb):
    masterTile(parent), mode(REAL), mainWindow(mW)
{
	registerFile = vector<uint64_t>(REGISTER_FILE_SIZE, 0);
	statusWord[0] = true;
	totalTicks = 1;
	currentTLB = 0;
	randomPage = -1;
	hardFaultCount = 0;
	smallFaultCount = 0;
	blocks = 0;
        randomPage = 7;
	inInterrupt = false;
	processorNumber = numb;
	clockDue = false;
	QObject::connect(this, SIGNAL(hardFault()),
        	mW, SLOT(updateHardFaults()));
	QObject::connect(this, SIGNAL(smallFault()),
        	mW, SLOT(updateSmallFaults()));
}

void Processor::resetCounters()
{
	hardFaultCount = 0;
	smallFaultCount = 0;
	blocks = 0;
	serviceTime = 0;
}

void Processor::setMode()
{
	if (!statusWord[0]) {
		mode = REAL;
		statusWord[0] = true;
	} else {
		mode = VIRTUAL;
		statusWord[0] = false;
	}
}

void Processor::switchModeReal()
{
	if (!statusWord[0]) {
		mode = REAL;
		statusWord[0] = true;
	}
}

void Processor::switchModeVirtual()
{
	if (statusWord[0]) {
		mode = VIRTUAL;
		statusWord[0] = false;
	}
}

void Processor::zeroOutTLBs(const uint64_t& frames)
{
	for (unsigned int i = 0; i < frames; i++) {
		tlbs.push_back(tuple<uint64_t, uint64_t, bool>
			(PAGESLOCAL + (1 << pageShift) * i,
			 PAGESLOCAL + (1 << pageShift) * i, false));
	}
}

void Processor::writeOutPageAndBitmapLengths(const uint64_t& reqPTEPages)
{
	masterTile->writeLong(PAGESLOCAL, reqPTEPages);
}

void Processor::writeOutBasicPageEntries(const uint64_t& pagesAvailable)
{
	const uint64_t tablesOffset = KERNELPAGES * (1 << pageShift) +
		PAGESLOCAL;
    for (unsigned int i = 0; i < pagesAvailable; i++) {
        long memoryLocalOffset = i * PAGETABLEENTRY + tablesOffset;
        masterTile->writeLong(
            memoryLocalOffset + VOFFSET,
                    i * (1 << pageShift) + PAGESLOCAL);
        masterTile->writeLong(
            memoryLocalOffset + POFFSET,
                    i * (1 << pageShift) + PAGESLOCAL);
		masterTile->writeLong(
            memoryLocalOffset + FRAMEOFFSET, i);
		masterTile->writeWord32(
			memoryLocalOffset + FLAGOFFSET, 0);
	}
}

void Processor::markUpBasicPageEntries(const uint64_t& reqPTEPages)
{
	//mark for page tables, and 2 notional pages for kernel
	for (unsigned int i = 0; i <= reqPTEPages + KERNELPAGES; i++) {
		const uint64_t pageEntryBase = (1 << pageShift) * KERNELPAGES  +
			i * PAGETABLEENTRY + PAGESLOCAL;
		const uint64_t mappingAddress = PAGESLOCAL +
			i * (1 << pageShift);
        	masterTile->writeLong(pageEntryBase + VOFFSET,
 			mappingAddress);
        	masterTile->writeLong(pageEntryBase + POFFSET,
            		mappingAddress);
		masterTile->writeWord32(pageEntryBase + FLAGOFFSET, 0x07);
	}
	//stack
    	uint64_t stackFrame = (TILE_MEM_SIZE >> pageShift) - 1;
	uint64_t stackInTable = (1 << pageShift) * KERNELPAGES + 
        	stackFrame * PAGETABLEENTRY + PAGESLOCAL;
	for (unsigned int i = 0; i < STACKPAGES; i++) {
    		masterTile->writeLong(stackInTable + VOFFSET,
        		stackFrame * (1 << pageShift) + PAGESLOCAL);
    		masterTile->writeLong(stackInTable + POFFSET,
        		stackFrame * (1 << pageShift) + PAGESLOCAL);
		masterTile->writeWord32(stackInTable + FLAGOFFSET, 0x07);
		stackInTable -= PAGETABLEENTRY;
		stackFrame--;
	}
}

void Processor::flushPagesStart()
{
    interruptBegin();
}

void Processor::flushPagesEnd()
{
    interruptEnd();
}

void Processor::createMemoryMap(Memory *local, long pShift)
{
	localMemory = local;
	pageShift = pShift;
	memoryAvailable = localMemory->getSize();
	pagesAvailable = memoryAvailable >> pageShift;
	uint64_t requiredPTESize = pagesAvailable * PAGETABLEENTRY;
    uint64_t requiredPTEPages = requiredPTESize >> pageShift;
	if ((requiredPTEPages << pageShift) != requiredPTESize) {
		requiredPTEPages++;
	}

	stackPointer = TILE_MEM_SIZE + PAGESLOCAL;
    	stackPointerUnder = stackPointer;
    	stackPointerOver = stackPointer - (STACKPAGES << pageShift);

	zeroOutTLBs(pagesAvailable);

	writeOutPageAndBitmapLengths(requiredPTEPages);
	writeOutBasicPageEntries(pagesAvailable);
	markUpBasicPageEntries(requiredPTEPages);
	pageMask = 0xFFFFFFFFFFFFFFFF;
	pageMask = pageMask >> pageShift;
	pageMask = pageMask << pageShift;
	bitMask = ~ pageMask;
	uint64_t pageCount = requiredPTEPages + KERNELPAGES;
	for (unsigned int i = 0; i <= pageCount; i++) {
		const uint64_t pageStart =
			PAGESLOCAL + i * (1 << pageShift);
		fixTLB(i, pageStart);
	}
    	//TLB and bitmap for stack
    	uint64_t stackPage = PAGESLOCAL + TILE_MEM_SIZE;
    	uint64_t stackPageNumber = pagesAvailable;
	for (unsigned int i = 0; i < STACKPAGES; i++) {
		stackPageNumber--;
		stackPage -= (1 << pageShift);
    		fixTLB(stackPageNumber, stackPage);
	}
}

uint64_t Processor::generateAddress(const uint64_t& frame,
	const uint64_t& address) const
{
	uint64_t offset = address & bitMask;
    return (frame << pageShift) + offset + PAGESLOCAL;
}

void Processor::interruptBegin()
{
	interruptLock.lock();
	inInterrupt = true;
	switchModeReal();
	for (auto i: registerFile) {
		waitATick();
		pushStackPointer();	
		waitATick();
		masterTile->writeLong(stackPointer, i);
	}
}

void Processor::interruptEnd()
{
	for (int i = registerFile.size() - 1; i >= 0; i--) {
		waitATick();
		registerFile[i] = masterTile->readLong(stackPointer);
		waitATick();
		popStackPointer();
	}
	switchModeVirtual();
	inInterrupt = false;
	interruptLock.unlock();
}

// Maximum flit size 128 bits
// Maximum packet size 5 flits

//tuple - vector of bytes, size of vector, success

const vector<uint8_t> Processor::requestRemoteMemory(
	const uint64_t& size, const uint64_t& remoteAddress,
	const uint64_t& localAddress, const bool& write)
{
	//assemble request
	MemoryPacket memoryRequest(this, remoteAddress,
		localAddress, size);
    	if (write) {
        	memoryRequest.setWrite();
    	}
	//wait for response
	masterTile->setPowerStateOff();
	if (masterTile->treeLeaf->acceptPacketUp(memoryRequest)) {
        	masterTile->treeLeaf->routePacket(memoryRequest);
	} else {
		cerr << "FAILED" << endl;
		exit(1);
	}
	return memoryRequest.getMemory();
}

void Processor::transferGlobalToLocal(const uint64_t& address,
	const tuple<uint64_t, uint64_t, bool>& tlbEntry,
    const uint64_t& size, const bool& write)
{
	//mimic a DMA call - so need to advance PC
	uint64_t maskedAddress = address & BITMAP_MASK;
	int offset = 0;
	vector<uint8_t> answer = requestRemoteMemory(size,
		maskedAddress, get<1>(tlbEntry) +
        	(maskedAddress & bitMask), write);
	masterTile->setPowerStateOn();
	for (auto x: answer) {
		masterTile->writeByte(get<1>(tlbEntry) + offset + 
			(maskedAddress & bitMask), x);
		offset++;
	}
}

void Processor::transferLocalToGlobal(const uint64_t& address,
	const tuple<uint64_t, uint64_t, bool>& tlbEntry,
	const uint64_t& size)
{
    	//again - this is like a DMA call, there is a delay, but no need
    	//to advance the PC
    	uint64_t maskedAddress = address & BITMAP_MASK;
    	//make the call - ignore the results
    	requestRemoteMemory(size, get<0>(tlbEntry), maskedAddress, true);
	masterTile->setPowerStateOn();
}


const pair<const uint64_t, bool> Processor::getRandomFrame()
{
	waitATick();
	//See 3.2.1 of Knuth (third edition)
	//simple ramdom number generator
	//pick pages 2 - 14 (0 - 12)
        //old rand had factor of 3, better has factor of 1
	randomPage = (randomPage + 1)%FREEPAGES;
	waitATick(); //store
	return pair<const uint64_t, bool>(randomPage + BASEPAGES, true);
}
	

//nominate a frame to be used
const pair<const uint64_t, bool> Processor::getFreeFrame()
{
	//have we any empty frames?
	//we assume this to be subcycle
	uint64_t frames = (localMemory->getSize()) >> pageShift;
	uint64_t couldBe = 0xFFFF;
	for (uint64_t i = 0; i < frames; i++) {
		uint32_t flags = masterTile->readWord32((1 << pageShift) * KERNELPAGES
			+ i * PAGETABLEENTRY + FLAGOFFSET + PAGESLOCAL);
        	if (!(flags & 0x01)) {
            		return pair<const uint64_t, bool>(i, false);
        	}
        	if (flags & 0x02) {
			continue;
		}
        	else if (!(flags & 0x04)) {
			couldBe = i;
		}
	}
	if (couldBe < 0xFFFF) {
		return pair<const uint64_t, bool>(couldBe, true);
	}
	//no free frames, so we have to pick one
	return getRandomFrame();
}

//drop page from TLBs and page tables - no write back
void Processor::dropPage(const uint64_t& frameNo)
{
    	waitATick();
    	//firstly get the address
    	const uint64_t pageAddress = masterTile->readLong(
        	frameNo * PAGETABLEENTRY + PAGESLOCAL + VOFFSET +
        	(1 << pageShift) * KERNELPAGES);
    	dumpPageFromTLB(pageAddress);
    	//mark as invalid in page table
    	waitATick();
    	masterTile->writeWord32(frameNo * PAGETABLEENTRY + PAGESLOCAL +
        	FLAGOFFSET + (1 << pageShift) * KERNELPAGES, 0);
}

//only used to dump a frame
void Processor::writeBackMemory(const uint64_t& frameNo)
{
	//is this a read-only frame?
    	if (localMemory->readWord32((
		1 << pageShift) * KERNELPAGES + frameNo * PAGETABLEENTRY
        	+ FLAGOFFSET) & 0x08) {
        	return;
    	}
    	const uint64_t physicalAddress = mapToGlobalAddress(
        	localMemory->readLong((1 << pageShift) * KERNELPAGES +
        	frameNo * PAGETABLEENTRY)).first;
    	for (unsigned int i = 0; i < (1 << pageShift); i+= 16)
    	{
            	//simulate transfer
        	transferLocalToGlobal(
			frameNo * (1 << pageShift) 
			+ PAGESLOCAL + i,
			 tlbs[frameNo], 0x10);
            	for (unsigned int j = 0;
                	    j < (16 / sizeof(uint64_t)); j++)
            	{
                	//actual transfer done in here
                	waitATick();
                	uint64_t toGo = masterTile->readLong(
                    			fetchAddressRead(frameNo * 
					(1 << pageShift) +
                    			PAGESLOCAL + 
					i +
                    			j * sizeof(uint64_t)));
                	masterTile->writeLong(fetchAddressWrite(
                    		physicalAddress + i 
                    		+ j * sizeof(uint64_t)), toGo);
            	}
        }
}

void Processor::fixPageMap(const uint64_t& frameNo,
    const uint64_t& address, const bool& readOnly)
{
	const uint64_t pageAddress = address & pageMask;
    	const uint64_t writeBase =
        	(1 << pageShift) + frameNo * PAGETABLEENTRY;
	waitATick();
	localMemory->writeLong(writeBase + VOFFSET, pageAddress);
	waitATick();
    	if (readOnly) {
        	localMemory->writeWord32(writeBase + FLAGOFFSET, 0x0D);
    	} else {
        	localMemory->writeWord32(writeBase + FLAGOFFSET, 0x05);
    	}
}

//write in initial page of code
void Processor::fixPageMapStart(const uint64_t& frameNo,
	const uint64_t& address) 
{
	const uint64_t pageAddress = address & pageMask;
	localMemory->writeLong((1 << pageShift) * KERNELPAGES +
            frameNo * PAGETABLEENTRY + VOFFSET, pageAddress);
	localMemory->writeWord32((1 << pageShift) * KERNELPAGES +
        frameNo * PAGETABLEENTRY + FLAGOFFSET, 0x0D);
}

void Processor::fixTLB(const uint64_t& frameNo,
	const uint64_t& address)
{
	const uint64_t pageAddress = address & pageMask;
	get<1>(tlbs[frameNo]) = frameNo * (1 << pageShift) + PAGESLOCAL;
	get<0>(tlbs[frameNo]) = pageAddress;
	get<2>(tlbs[frameNo]) = true;
}


static inline uint64_t bit_mask(uint64_t x)
{
	uint64_t mask = 0;
	const uint64_t digit = 1;
	for (uint64_t i = 0; i < x; i++) {
		mask |= digit << i;
	}
	return mask;
}

const static uint64_t SUPER_DIR_BL= 11;
const static uint64_t SUPER_DIR_SHIFT = 37;
const static uint64_t DIR_BL = 9;
const static uint64_t DIR_SHIFT = 28;
const static uint64_t SUPER_TAB_BL = 9;
const static uint64_t SUPER_TAB_SHIFT = 19;
const static uint64_t TAB_BL = 19;
const static uint64_t TAB_SHIFT = 9;
const static uint64_t ADDRESS_SPACE_LEN = 48;


//below is always called from the interrupt context
const pair<uint64_t, uint8_t>
    Processor::mapToGlobalAddress(const uint64_t& address)
{
	uint64_t globalPagesBase = 0x800;
    	//48 bit addresses
    	uint64_t address48 = address & bit_mask(ADDRESS_SPACE_LEN);
    	uint64_t superDirectoryIndex = (address48 >> SUPER_DIR_SHIFT) & 
		bit_mask(SUPER_DIR_BL);
    	uint64_t directoryIndex = (address48 >> DIR_SHIFT) & bit_mask(DIR_BL);
    	uint64_t superTableIndex = (address48 >> SUPER_TAB_SHIFT) & bit_mask(SUPER_TAB_BL);
    	uint64_t tableIndex = (address48 & bit_mask(TAB_BL)) >> TAB_SHIFT;
    	waitATick();
    	//read off the superDirectory number
    	//simulate read of global table
    	fetchAddressToRegister();
    	uint64_t ptrToDirectory = masterTile->readLong(globalPagesBase +
        	superDirectoryIndex * (sizeof(uint64_t) + sizeof(uint8_t)));
    	if (ptrToDirectory == 0) {
        	cerr << "Bad SuperDirectory: " << hex << address << endl;
        	throw new bad_exception();
    	}
    	waitATick();
    	fetchAddressToRegister();
    	uint64_t ptrToSuperTable = masterTile->readLong(ptrToDirectory +
        	directoryIndex * (sizeof(uint64_t) + sizeof(uint8_t)));
    	if (ptrToSuperTable == 0) {
        	cerr << "Bad Directory: " << hex << address << endl;
        	throw new bad_exception();
    	}
    	waitATick();
    	fetchAddressToRegister();
    	uint64_t ptrToTable = masterTile->readLong(ptrToSuperTable +
        	superTableIndex * (sizeof(uint64_t) + sizeof(uint8_t)));
    	if (ptrToTable == 0) {
        	cerr << "Bad SuperTable: " << hex << address << endl;
        	throw new bad_exception();
    	}
    	waitATick();
    	fetchAddressToRegister();
    	pair<uint64_t, uint8_t> globalPageTableEntry(
        	masterTile->readLong(ptrToTable + tableIndex *
                 (sizeof(uint64_t) + sizeof(uint8_t))),
        	masterTile->readByte(ptrToTable + tableIndex *
                 (sizeof(uint64_t) + sizeof(uint8_t)) + sizeof(uint64_t)));
    	waitATick();
   	 return globalPageTableEntry;

}

uint64_t Processor::triggerHardFault(const uint64_t& address,
    const bool& readOnly, const bool& write)
{
    	emit hardFault();
    	hardFaultCount++;
    	interruptBegin();
   	const pair<const uint64_t, bool> frameData = getFreeFrame();
    	if (frameData.second) {
        	writeBackMemory(frameData.first);
    	}
    	pair<uint64_t, uint8_t> translatedAddress = mapToGlobalAddress(address);
    	fixTLB(frameData.first, translatedAddress.first);
	for (int i = 0; i < (1 << PAGE_SHIFT); i += BITMAP_BYTES) { 
    		transferGlobalToLocal(translatedAddress.first + i,
        		tlbs[frameData.first], BITMAP_BYTES, write);
	}
    	fixPageMap(frameData.first, translatedAddress.first, readOnly);
    	interruptEnd();
    	return generateAddress(frameData.first, translatedAddress.first +
        	(address & bitMask));
}

void Processor::incrementBlocks()
{
	ControlThread *pBarrier = masterTile->getBarrier();
	pBarrier->incrementBlocks();
        blocks++;
}

void Processor::incrementServiceTime()
{
        serviceTime++;
}

//when this returns, address guarenteed to be present at returned local address
uint64_t Processor::fetchAddressRead(const uint64_t& address,
    const bool& readOnly, const bool& write)
{
	//implement paging logic
	if (mode == VIRTUAL) {
		uint64_t pageSought = address & pageMask;
	 	uint64_t y = 0;
		for (auto x: tlbs) {
            		if (get<2>(x) && (pageSought == 
				(get<0>(x) & pageMask))) {
                		return generateAddress(y, address);
			}
            		y++;
		}
		//not in TLB - but check if it is in page table
		waitATick(); 
		for (unsigned int i = 0; i < TOTAL_LOCAL_PAGES; i++) {
			waitATick();
            		uint64_t addressInPageTable = PAGESLOCAL +
                        	(i * PAGETABLEENTRY) + (1 << pageShift) * KERNELPAGES;
            		uint64_t flags = masterTile->readWord32(
				addressInPageTable
                        	+ FLAGOFFSET);
            		if (!(flags & 0x01)) {
                		continue;
            		}
            		waitATick();
            		uint64_t storedPage = masterTile->readLong(
                        	addressInPageTable + VOFFSET);
            		waitATick();
            		if (pageSought == storedPage) {
                		waitATick();
                		flags |= 0x04;
                		masterTile->writeWord32(
					addressInPageTable + FLAGOFFSET,
                    			flags);
                		waitATick();
                		fixTLB(i, address);
                		waitATick();
                		return fetchAddressRead(address);
            		}
            		waitATick();
        	}
        	waitATick();
        	return triggerHardFault(address, readOnly, write);
	} else {
		//what do we do if it's physical address?
		return address;
	}
}

uint64_t Processor::fetchAddressWrite(const uint64_t& address)
{
	const bool readOnly = false;
    	//implement paging logic
    	if (mode == VIRTUAL) {
        	uint64_t pageSought = address & pageMask;
        	uint64_t y = 0;
        	for (auto x: tlbs) {
            		if (get<2>(x) && (pageSought == 
				(get<0>(x) & pageMask))) {
                	return generateAddress(y, address);
            	}
            	y++;
        }
        //not in TLB - but check if it is in page table
        waitATick();
        for (unsigned int i = 0; i < TOTAL_LOCAL_PAGES; i++) {
            	waitATick();
            	uint64_t addressInPageTable = PAGESLOCAL +
                        (i * PAGETABLEENTRY) + (1 << pageShift) * KERNELPAGES;
            	uint64_t flags = masterTile->readWord32(addressInPageTable
                        + FLAGOFFSET);
            	if (!(flags & 0x01)) {
                	continue;
            	}
            	waitATick();
            	uint64_t storedPage = masterTile->readLong(
                        addressInPageTable + VOFFSET);
            	waitATick();
            	if (pageSought == storedPage) {
                	waitATick();
                	flags |= 0x04;
                	masterTile->writeWord32(addressInPageTable + FLAGOFFSET,
                    		flags);
                	waitATick();
                	fixTLB(i, address);
                	waitATick();
                	return fetchAddressRead(address);
            	}
            	waitATick();
        }
        waitATick();
        return triggerHardFault(address, readOnly, true);
    	} else {
        	//what do we do if it's physical address?
        	return address;
    }
}

//function to mimic delay from read of global page tables
void Processor::fetchAddressToRegister()
{
    emit smallFault();
    smallFaultCount++;
    requestRemoteMemory(0x0, 0x0, 0x0, false);
}
		
void Processor::writeAddress(const uint64_t& address,
	const uint64_t& value)
{
    uint64_t fetchedAddress = fetchAddressWrite(address);
    masterTile->writeLong(fetchedAddress, value);
}

void Processor::writeAddress64(const uint64_t& address)
{
    writeAddress(address, 0);
}

void Processor::writeAddress32(const uint64_t& address)
{
    writeAddress(address, 0);
}

void Processor::writeAddress16(const uint64_t& address)
{
    writeAddress(address, 0);
}

void Processor::writeAddress8(const uint64_t& address)
{
    writeAddress(address, 0);
}

uint64_t Processor::getLongAddress(const uint64_t& address)
{
	return masterTile->readLong(fetchAddressRead(address));
}

uint8_t Processor::getAddress(const uint64_t& address)
{
	return masterTile->readByte(fetchAddressRead(address));
}

uint64_t Processor::getStackPointer() const
{
    return stackPointer;
}

void Processor::setRegister(const uint64_t& regNumber,
    const uint64_t& value)
{
	//R0 always a zero
	if (regNumber == 0) {
		return;
	} else if (regNumber > REGISTER_FILE_SIZE - 1) {
		throw "Bad register number";
	}
	else {
		registerFile[regNumber] = value;
	}
}

uint64_t Processor::getRegister(const uint64_t& regNumber) const
{
	if (regNumber == 0) {
		return 0;
	}
	else if (regNumber > REGISTER_FILE_SIZE - 1) {
		throw "Bad register number";
	}
	else {
		return registerFile[regNumber];
	}
}

uint64_t Processor::multiplyWithCarry(const uint64_t& A,
    const uint64_t& B)
{
	carryBit = false;
    checkCarryBit();
	if (A == 0 || B == 0) {
		return 0;
	} else {
		if (A > ULLONG_MAX / B) {
			carryBit = true;
            checkCarryBit();
		}
		return A * B;
	}
}

uint64_t Processor::subtractWithCarry(const uint64_t &A, const uint64_t& B)
{
    uint64_t a = A;
    uint64_t b = B;
    carryBit = false;
    if (b > a) {
        carryBit = true;
        uint64_t c = b;
        b = a;
        a = c;
    }
    checkCarryBit();
    return a - b;
}

void Processor::checkCarryBit()
{
    statusWord[1] = carryBit;
}

void Processor::setPCNull()
{
	programCounter = 0;
}

void Processor::start()
{
	switchModeVirtual();
	ControlThread *pBarrier = masterTile->getBarrier();
	pBarrier->waitForBegin();
	//enforce power discipline from start
	int jitter = rand()%257;
	for (int i = 0; i < jitter; i++) {
		idleTick();
	}
	pBarrier->sufficientPower(this);
	waitATick();
}	

void Processor::pcAdvance(const long count)
{
	programCounter += count;
	fetchAddressRead(programCounter);
	waitATick();
	masterTile->getBarrier()->sufficientPower(this);
}

void Processor::idleTick()
{
	ControlThread *pBarrier = masterTile->getBarrier();
	pBarrier->releaseToRun();
}

void Processor::waitATick()
{
	ControlThread *pBarrier = masterTile->getBarrier();
	pBarrier->releaseToRun();
	totalTicks++;
	if (totalTicks%clockTicks == 0) {
		clockDue = true;
	}	
	if (clockDue && !inClock) {
		clockDue = false;
		activateClock();
	}
	if (!inInterrupt && masterTile->getPowerState() == false) {
		waitATick();
	}
}

void Processor::waitGlobalTick()
{
	for (uint64_t i = 0; i < GLOBALCLOCKSLOW; i++) {
		ControlThread *pBarrier = masterTile->getBarrier();
		pBarrier->releaseToRun();
		totalTicks++;
		if (totalTicks%clockTicks == 0) {
			clockDue == true;
		}	
	}
}

void Processor::pushStackPointer()
{
	stackPointer -= sizeof(uint64_t);
    	if (stackPointer >= stackPointerUnder) {
        	cerr << "Stack Underflow" << endl;
        	throw "Stack Underflow\n";
	}
}

void Processor::popStackPointer()
{
	stackPointer += sizeof(uint64_t);
	if (stackPointer < stackPointerOver) {
        	cerr << "Stack Overflow" << endl;
        	throw "Stack Overflow\n";
	}
}

void Processor::activateClock()
{
	if (inInterrupt) {
		return;
	}
	masterTile->setPowerStateOn(); //wake up if needed
	inClock = true;
    	uint64_t pages = TILE_MEM_SIZE >> pageShift;
	interruptBegin();
    	int wiped = 0;
    	for (uint8_t i = 0; i < pages; i++) {
		waitATick();
        	uint64_t flagAddress = (1 << pageShift) * KERNELPAGES  
			+ PAGESLOCAL +
                	((i + currentTLB) % pagesAvailable) * PAGETABLEENTRY
                	+ FLAGOFFSET;
		uint32_t flags = masterTile->readWord32(flagAddress);
		waitATick();
        	if (!(flags & 0x01) || flags & 0x02) {
			continue;
		}
		flags = flags & (~0x04);
		waitATick();
		masterTile->writeWord32(flagAddress, flags);
		waitATick();
        	get<2>(tlbs[(i + currentTLB) % pagesAvailable]) = false;
        	if (++wiped >= clockWipe)
            		break;
	}
	waitATick();
	currentTLB = (currentTLB + clockWipe) % pagesAvailable;
	inClock = false;
	interruptEnd();
}

void Processor::dumpPageFromTLB(const uint64_t& address)
{
    waitATick();
    uint64_t pageAddress = address & pageMask;
    for (auto& x: tlbs) {
        if (get<0>(x) == pageAddress) {
            get<2>(x) = false;
            break;
        }
    }
}
