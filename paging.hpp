#ifndef _PAGING_CLASS_
#define _PAGING_CLASS_


#define MAXREGIONS 4
#define MAXGROW	3
#define MAXDROP 4095

//each region is one TB

class RegionList {
	private:
	std::vector<unsigned long> regions;

	public:
	bool addRegion(const unsigned long& number);
	bool isAddressValid(const unsigned long& address) const;
	bool addRegionForAddress(const unsigned long& address);
};


class PageTable {
	private:
    std::vector<std::pair<uint64_t, uint8_t>> entries;
	int length;
	
	public:
	PageTable(int bitLength);
    uint8_t getPageFlags(const uint64_t& index) const;
    void setPageFlags(const uint64_t& index, uint8_t flags);
    unsigned long streamToMemory(Memory& mem, uint64_t start);
};

#endif
