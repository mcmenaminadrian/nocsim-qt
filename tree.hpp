#ifndef _TREE_CLASS_
#define _TREE_CLASS_

class Noc;
class Mux;
class Memory;

class Tree {

private:
	std::vector<std::vector<Mux>> nodesTree;
	long levels;
	

public:
	Tree(Memory& globalMemory, Noc& noc,
		const long columns, const long rows);
};
#endif
