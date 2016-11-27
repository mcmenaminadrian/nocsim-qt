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
#include <QFile>
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

#define PAGE_TABLE_COUNT 256

using namespace std;

Noc::Noc(const long columns, const long rows, const long pageShift,
    const long bSize, MainWindow* pWind, const long blocks):
    columnCount(columns), rowCount(rows),
    blockSize(bSize), mainWindow(pWind), memoryBlocks(blocks)
{
    uint64_t number = 0;
    for (int i = 0; i < columns; i++) {
		tiles.push_back(vector<Tile *>(rows));
		for (int j = 0; j < rows; j++) {
    		        tiles[i][j] = new Tile(
				this, i, j, pageShift, mainWindow, number++);
		}
	}
	//construct non-memory network
	//NB currently not used
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

	//in reality we are only using one tree and one memory block
	trees.push_back(new Tree(globalMemory[0], *this, columns, rows));
/*
	for (int i = 0; i < memoryBlocks; i++)
	{
		trees.push_back(new Tree(globalMemory[i], *this, columns,
			rows));
	}
*/
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
	ifstream inputFile(path);
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
}	

//memory regions - pair: 1st is number, 2nd is flag
//on flag - bit 1 is valid

unsigned long Noc::createBasicPageTables()
{
    uint64_t startOfPageTables = 2048;
	//create a bottom of the heirarchy table

    PageTable superDirectory(12);
    uint64_t runLength = 0;
    uint64_t superDirectoryLength =
		superDirectory.streamToMemory(globalMemory[0],
		startOfPageTables);
    globalMemory[0].writeLong(startOfPageTables + runLength,
        startOfPageTables + runLength + superDirectoryLength);
	//mark address as valid
    globalMemory[0].writeByte(startOfPageTables + sizeof(uint64_t),
		1);
    runLength += superDirectoryLength;

    PageTable directory(12);
    uint64_t directoryLength =
		directory.streamToMemory(globalMemory[0],
		startOfPageTables + runLength);
	globalMemory[0].writeLong(startOfPageTables + runLength,
		startOfPageTables + runLength + directoryLength);
	globalMemory[0].writeByte(
        startOfPageTables + runLength + sizeof(uint64_t), 1);
	runLength += directoryLength;

    PageTable superTable(12);
    uint64_t superTableLength =
		superTable.streamToMemory(globalMemory[0],
		startOfPageTables + runLength);
	globalMemory[0].writeLong(startOfPageTables + runLength,
        startOfPageTables + runLength + superTableLength);
	globalMemory[0].writeByte(
        startOfPageTables + runLength + sizeof(uint64_t), 1);
    runLength += superTableLength;

    vector<PageTable> tables;
    for (int i = 0; i < PAGE_TABLE_COUNT; i++) {
        PageTable pageTable(8);
        tables.push_back(pageTable);
    }
    uint64_t tableLength =
        tables[0].streamToMemory(globalMemory[0],
		startOfPageTables + runLength);
    for (int i = 1; i < PAGE_TABLE_COUNT; i++) {
        tables[i].streamToMemory(globalMemory[0],
                startOfPageTables + runLength + i * tableLength);
    }
    for (int i = 0; i < PAGE_TABLE_COUNT; i++) {
        uint64_t offsetA = startOfPageTables + runLength - superTableLength +
                i * (sizeof(uint64_t) + sizeof(uint8_t));
        globalMemory[0].writeLong(offsetA,
            startOfPageTables + runLength + tableLength * i);
        globalMemory[0].writeByte(offsetA + sizeof(uint64_t), 0x01);
    }
    uint64_t bottomOfPageTable = runLength + tableLength * PAGE_TABLE_COUNT;
    for (unsigned int i = 0; i < (1 << 8) * PAGE_TABLE_COUNT; i++) {
        uint64_t offsetB = startOfPageTables + runLength
                + i * (sizeof(uint64_t) + sizeof(uint8_t));
        globalMemory[0].writeLong(offsetB, i * (1 << PAGE_SHIFT));
        uint8_t flagOut = 0x03;
        if (i > (2 + ((bottomOfPageTable + startOfPageTables) >> PAGE_SHIFT)))
        {
            	flagOut = 0x01;
        }
        globalMemory[0].writeByte(offsetB + sizeof(uint64_t), flagOut);
    	}

    	runLength += tableLength * PAGE_TABLE_COUNT;

    	unsigned long pagesUsedForTables = runLength >> PAGE_SHIFT;
	if (runLength%1024) {
		pagesUsedForTables++;
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
