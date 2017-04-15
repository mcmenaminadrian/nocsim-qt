#define TILE_MEM_SIZE (17 * 1024)
#ifndef _TILE_CLASS_
#define _TILE_CLASS_
#include <QString>

class Memory;
class Processor;
class Noc;

class Tile
{
private:
	Memory *tileLocalMemory;
	const std::pair<const long, const long> coordinates;
	std::vector<std::pair<long, long> > connections;
	Noc *parentBoard;
	MainWindow *mainWindow;

public:
    	Tile(Noc* parent, const long col, const long r, const long pShift,
        	MainWindow *mW, uint64_t numb);
	~Tile();
	Mux *treeLeaf;
	Processor *tileProcessor;
	void addTreeLeaf(Mux* leaf);
	void addConnection(const long col, const long row);
    	unsigned long getOrder() const;
    	long getRow() const {return coordinates.second;}
    	long getColumn() const { return coordinates.first;}


	//memory pass through
    	uint8_t readByte(const uint64_t& address) const;
    	uint64_t readLong(const uint64_t& address) const;
    	uint32_t readWord32(const uint64_t& address) const;
    	void writeWord32(const uint64_t& address, const uint32_t& value) const;
    	void writeByte(const uint64_t& address, const uint8_t& value) const;
   	 void writeLong(const uint64_t& address, const uint64_t& value) const;
	ControlThread *getBarrier();
};

#endif
