#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <bitset>
#include "memorypacket.hpp"
#include "mux.hpp"
#include "memory.hpp"
#include "ControlThread.hpp"
#include "noc.hpp"
#include "tree.hpp"
#include "tile.hpp"
#include "processor.hpp"


using namespace std;


void usage() {
	cout << "nocSIM: simulate a large NOC array" << endl;
	cout << "Copyright Adrian McMenamin, 2015" << endl;
	cout << "---------" << endl;
	cout << "-b    Memory blocks: default 4" << endl;
	cout << "-s    Memory block size: default 1GB" << endl;
	cout << "-r    Rows of CPUs in NoC (default 16)" << endl;
	cout << "-c    Columns of CPUs in NoC (default 16)" << endl;
	cout << "-p    Page size in power of 2 (default 10)" << endl;
	cout << "-?    Print this message and exit" << endl;
}

int main(int argc, char *argv[])
{
	long memoryBlocks = 1;
	long blockSize = 1024 * 1024 * 1024;
	long rows = 16;
	long columns = 16;
	long pageShift = PAGE_SHIFT;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-?") == 0) {
			usage();
			exit(EXIT_SUCCESS);
		}
		if (strcmp(argv[i], "-b") == 0) {
			memoryBlocks = atol(argv[++i]);
			continue;
		}
		if (strcmp(argv[i], "-s") == 0) {
			blockSize = atol(argv[++i]);
			continue;
		}
		if (strcmp(argv[i], "-r") == 0) {
			rows = atol(argv[++i]);
			continue;
		}
		if (strcmp(argv[i], "-c") == 0) {
			columns = atol(argv[++i]);
			continue;
		}
		if (strcmp(argv[i], "-p") == 0) {
			pageShift = atol(argv[++i]);
			continue;
		}
		
		//unrecognised option
		usage();
		exit(EXIT_FAILURE);
	}

	long totalTiles = rows * columns;
	if ((totalTiles == 0) || (totalTiles & (totalTiles - 1))) {
		cout << "Must have power of two for number of tiles." << endl;
		exit(EXIT_FAILURE);
	}  

	Noc networkTiles(columns, rows, pageShift, memoryBlocks, blockSize);
	//Let's Go!
	networkTiles.executeInstructions();
}
