#ifndef __FUNCTOR_
#define __FUNCTOR_

#define OUTPOINT 0x1000

class ProcessorFunctor {

private:
	Tile *tile;
	Processor *proc;
	void add_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& rC) const;
	void addi_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& imm) const;
	void addm_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& address) const;
	void and_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& rC) const;
	void sw_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& rC) const;
	void swi_(const uint64_t& rA, const uint64_t& rB,
	const uint64_t& imm) const;
	void lw_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& rC) const;
	void lwi_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& imm) const;
	bool beq_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& addr) const;
	void br_(const uint64_t& addr) const;
	void mul_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& rC) const;
	void muli_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& imm) const;
	void getsw_(const uint64_t& regA) const;
	void setsw_(const uint64_t& regA) const;
	void getsp_(const uint64_t& regA) const;
	void setsp_(const uint64_t& regA) const;
	void push_(const uint64_t& regA) const;
	void pop_(const uint64_t& regA) const;

	void loadInitialData(const uint64_t order);

public:
	ProcessorFunctor(Tile *tileIn);
	void operator()();
};

#endif
