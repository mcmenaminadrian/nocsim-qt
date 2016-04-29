#ifndef __FUNCTOR_
#define __FUNCTOR_

#define OUTPOINT 0x1000

class ProcessorFunctor {

private:
    static const uint64_t sumCount;
	Tile *tile;
	Processor *proc;
    uint64_t startingPoint;
	void add_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& rC) const;
	void addi_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& imm) const;
	void and_(const uint64_t& rA, const uint64_t& rB,
		const uint64_t& rC) const;
    void andi_(const uint64_t& rA, const uint64_t& rB,
        const uint64_t& imm) const;
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
    void div_(const uint64_t& rA, const uint64_t& rB,
        const uint64_t& rC) const;
    void divi_(const uint64_t& rA, const uint64_t& rB,
        const uint64_t& imm) const;
    void subi_(const uint64_t& regA, const uint64_t& regB,
        const uint64_t& imm) const;
    void sub_(const uint64_t& regA, const uint64_t& regB,
        const uint64_t& regC) const;
	void getsw_(const uint64_t& regA) const;
	void setsw_(const uint64_t& regA) const;
	void getsp_(const uint64_t& regA) const;
	void setsp_(const uint64_t& regA) const;
	void push_(const uint64_t& regA) const;
	void pop_(const uint64_t& regA) const;
    void nop_() const;
    void shiftl_(const uint64_t& regA) const;
    void shiftli_(const uint64_t& regA, const uint64_t& imm) const;
    void shiftr_(const uint64_t& regA) const;
    void shiftri_(const uint64_t& regA, const uint64_t& imm) const;
    void xor_(const uint64_t& regA, const uint64_t& regB,
              const uint64_t& regC) const;
    void or_(const uint64_t& regA, const uint64_t& regB,
              const uint64_t& regC) const;
    void ori_(const uint64_t& regA, const uint64_t& regB,
              const uint64_t& imm) const;
    void shiftrr_(const uint64_t& regA, const uint64_t& regB)
        const;
    void shiftlr_(const uint64_t& regA, const uint64_t& regB)
        const;
    void normaliseLine() const;
    void euclidAlgorithm() const;
    void dropPage() const;
    void flushSelectedPage() const;
    void flushPages() const;
    void forcePageReload() const;
    void cleanCaches() const;
    void nextRound() const;
	void loadInitialData(const uint64_t order);

public:
	ProcessorFunctor(Tile *tileIn);
	void operator()();
};

#endif
