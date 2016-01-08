#define TILE_MEM_SIZE (16 * 1024)

#ifndef _TILE_CLASS_
#define _TILE_CLASS_


class Memory;
class Processor;
class Noc;

class Tile
{
private:
	Memory *tileLocalMemory;
	Memory *globalMemory;
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
    uint8_t readByte(const unsigned long address) const;
    uint64_t readLong(const unsigned long address) const;
    uint32_t readWord32(const unsigned long address) const;
	void writeWord32(const unsigned long address, const uint32_t value) const;
	void writeByte(const unsigned long address, const uint8_t value) const;
	void writeLong(const unsigned long address, const unsigned long value) const;
	ControlThread *getBarrier();
};

#endif
