#include <iostream>
#include <vector>
#include <map>
#include <mutex>
#include <bitset>
#include <condition_variable>
#include "mainwindow.h"
#include "ControlThread.hpp"
#include "memorypacket.hpp"
#include "mux.hpp"
#include "memory.hpp"
#include "tree.hpp"
#include "noc.hpp"
#include "tile.hpp"
#include "processor.hpp"


using namespace std;

Tree::Tree(Memory& globalMemory, Noc& noc, const long columns, const long rows)
{
	long totalLeaves = columns * rows;
	levels = 0;
	long muxCount = totalLeaves;

	//create the nodes
	while (muxCount > 1) {
		nodesTree.push_back(vector<Mux>(muxCount));
		for (unsigned int i = 0; i < nodesTree[levels].size(); i++){
			nodesTree[levels][i].assignGlobalMemory(&globalMemory);
		}
		muxCount /= 2;
		levels++;
	}
	//number the leaves
	for (unsigned int i = 0; i < nodesTree[0].size(); i++)
	{
		nodesTree[0][i].assignNumbers(i, i, i, i);
		Tile *targetTile = noc.tileAt(i);
		if (!targetTile) {
			cout << "Bad tile index" << endl;
			throw "tile index error";
		}
		targetTile->addTreeLeaf(&(nodesTree[0][i]));
	}
	//root Mux - connects to global memory
	nodesTree.push_back(vector<Mux>(1));
	nodesTree[levels][0].assignGlobalMemory(&globalMemory);
	for (int i = 0; i <= levels; i++) {
		for (unsigned int j = 0; j < nodesTree[i].size(); j++) {
			if (i > 0) {
				nodesTree[i][j].downstreamMuxLow = 
					&(nodesTree[i - 1][j * 2]);
				nodesTree[i][j].downstreamMuxHigh = 
					&(nodesTree[i - 1][j * 2 + 1]);
			}
			nodesTree[i][j].upstreamMux = &(nodesTree[i + 1][j/2]);
		}
	}
	//join the nodes internally
	for (int i = 0; i < levels; i++)
	{
		for (unsigned int j = 0; j < nodesTree[i].size(); j+= 2)
		{
			nodesTree[i + 1][j / 2].joinUpMux(
				nodesTree[i][j], nodesTree[i][j + 1]);
		}
	}
	//initialise the mutexes
	for (unsigned int i = 0; i < nodesTree.size(); i++) {
		for (unsigned int j = 0; j < nodesTree[i].size(); j++) {
			nodesTree[i][j].initialiseMutex();
		}
	}

	//attach root to global memory
	globalMemory.attachTree(&(nodesTree.at(nodesTree.size() - 1)[0]));
}
