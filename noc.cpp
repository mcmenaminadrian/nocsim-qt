#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>
#include <map>
#include <string>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <bitset>
#include <QFileDialog>
#include <QString>
#include "mainwindow.h"
#include "memory.hpp"
#include "ControlThread.hpp"
#include "memorypacket.hpp"
#include "mux.hpp"
#include "noc.hpp"
#include "tile.hpp"
#include "tree.hpp"
#include "processor.hpp"
#include "paging.hpp"
#include "processorFunc.hpp"
#include "ControlThread.hpp"


using namespace std;

Noc::Noc(const long columns, const long rows, const long pageShift,
    const long blocks, const long bSize, MainWindow* pWind):
    columnCount(columns), rowCount(rows), memoryBlocks(blocks),
    blockSize(bSize), mainWindow(pWind)
{
    uint64_t number = 0;
	for (int i = 0; i < columns; i++) {
		tiles.push_back(vector<Tile *>(rows));
		for (int j = 0; j < rows; j++) {
            tiles[i][j] = new Tile(this, i, j, pageShift, mainWindow, number++);
		}
	}
	//construct non-memory network
	for (int i = 0; i < columns; i++) {
		for (int j = 0; j < (rows - 1); j++) {
			tiles[i][j]->addConnection(i, j + 1);
			tiles[i][j + 1]->addConnection(i, j);
		}
	}
	for (int i = 0; i < (columns - 1); i++) {
		for (int j = 0; j < rows; j++) {
			tiles[i][j]->addConnection(i + 1, j);
			tiles[i + 1][j]->addConnection(i, j);
		}
	}

	for (int i = 0; i < memoryBlocks; i++) {
		globalMemory.push_back(Memory(i * blockSize, blockSize));
	}

	for (int i = 0; i < memoryBlocks; i++)
	{
		trees.push_back(new Tree(globalMemory[i], *this, columns,
			rows));
	}
	pBarrier = nullptr;
}

Noc::~Noc()
{
	for (int i = 0; i < columnCount; i++) {
		for (int j = 0; j < rowCount; j++) {
			Tile* toGo = tiles[i][j];
			delete toGo;
		}
	}

	for (int i = 0; i < memoryBlocks; i++) {
		delete trees[i];
	}

}

bool Noc::attach(Tree& memoryTree, const long leafTile)
{
	return true;
}

Tile* Noc::tileAt(long i)
{
	if (i >= columnCount * rowCount || i < 0){
		return NULL;
	}
	long columnAccessed = i/columnCount;
	long rowAccessed = i - (columnAccessed * rowCount);
	return tiles[columnAccessed][rowAccessed];
}

long Noc::readInVariables(const string& path)
{
    ifstream inputFile("/Users/adrian/noc-qt/variables.csv");
	//first line is the answer
	string rawAnswer;
	getline(inputFile, rawAnswer);
	istringstream stringy(rawAnswer);
	string number;
	while(getline(stringy, number, ',')) {
		answers.push_back(atol(number.c_str()));
	}

	long lineCnt = 0;
	//now read in the system
	while(getline(inputFile, rawAnswer)) {
		lineCnt++;
		istringstream stringy(rawAnswer);
		vector<long> innerLine;
		while (getline(stringy, number, ',')) {
			innerLine.push_back(atol(number.c_str()));
		}
		lines.push_back(innerLine);
	}
	return lineCnt;
}

unsigned long Noc::scanLevelFourTable(unsigned long offsetAddr)
{
	//scan through pages looking for first available, non-fixed
	for (int i = 0; i < (1 << 18); i++) {
		uint8_t pageStatus = globalMemory[0].
			readByte(offsetAddr + sizeof(long));
		if (pageStatus == 0) {
			goto fail;
		} else if (pageStatus == 0x01) {
			return offsetAddr;
		}
		offsetAddr += (sizeof(long) + 1);
	}
fail:
	cerr << "Run out of pages" << endl;
	throw "Error";
}

//get rid of any unneeded entries
void Noc::cleanRestOfPageTable(unsigned long address)
{
	unsigned long entrySize = sizeof(long) + 1;
	
	unsigned long levelTwoTableAddr =
		globalMemory[0].
		readLong(ptrBasePageTables + (address >> 52) * entrySize);
	unsigned long levelThreeTableAddr =
		globalMemory[0].readLong(levelTwoTableAddr + 
			((address & 0xFFF0000000000) >> 40) * entrySize);
	unsigned long levelFourTableAddr =
		globalMemory[0].readLong(levelThreeTableAddr +
			((address & 0xFFF0000000) >> 28) * entrySize);
	unsigned long entryOffset =
		(((address & 0xFFFFC00) >> 10) + 1) * entrySize;
	while (true) {
		uint8_t status = globalMemory[0].
			readByte(levelFourTableAddr + entryOffset +
			sizeof(long));
		if (status != 0) {
			globalMemory[0].writeByte(
			levelFourTableAddr + entryOffset + sizeof(long), 0);
		} else {
			return;
		}
		entryOffset += entrySize;
	}
}	

void Noc::writeSystemToMemory()
{
	//write variables out to memory as AP integers
	//begin by looking through pages for first non-fixed pages
	unsigned long levelTwoTableAddr =
		globalMemory[0].readLong(ptrBasePageTables);
	unsigned long levelThreeTableAddr =
		globalMemory[0].readLong(levelTwoTableAddr);
	unsigned long levelFourTableAddr = globalMemory[0].
		readLong(levelThreeTableAddr);
	unsigned long firstFreePageAddr =
		scanLevelFourTable(levelFourTableAddr);
	unsigned long address = globalMemory[0].readLong(firstFreePageAddr);
	globalMemory[0].writeLong(sizeof(long) * 2, address);
    for (uint32_t i = 0; i < lines.size(); i++) {
        for (uint32_t j = 0; j <= lines.size(); j++) {
			//nominator
			long sign = sgn(lines[i][j]);
			if (sign < 1) {
				globalMemory[0].writeByte(address, 0x01);
			} else {
				globalMemory[0].writeByte(address, 0);
			}
			address++;
			globalMemory[0].writeByte(address, APNUMBERSIZE);
            address+= (sizeof(uint64_t) - 1);
            globalMemory[0].writeLong(address,abs(lines[i][j]));
            address += sizeof(uint64_t);
            for (int k = 0; k < APNUMBERSIZE - 1; k++) {
				globalMemory[0].writeLong(address, 0);
                address += sizeof(uint64_t);
			}
			//denominator
            globalMemory[0].writeLong(address , 1);
            address+= sizeof(uint64_t);
            for (int k = 0; k < APNUMBERSIZE - 1; k++) {
				globalMemory[0].writeLong(address, 0);
                address += sizeof(uint64_t);
			}	
		}
	}
	cleanRestOfPageTable(address);
    //signal only core 0 should execute
    globalMemory[0].writeLong(0x100, 0xFF00);
}	

//memory regions - pair: 1st is number, 2nd is flag
//on flag - bit 1 is valid

unsigned long Noc::createBasicPageTables()
{
	unsigned long startOfPageTables = 2048;
	//create a bottom of the heirarchy table
	PageTable superDirectory(12);
	unsigned long runLength =
		superDirectory.streamToMemory(globalMemory[0],
		startOfPageTables);

	globalMemory[0].writeLong(startOfPageTables,
		startOfPageTables + runLength);
	//mark address as valid
	globalMemory[0].writeByte(startOfPageTables + sizeof(long),
		1);
	PageTable directory(12);
	unsigned long directoryLength = 
		directory.streamToMemory(globalMemory[0],
		startOfPageTables + runLength);
	globalMemory[0].writeLong(startOfPageTables + runLength,
		startOfPageTables + runLength + directoryLength);
	globalMemory[0].writeByte(
		startOfPageTables + runLength + sizeof(long), 1);
	runLength += directoryLength;
	PageTable superTable(12);
	directoryLength = 
		superTable.streamToMemory(globalMemory[0],
		startOfPageTables + runLength);
	globalMemory[0].writeLong(startOfPageTables + runLength,
		startOfPageTables + runLength + directoryLength);
	globalMemory[0].writeByte(
		startOfPageTables + runLength + sizeof(long), 1);
	runLength += directoryLength;
	PageTable table(18);
	directoryLength = 
		table.streamToMemory(globalMemory[0],
		startOfPageTables + runLength);
	unsigned long bottomOfPageTable = runLength;
	runLength += directoryLength;

	unsigned long pagesUsedForTables = runLength/1024;
	if (runLength%1024) {
		pagesUsedForTables++;
	}

	unsigned long sizeOfEntry = sizeof(long) + 1;	
    for (uint32_t i = 0; i < pagesUsedForTables + 2; i++) {
		globalMemory[0].writeLong(
			startOfPageTables + bottomOfPageTable +
			i * sizeOfEntry, i * 1024);
		globalMemory[0].writeByte(
			startOfPageTables + bottomOfPageTable + 
			i * sizeOfEntry + sizeof(long), 0x03);
	}
	//mark out 12MB more
    for (uint32_t i = pagesUsedForTables + 2; i < (pagesUsedForTables + 12002);
		 i++) {
			globalMemory[0].writeLong(
				startOfPageTables + bottomOfPageTable +
				i * sizeOfEntry, (i + 100) * 1024);
		globalMemory[0].writeByte(startOfPageTables + bottomOfPageTable
		+ i * sizeOfEntry + sizeof(long), 0x01);
	}
	
	return startOfPageTables;
}

long Noc::executeInstructions()
{
	//set up global memory map
	//Regions
	RegionList startRegions;
	startRegions.addRegion(0);
	startRegions.addRegion(4096);

	ptrBasePageTables = createBasicPageTables();

    readInVariables();
	writeSystemToMemory();
    pBarrier = new ControlThread(0, mainWindow);
	vector<thread *> threads;

	for (int i = 0; i < columnCount * rowCount; i++) {
		ProcessorFunctor funcky(tileAt(i));
		//spawn a thread per tile
		threads.push_back(new thread(funcky));
		pBarrier->incrementTaskCount();
		
	}
	pBarrier->begin();
	for (int i = 0; i < columnCount * rowCount; i++) {
		threads[i]->join();
	}
	delete pBarrier;
	pBarrier = nullptr;
	return 0;
}

ControlThread* Noc::getBarrier()
{
	return pBarrier;
}
