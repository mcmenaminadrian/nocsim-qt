#include <cstdlib>
#include <iostream>
#include <vector>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <bitset>
#include <map>
#include <QFile>
#include "mainwindow.h"
#include "ControlThread.hpp"
#include "memorypacket.hpp"
#include "mux.hpp"
#include "noc.hpp"
#include "memory.hpp"
#include "tile.hpp"
#include "processor.hpp"
#include "processorFunc.hpp"

using namespace std;

const uint64_t ProcessorFunctor::sumCount = 0x101;

//alter filter to trap per page bitmaps of less than 64bits
static const uint64_t BITMAP_FILTER = 0xFFFFFFFFFFFFFFFF;
//alter to adjust for page size
static const uint64_t PAGE_ADDRESS_MASK = 0xFFFFFFFFFFFFFC00;

//Number format
//numerator
//first 64 bits - sign in first byte (1 is negative)
//size in second byte
//further APUMBERSIZE 64 bit words follow
//then denominator - APNUMBERSIZE 64 bit words

//avoid magic numbers

enum reg {REG0, REG1, REG2, REG3, REG4, REG5, REG6, REG7, REG8, REG9,
	REG10, REG11, REG12, REG13, REG14, REG15, REG16, REG17, REG18, REG19,
	REG20, REG21, REG22, REG23, REG24, REG25, REG26, REG27, REG28, REG29,
	REG30, REG31};

//instructions
//limited RISC instruction set
//based on Ridiciulously Simple Computer concept
//instructions:
//	add_ 	rA, rB, rC	: rA <- rB + rC		add
//	addi_	rA, rB, imm	: rA <- rB + imm	add immediate
//	and_	rA, rB, rC	: rA <- rB & rC		and
//  andi_   rA, rB, imm : rA <- rB & imm    and immediate
//	sw_	rA, rB, rC	: rA -> *(rB + rC)	store word
//	swi_	rA, rB, imm	: rA -> *(rB + imm)	store word immediate
//	lw_	rA, rB, rC	: rA <- *(rB + rC)	load word
//	lwi_	rA, rB, imm	: rA <-	*(rB + imm)	load word immediate
//	beq_	rA, rB, imm	: PC <- imm iff rA == rB	branch if equal
//	br_	imm		: PC <- imm		branch immediate
//	mul_	rA, rB, rC	: rA <- rB * rC		multiply
//	muli_	rA, rB, imm	: rA <- rB * imm	multiply immediate
//  setsw_  rA          : set status word
//  getsw_  rA          : get status word
//  push_   rA          : rA -> *SP, SP++   push reg to stack
//  pop_    rA          : *SP -> rA, SP--   pop stack to reg
//  shiftl_ rA          : rA << 1           shift left
//  shiftlr_ rA, rB     : rA << rB          shift left
//  shiftli_ rA, imm    : rA << imm         shift left
//  shiftr_ rA          : rA >> 1           shift right
//  shiftrr_ rA, rB     : rA >> rB          shift right
//  shiftri_ rA, imm    : rA >> imm         shift right
//  div_    rA, rB, rC  : rA = rB/rC        integer division
//  divi_   rA, rB, imm : rA = rB/imm       integer division by immediate
//  sub_    rA, rB, rC  : rA = rB - rC      subtract (with carry)
//  subi_   rA, rB, rC  : rA = rB - imm     subtract immediate (with carry)
//  xor_    rA, rB, rC  : rA = rB xor rC    exclusive or
//  or_     rA, rB, rC  : rA = rB or rC     or
//  ori_    rA, rB, imm : rA = rB or imm    or immediate
//  nop_                 : no operation

void ProcessorFunctor::add_(const uint64_t& regA,
	const uint64_t& regB, const uint64_t& regC) const
{
	proc->setRegister(regA,
		proc->getRegister(regB) + proc->getRegister(regC));
	proc->pcAdvance();
}

void ProcessorFunctor::addi_(const uint64_t& regA,
	const uint64_t& regB, const uint64_t& imm) const
{
	proc->pcAdvance();
	proc->setRegister(regA, proc->getRegister(regB) + imm);
	proc->pcAdvance();
}

void ProcessorFunctor::and_(const uint64_t& regA,
	const uint64_t& regB, const uint64_t& regC) const
{
	proc->setRegister(regA,
		proc->getRegister(regB) & proc->getRegister(regC));
	proc->pcAdvance();
}

void ProcessorFunctor::andi_(const uint64_t& regA,
    const uint64_t& regB, const uint64_t& imm) const
{
    proc->pcAdvance();
    proc->setRegister(regA,
        proc->getRegister(regB) & imm);
    proc->pcAdvance();
}

void ProcessorFunctor::sw_(const uint64_t& regA, const uint64_t& regB,
	const uint64_t& regC) const
{
	proc->writeAddress(proc->getRegister(regB) + proc->getRegister(regC),
		proc->getRegister(regA));
	proc->pcAdvance();
}

void ProcessorFunctor::swi_(const uint64_t& regA,
	const uint64_t& regB, const uint64_t& address) const
{
	proc->pcAdvance();
	proc->writeAddress(proc->getRegister(regB) + address,
		proc->getRegister(regA));
	proc->pcAdvance();
}

void ProcessorFunctor::lw_(const uint64_t& regA, const uint64_t& regB,
	const uint64_t& regC) const
{
	proc->setRegister(regA, proc->getLongAddress(
		proc->getRegister(regB) + proc->getRegister(regC)));
	proc->pcAdvance();
}

void ProcessorFunctor::lwi_(const uint64_t& regA,
	const uint64_t& regB, const uint64_t& address) const
{
	proc->pcAdvance();
	proc->setRegister(regA, proc->getLongAddress(
		proc->getRegister(regB) + address)); 
	proc->pcAdvance();
}

bool ProcessorFunctor::beq_(const uint64_t& regA,
	const uint64_t& regB, const uint64_t& address) const
{
    proc->waitATick();
	if (proc->getRegister(regA) == proc->getRegister(regB)) {
		return true;
	} else {
		proc->pcAdvance();
		return false;
	}
}

void ProcessorFunctor::br_(const uint64_t& address) const
{
    proc->waitATick();
    proc->pcAdvance();
    //do nothing else
}

void ProcessorFunctor::nop_() const
{
    proc->pcAdvance();
    //no operation
    
}

void ProcessorFunctor::div_(const uint64_t& regA,
    const uint64_t& regB, const uint64_t& regC) const
{
    proc->setRegister(regA, proc->getRegister(regB) / proc->getRegister(regC));
    for (int i = 0; i < 32; i++) {
        proc->waitATick();
    }
    proc->pcAdvance();
}

void ProcessorFunctor::divi_(const uint64_t& regA,
    const uint64_t& regB, const uint64_t& imm) const
{
    proc->pcAdvance();
    proc->setRegister(regA, proc->getRegister(regB) / imm);
    for (int i = 0; i < 32; i++) {
        proc->waitATick();
    }
    proc->pcAdvance();
}


void ProcessorFunctor::mul_(const uint64_t& regA,
	const uint64_t& regB, const uint64_t& regC) const
{
	proc->setRegister(regA, 
		proc->multiplyWithCarry(
		proc->getRegister(regB), proc->getRegister(regC)));
	proc->pcAdvance();
}

void ProcessorFunctor::muli_(const uint64_t& regA,
	const uint64_t& regB, const uint64_t& multiplier) const
{
    proc->pcAdvance();
    proc->setRegister(regA, proc->multiplyWithCarry(proc->getRegister(regB),
        multiplier));
	proc->pcAdvance();
}

void ProcessorFunctor::sub_(const uint64_t& regA, const uint64_t& regB,
    const uint64_t& regC) const
{
    proc->setRegister(regA, proc->subtractWithCarry(proc->getRegister(regB),
        proc->getRegister(regC)));
    proc->pcAdvance();
}

void ProcessorFunctor::subi_(const uint64_t& regA, const uint64_t& regB,
    const uint64_t& imm) const
{
    proc->pcAdvance();
    proc->setRegister(regA, proc->subtractWithCarry(proc->getRegister(regB),
        imm));
    proc->pcAdvance();
}


void ProcessorFunctor::getsw_(const uint64_t& regA) const
{
	proc->setRegister(regA, proc->statusWord.to_ulong());
	proc->pcAdvance();
}

void ProcessorFunctor::setsw_(const uint64_t& regA) const
{
	uint32_t statusWord = proc->getRegister(regA);
	for (int i = 0; i < 32; i++) {
		proc->statusWord[i] = (statusWord & (1 << i));
	}
	proc->setMode();
	proc->pcAdvance();
}

void ProcessorFunctor::getsp_(const uint64_t& regA) const
{
	proc->setRegister(regA, proc->getStackPointer());
	proc->pcAdvance();
}

void ProcessorFunctor::setsp_(const uint64_t& regA) const
{
	proc->setStackPointer(proc->getRegister(regA));
	proc->pcAdvance();
}

void ProcessorFunctor::pop_(const uint64_t& regA) const
{
    uint64_t sP = proc->getStackPointer();
    proc->setRegister(regA, proc->getLongAddress(sP));
    proc->waitATick();
    proc->popStackPointer();
    proc->pcAdvance();
}

void ProcessorFunctor::push_(const uint64_t& regA) const
{
    proc->pushStackPointer();
    proc->waitATick();
    proc->writeAddress(proc->getStackPointer(), proc->getRegister(regA));
    proc->pcAdvance();
}

void ProcessorFunctor::shiftl_(const uint64_t& regA) const
{
    proc->setRegister(regA, proc->getRegister(regA) << 1);
    proc->pcAdvance();
}

void ProcessorFunctor::shiftli_(const uint64_t& regA, const uint64_t& imm)
    const
{
    proc->pcAdvance();
    proc->setRegister(regA, proc->getRegister(regA) << imm);
    proc->pcAdvance();
}

void ProcessorFunctor::shiftr_(const uint64_t& regA) const
{
    proc->setRegister(regA, proc->getRegister(regA) >> 1);
    proc->pcAdvance();
}

void ProcessorFunctor::shiftrr_(const uint64_t& regA, const uint64_t& regB)
    const
{
    proc->setRegister(regA,
        proc->getRegister(regA) >> proc->getRegister(regB));
    proc->pcAdvance();
}

void ProcessorFunctor::shiftlr_(const uint64_t& regA, const uint64_t& regB)
    const
{
    proc->setRegister(regA,
        proc->getRegister(regA) << proc->getRegister(regB));
    proc->pcAdvance();
}

void ProcessorFunctor::shiftri_(const uint64_t& regA, const uint64_t& imm)
    const
{
    proc->pcAdvance();
    proc->setRegister(regA, proc->getRegister(regA) >> imm);
    proc->pcAdvance();
}

void ProcessorFunctor::xor_(const uint64_t& regA, const uint64_t& regB,
    const uint64_t& regC) const
{
    proc->setRegister(regA, proc->getRegister(regB) ^ proc->getRegister(regC));
    proc->pcAdvance();
}

void ProcessorFunctor::or_(const uint64_t& regA, const uint64_t& regB,
    const uint64_t& regC) const
{
    proc->setRegister(regA, proc->getRegister(regB) | proc->getRegister(regC));
    proc->pcAdvance();
}

void ProcessorFunctor::ori_(const uint64_t& regA, const uint64_t& regB,
    const uint64_t& imm) const
{
    proc->pcAdvance();
    proc->setRegister(regA, proc->getRegister(regB) | imm);
    proc->pcAdvance();
}

///End of instruction set ///

#define SETSIZE 256

ProcessorFunctor::ProcessorFunctor(Tile *tileIn):
	tile{tileIn}, proc{tileIn->tileProcessor}
{
}

//return address in REG1
void ProcessorFunctor::flushPages() const
{
    uint64_t writeOutBytes;
    push_(REG1);
    br_(0);
    proc->flushPagesStart();
    //REG1 points to start of page table
    addi_(REG1, REG0, PAGETABLESLOCAL + (1 << PAGE_SHIFT));
    //REG2 counts number of pages
    addi_(REG2, REG0, TILE_MEM_SIZE >> PAGE_SHIFT);
    //REG9 points to start of bitmaps
    muli_(REG9, REG2, ENDOFFSET);
    shiftri_(REG9, PAGE_SHIFT);
    addi_(REG9, REG9, 0x01);
    shiftli_(REG9, PAGE_SHIFT);
    add_(REG9, REG9, REG1);
    //REG10 holds bitmap bytes per page
    addi_(REG10, REG0, 1 << PAGE_SHIFT);
    shiftri_(REG10, BITMAP_SHIFT + 0x03);
    //REG3 holds pages done so far
    add_(REG3, REG0, REG0);
    uint64_t aboutToCheck = proc->getProgramCounter();

check_page_status:
    proc->setProgramCounter(aboutToCheck);
    muli_(REG5, REG3, PAGETABLEENTRY);
    add_(REG17, REG1, REG5);
    //REG4 holds flags    
    lwi_(REG4, REG17, FLAGOFFSET);
    //don't flush an unmovable page
    andi_(REG30, REG4, 0x02);
    addi_(REG31, REG0, 0x02);
    if (beq_(REG30, REG31, 0)) {
        goto next_pte;
    }
    //don't flush an invalid page
    andi_(REG4, REG4, 0x01);
    if (beq_(REG4, REG0, 0)) {
        goto next_pte;
    }
    //load virtual address in REG4
    lwi_(REG4, REG17, VOFFSET);
    //test if it is remote
    subi_(REG5, REG4, PAGETABLESLOCAL);
    getsw_(REG5);
    andi_(REG5, REG5, 0x02);
    addi_(REG6, REG0, 0x02);
    if (beq_(REG5, REG6, 0)) {
        goto flush_page;
    }
    addi_(REG6, REG0, PAGETABLESLOCAL + TILE_MEM_SIZE);
    sub_(REG5, REG6, REG4);
    getsw_(REG5);
    andi_(REG5, REG5, 0x02);
    addi_(REG6, REG0, 0x02);
    if (beq_(REG5, REG6, 0)) {
        goto flush_page;
    }
    //not a remote address
    br_(0);
    goto next_pte;

flush_page:
    //REG16 holds bytes traversed
    addi_(REG16, REG0, 0);
    //get frame number
    lwi_(REG5, REG17, FRAMEOFFSET);
    //REG7 - points into bitmap
    mul_(REG7, REG10, REG5);
    //now get REG5 to point to base of page in local memory
    muli_(REG5, REG5, 1 << PAGE_SHIFT);
    addi_(REG5, REG5, PAGETABLESLOCAL);
    add_(REG7, REG7, REG9);

start_check_off:
    //REG13 holds single bit
    addi_(REG13, REG0, 0x01);
    //REG12 holds bitmap (64 bits at a time)
    lw_(REG12, REG7, REG0);
    andi_(REG12, REG12, BITMAP_FILTER);

check_next_bit:
    and_(REG14, REG13, REG12);
    if (beq_(REG14, REG0, 0)) {
        goto next_bit;
    }
    addi_(REG15, REG0, BITMAP_BYTES);

    writeOutBytes = proc->getProgramCounter();

write_out_bytes:
    proc->setProgramCounter(writeOutBytes);
    //REG17 holds contents
    lw_(REG17, REG5, REG16);
    sw_(REG17, REG4, REG16);
    subi_(REG15, REG15, sizeof(uint64_t));
    addi_(REG16, REG16, sizeof(uint64_t));
    if (beq_(REG15, REG0, 0)) {
        goto next_bit_no_add;
    }
    br_(0);
    goto write_out_bytes;

next_bit:
    addi_(REG16, REG16, BITMAP_BYTES);
next_bit_no_add:
    shiftli_(REG13, 1);
    if (beq_(REG13, REG0, 0)) {
        //used up all our bits
        goto read_next_bitmap_word;
    }
    br_(0);
    goto check_next_bit;

read_next_bitmap_word:
    subi_(REG15, REG16, 1 << PAGE_SHIFT);
    if (beq_(REG15, REG0, 0)) {
        //have done whole page
        goto next_pte;
    }
    addi_(REG7, REG7, sizeof(uint64_t));
    br_(0);
    goto start_check_off;

next_pte:
    addi_(REG3, REG3, 0x01);
    sub_(REG13, REG2, REG3);
    if (beq_(REG13, REG0, 0)) {
        goto finished_flushing;
    }
    br_(0);
    goto check_page_status;


finished_flushing:
    br_(0);
    proc->flushPagesEnd();
    pop_(REG1);
    br_(0);
    proc->setProgramCounter(proc->getRegister(REG1));
    return;
}

//returns GCD in REG3, return address in REG1
//first number in REG10, second in REG11
void ProcessorFunctor::euclidAlgorithm() const
{
    push_(REG1);
    push_(REG5);
    push_(REG10);
    push_(REG11);
    push_(REG4);
    uint64_t anchor1 = proc->getProgramCounter();
test:
    proc->setProgramCounter(anchor1);
    sub_(REG4, REG10, REG11);
    if (beq_(REG4, REG0, 0)) {
        proc->setProgramCounter(proc->getProgramCounter() +
            sizeof(uint64_t) * 16);
        goto answer;
    }
    getsw_(REG1);
    andi_(REG1, REG1, 0x02);
    addi_(REG3, REG0, 0x02);
    if (beq_(REG1, REG3, 0)) {
        proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t));
        goto swap;
    }
    br_(0);
    proc->setProgramCounter(proc->getProgramCounter() + 4 * sizeof(uint64_t));
    goto divide;
swap:
    push_(REG10);
    push_(REG11);
    pop_(REG10);
    pop_(REG11);
divide:
    div_(REG4, REG10, REG11);
    mul_(REG1, REG4, REG11);
    sub_(REG5, REG10, REG1);
    if (beq_(REG5, REG0, 0)) {
        proc->setProgramCounter(proc->getProgramCounter() +  sizeof(uint64_t));
        goto multiple;
    }
    br_(0);
    proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t));
    goto remainder;
multiple:
    proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t) * 2);
    goto answer;
remainder:
    push_(REG11);
    pop_(REG10);
    push_(REG5);
    pop_(REG11);
    goto test;

answer:
    add_(REG3, REG0, REG11);
    pop_(REG4);
    pop_(REG11);
    pop_(REG10);
    pop_(REG5);
    pop_(REG1);
    br_(0); //simulate return
    proc->setProgramCounter(proc->getRegister(REG1));
    return;
}

//return address in REG1
//REG2 tells us which line
void ProcessorFunctor::normaliseLine() const
{
    cout << "Normalising line " << (proc->getRegister(REG2) & 0xFF) << endl;

    //reset processor count to zero
    swi_(REG0, REG0, 0x110);
    push_(REG1);
    addi_(REG1, REG0, proc->getProgramCounter());
    flushPages();
    pop_(REG1);

    //copy REG2 to REG30;
    push_(REG2);
    pop_(REG30);
    andi_(REG30, REG30, 0xFF);
    push_(REG1);
    //read in the data
    addi_(REG1, REG0, SETSIZE + 1);
    addi_(REG2, REG0, APNUMBERSIZE);
    add_(REG3, REG0, REG30);
    //REG28 takes offset on line
    muli_(REG28, REG30, (APNUMBERSIZE * 2 + 1) * sizeof(uint64_t));
    //REG29 takes offset to line
    muli_(REG29, REG30, ((APNUMBERSIZE * 2 + 1) * sumCount) * sizeof(uint64_t));
    //REG4 takes address of start of numbers
    lwi_(REG4, REG0, sizeof(uint64_t) * 2);
    add_(REG4, REG4, REG29);
    add_(REG4, REG4, REG28);
    //REG5 reads in first 64 bit word - sign etc
    lw_(REG5, REG0, REG4);
    //set REG6 to 1
    addi_(REG6, REG0, 1);
    //read first number
    lwi_(REG7, REG4, sizeof(uint64_t));
    //convert number to 1
    swi_(REG6, REG4, sizeof(uint64_t));
    //increment loop counter
    addi_(REG3, REG0, 1);
    //set REG6 to sign
    //and store positive sign
    andi_(REG6, REG5, 0xFF);
    xor_(REG9, REG6, REG6);
    andi_(REG5, REG5, 0xFFFFFFFFFFFFFF00);
    or_(REG5, REG5, REG9);
    sw_(REG5, REG0, REG4);

    uint64_t anchor1 = proc->getProgramCounter();
loop1:
    proc->setProgramCounter(anchor1);
    //point REG9 to offset to next number sign block
    muli_(REG9, REG3, (APNUMBERSIZE * 2 + 1) * sizeof(uint64_t));
    //load sign block
    lw_(REG10, REG9, REG4);

    //xor sign blocks
    andi_(REG11, REG10, 0xFF);
    xor_(REG11, REG11, REG6);
    andi_(REG10, REG10, 0xFFFFFFFFFFFFFF00);
    or_(REG10, REG10, REG11);
    sw_(REG10, REG9, REG4);

    //now denominator offset
    addi_(REG8, REG9, (APNUMBERSIZE + 1) * sizeof(uint64_t));

    //load number
    addi_(REG9, REG9, sizeof(uint64_t));
    lw_(REG10, REG9, REG4);
    if (beq_(REG10, REG0, 0)) {
        proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t));
        goto zero;
    }
    br_(0);
    proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t) * 2);
    goto notzero;
zero:
    addi_(REG11, REG0, 1);
    br_(0);
    proc->setProgramCounter(proc->getProgramCounter() + 9 * sizeof(uint64_t));
    goto store;

notzero:
    //process
    add_(REG11, REG0, REG7);
    push_(REG3);
    push_(REG1);
    addi_(REG1, REG0, proc->getProgramCounter());
    br_(0);
    euclidAlgorithm();
    pop_(REG1);
    //calculate
    div_(REG10, REG10, REG3);
    div_(REG11, REG7, REG3);
    pop_(REG3);

store:
    //store
    sw_(REG10, REG9, REG4);
    sw_(REG11, REG4, REG8);
    addi_(REG3, REG3, 1);
    if (beq_(REG3, REG1, 0)) {
        proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t));
        goto ending;
    }
    br_(0);
    goto loop1;
ending:
    pop_(REG1);
    br_(0);
    proc->setProgramCounter(proc->getRegister(REG1));
    return;
}

//'interrupt' function to force clean read of a page
// - dumps the page (not flushed) and reads address in
//REG3, returning value in REG4
//REG1 holds return address
void ProcessorFunctor::forcePageReload() const
{
    push_(REG1);
    //Enter interrupt context
    //find page, wipe TLB and page table,
    //then exit interrupt and read address
    uint64_t reloadStartAddress = proc->getProgramCounter();
    proc->flushPagesStart();
    //REG6 holds page address
    andi_(REG6, REG3, PAGE_ADDRESS_MASK);
    //walk page table
    //REG2 counts number of pages
    addi_(REG2, REG0, TILE_MEM_SIZE >> PAGE_SHIFT);
    //REG5 holds pages done so far
    add_(REG5, REG0, REG0);
    uint64_t walking_the_table = proc->getProgramCounter();

table_walk:
    proc->setProgramCounter(walking_the_table);
    muli_(REG12, REG5, PAGETABLEENTRY);
    lwi_(REG11, REG12, PAGETABLESLOCAL + VOFFSET + (1 << PAGE_SHIFT));
    if (beq_(REG11, REG6, 0)) {
        goto matched_page;
    }

walk_next_page:
    addi_(REG5, REG5, 1);
    if (beq_(REG5, REG2, 0)) {
        goto page_walk_done;
    }
    br_(0);
    goto table_walk;
 
matched_page:
    lwi_(REG11, REG12, PAGETABLESLOCAL + FLAGOFFSET + (1 << PAGE_SHIFT));
    andi_(REG13, REG11, 0x01);
    if (beq_(REG13, REG0, 0)) {
        goto walk_next_page;
    }
    andi_(REG11, REG11, 0xFFFFFFFFFFFFFFFE);
    swi_(REG11, REG12, PAGETABLESLOCAL + FLAGOFFSET + (1 << PAGE_SHIFT));
    //dump the page - ie wipe the bitmap
    proc->dumpPageFromTLB(proc->getRegister(REG6));

page_walk_done:
    proc->flushPagesEnd();
    lw_(REG4, REG3, REG0);
    pop_(REG1);
    br_(0);
    proc->setProgramCounter(proc->getRegister(REG1));
    nop_();
}

void ProcessorFunctor::operator()()
{
    uint64_t hangingPoint, waitingOnZero;
    uint64_t loopingWaitingForProcessorCount;
    uint64_t waitingForTurn;
    uint64_t normaliseTickDown;
    uint64_t holdingPoint;
    uint64_t tickReadingDown;
    uint64_t testNextRound;
    const uint64_t order = tile->getOrder();
    Tile *masterTile = proc->getTile();
    if (order >= SETSIZE) {
        return;
    }
    proc->start();
    //REG15 holds count
    add_(REG15, REG0, REG0);
    addi_(REG1, REG0, 0x1);
    setsw_(REG1);
    //initial commands
    addi_(REG1, REG0, 0xFF00);
    swi_(REG1, REG0, 0x100);
    addi_(REG1, REG0, 0xFF);
    swi_(REG1, REG0, 0x110);
    add_(REG1, REG0, REG0);
    swi_(REG1, REG0, 0x120);
    addi_(REG1, REG0, proc->getProgramCounter());
    flushPages();
    //store processor number
    addi_(REG1, REG0, proc->getNumber());
    swi_(REG1, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3);

    const uint64_t readCommandPoint = proc->getProgramCounter();

read_command:
    proc->setProgramCounter(readCommandPoint);
    lwi_(REG1, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3);    
    addi_(REG3, REG0, 0x110);
    push_(REG1);
    addi_(REG1, REG0, proc->getProgramCounter());
    br_(0);
    forcePageReload();
    pop_(REG1);
    addi_(REG3, REG0, 0xFF);
    if (beq_(REG3, REG4, 0)) {
        goto keep_reading_command;
    }

    addi_(REG30, REG0, 0x101);
    sub_(REG30, REG30, REG1);

    //wait longer if we have low processor number
    muli_(REG3, REG30, 0x17);
    tickReadingDown = proc->getProgramCounter();
tick_read_down:
    proc->setProgramCounter(tickReadingDown);
    nop_();
    subi_(REG3, REG3, 0x01);
    if (beq_(REG3, REG0, 0)) {
	goto read_command;
    }
    br_(0);
    goto tick_read_down;

keep_reading_command:
    lwi_(REG2, REG0, 0x100);
    //REG3 holds signal
    andi_(REG3, REG2, 0xFF00);
    //REG7 holds signal to match against
    addi_(REG7, REG0, 0xFF00);
    //REG4 holds register
    andi_(REG4, REG2, 0xFF);
 
    //test for signal
    if (beq_(REG3, REG7, 0)) {
        goto test_for_processor;
    }
    br_(0);
    goto wait_for_next_signal;

test_for_processor:
    if (beq_(REG4, REG1, 0)) {
        goto normalise_line;
    }
    br_(0);
    goto wait_for_next_signal;

normalise_line:
    addi_(REG30, REG1, 0x01);
    swi_(REG30, REG0, 0x120);
    push_(REG1);
    push_(REG15);
    br_(0);
    addi_(REG1, REG), proc->getProgramCounter());
    normaliseLine();
    addi_(REG1, REG0, proc->getProgramCounter());
    br_(0);
    flushPages();
    pop_(REG15);
    add_(REG3, REG0, REG15);
    ori_(REG3, REG3, 0xFE00);
    push_(REG15);
    swi_(REG3, REG0, 0x100);
    addi_(REG1, REG0, proc->getProgramCounter()); 
    br_(0);
    flushPages();
    //wait 0x10000 ticks
    addi_(REG1, REG0, 0x10001);
    normaliseTickDown = proc->getProgramCounter();

normalise_tick_down:
    proc->setProgramCounter(normaliseTickDown);
    subi_(REG1, REG1, 0x01);
    if (beq_(REG1, REG0, 0)) {
        goto ready_for_next_normalisation;
    }
    br_(0);
    goto normalise_tick_down;

ready_for_next_normalisation:
    pop_(REG15);
    pop_(REG1);
    br_(0);
    goto prepare_to_normalise_next;

wait_for_next_signal:
    push_(REG15);
    //try a back off
    addi_(REG5, REG0, 0x10);
    addi_(REG6, REG0, 0x1000);
    waitingOnZero = proc->getProgramCounter();

wait_on_zero:
    proc->setProgramCounter(waitingOnZero);
    addi_(REG3, REG0, 0x100);
    addi_(REG1, REG0, proc->getProgramCounter());
    br_(0);
    forcePageReload(); //reads address in REG3, returning in REG4
    push_(REG4);
    andi_(REG4, REG4, 0xFF00);
    addi_(REG8, REG0, 0xFE00);
    if (beq_(REG8, REG4, 0)) {
        goto calculate_next;
    }
    pop_(REG4);

    //do the back off wait
    add_(REG7, REG0, REG5);
    hangingPoint = proc->getProgramCounter();
just_hanging_around:
    proc->setProgramCounter(hangingPoint);
    nop_();
    subi_(REG7, REG7, 1);
    if (beq_(REG7, REG0, 0)) {
        goto back_off_handler;
    }
    br_(0);
    goto just_hanging_around;

back_off_handler:
    shiftl_(REG5);
    sub_(REG7, REG6, REG5);
    if (beq_(REG7, REG0, 0)) {
        goto back_off_reset;
    }
    br_(0);
    goto wait_on_zero;

back_off_reset:
    addi_(REG5, REG0, 0x01);
    goto wait_on_zero;

calculate_next:
    pop_(REG4);
    andi_(REG4, REG4, 0xFF);
    if (beq_(REG4, REG15, 0)) {
	goto on_to_next_round;
    }
    br_(0);
    goto wait_on_zero;

on_to_next_round:
    lwi_(REG1, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3);
    addi_(REG31, REG0, 0x01);
    muli_(REG30, REG1, 0x400);

    testNextRound = proc->getProgramCounter();
test_next_round:
    proc->setProgramCounter(testNextRound);
    nop_();
    sub_(REG30, REG30, REG31);
    if (beq_(REG30, REG0, 0)) {
	goto do_next_round;
    }
    br_(0);
    goto test_next_round;

do_next_round:
    nextRound();

get_REG15_back:
    pop_(REG15);
prepare_to_normalise_next:
    cout << "Pass " << proc->getRegister(REG15) << " on processor " << proc->getRegister(REG1) << " complete." << endl;
    if (beq_(REG15, REG1, 0)) {
        goto work_here_is_done;
    }
    addi_(REG15, REG15, 0x1);
    //construct next signal

    waitingForTurn = proc->getProgramCounter();

wait_for_turn_to_complete:
    proc->setProgramCounter(waitingForTurn);
    push_(REG3);
    push_(REG4);
    addi_(REG3, REG0, 0x110);
    addi_(REG1, REG0, proc->getProgramCounter());
    forcePageReload();
    lwi_(REG1, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3);
    push_(REG5);
    addi_(REG5, REG4, 0x01);
    if (beq_(REG5, REG1, 0)) {
        goto write_out_next_processor;
    }
    sub_(REG5, REG1, REG4);
    //delay loop dependent on how much further we have to go
    muli_(REG5, REG5, 0x13);
    loopingWaitingForProcessorCount = proc->getProgramCounter();

loop_wait_processor_count:
    proc->setProgramCounter(loopingWaitingForProcessorCount);
    nop_();
    subi_(REG5, REG5, 0x01);
    if (beq_(REG5, REG0, 0)) {
	goto ready_to_loop_again;
    }
    br_(0);
    goto loop_wait_processor_count;

ready_to_loop_again:
    pop_(REG5);
    pop_(REG4);
    pop_(REG3);
    br_(0);
    goto wait_for_turn_to_complete;

write_out_next_processor:
    subi_(REG5, REG1, sumCount - 1);
    if (beq_(REG5, REG0, 0)) {
        goto write_processor_back_to_zero;
    }
    br_(0);
    goto write_out_next_processor_A;

write_processor_back_to_zero:
    swi_(REG0, REG0, 0x120);

write_out_next_processor_A:
    swi_(REG1, REG0, 0x110);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    flushPages();
    pop_(REG5);
    pop_(REG4);
    pop_(REG3);

    addi_(REG20, REG0, 0xFF00);
    or_(REG20, REG20, REG15);
    swi_(REG20, REG0, 0x100);
    push_(REG15);
    push_(REG1);
    addi_(REG1, REG0, proc->getProgramCounter());
    br_(0);
    flushPages();
    pop_(REG1);
    pop_(REG15);
    cout << "sending signal " << hex << proc->getRegister(REG20) << " from " << dec << proc->getNumber() << endl;

    //count down to avoid flooding memory net
    addi_(REG30, REG0, 0x100);
    holdingPoint = proc->getProgramCounter();
hold_on_a_while:
    proc->setProgramCounter(holdingPoint);
    nop_();
    subi_(REG30, REG30, 0x01);
    if (beq_(REG30, REG0, 0)) {
	goto moving_on;
    }
    br_(0);
    goto hold_on_a_while;

moving_on:
    br_(0);
    goto read_command;
    //construct next signal

work_here_is_done:
    swi_(REG1, REG0, 0x110);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter()); 
    flushPages();
    masterTile->getBarrier()->decrementTaskCount();
    cout << " - our work here is done - " << proc->getRegister(REG1) << endl;
    //some C++ to write out normalised line
    uint64_t myProcessor = proc->getRegister(REG1);
    uint64_t numberSize = (APNUMBERSIZE * 2 + 1) * sizeof(uint64_t);
    uint64_t lineSize = numberSize * 0x101;
    startingPoint = masterTile->readLong(sizeof(uint64_t) * 2);
    for (int i = 0; i < 0x100; i++) {
	uint64_t position = lineSize * myProcessor + i * numberSize;
        cout << masterTile->readLong(position);
        cout << "/";
        cout << masterTile->readLong(position +
            (APNUMBERSIZE + 1) * sizeof(uint64_t));
        cout << ",";
    }
    cout << endl;
 }

//this function just to break code up
void ProcessorFunctor::nextRound() const
{
    uint64_t beforeCallEuclid;
    uint64_t beforeSecondCallEuclid;
    //calculate factor for this line
    //REG1 - hold processor number
    //REG12 - the 'top' line
    lwi_(REG1, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3);
    cout << "Processor " << proc->getRegister(REG1) << " with base line " << proc->getRegister(REG12) << endl;
    if (beq_(REG1, REG12, 0)) {
        return;
    }
    //REG2 - size of each number
    addi_(REG2, REG0, (APNUMBERSIZE * 2 + 1) * sizeof(uint64_t));
    //REG11 - length of a line
    muli_(REG11, REG2, sumCount);
    //REG9 is offset to start of reference line
    mul_(REG9, REG12, REG11);
    //REG16 is offset to first number to test on line
    mul_(REG16, REG12, REG2);
    //REG3 - points to start of numbers
    lwi_(REG3, REG0, sizeof(uint64_t) * 2);
    //REG29 points to first number to use in our reference line
    add_(REG29, REG0, REG3);
    add_(REG29, REG29, REG9);
    add_(REG29, REG29, REG16);
    //REG4 - point to start of this processor's numbers
    mul_(REG4, REG11, REG1);
    add_(REG4, REG4, REG3);
    add_(REG4, REG4, REG16);
    //REG5 takes sign of first number
    lw_(REG5, REG4, REG0);
    andi_(REG5, REG5, 0xFF);
    //REG6 takes numerator
    //REG7 takes demoninator
    lwi_(REG6, REG4, sizeof(uint64_t));
    lwi_(REG7, REG4, (APNUMBERSIZE + 1) * sizeof(uint64_t));
    
    //next set of numbers
    //REG13 progress
    //REG14 limit
    add_(REG13, REG0, REG12);
    addi_(REG14, REG0, sumCount);
    //now loop through all the numbers

    const uint64_t nextRoundLoopStart = proc->getProgramCounter();
    //REG17 holds offset
    //REG18 holds calculated position on zero line
    //REG19 holds calculated position on processor line
 next_round_loop_start:
    proc->setProgramCounter(nextRoundLoopStart);
    muli_(REG17, REG13, (APNUMBERSIZE * 2 + 1) * sizeof(uint64_t));

    //fetch 'bottom' row number
    add_(REG19, REG4, REG17);
    lw_(REG20, REG19, REG0);
    andi_(REG20, REG20, 0xFF);
    lwi_(REG21, REG19, sizeof(uint64_t));
    lwi_(REG22, REG19, (APNUMBERSIZE + 1) * sizeof(uint64_t));

    if (beq_(REG6, REG0, 0)) {
        goto next_round_prepare_to_save;
    }

    //fetch 'top' row number
    add_(REG18, REG29, REG17);
    lw_(REG23, REG18, REG0);
    andi_(REG23, REG23, 0xFF);
    lwi_(REG24, REG18, sizeof(uint64_t));
    lwi_(REG25, REG18, (APNUMBERSIZE + 1) * sizeof(uint64_t));

    if (beq_(REG24, REG0, 0)) {
        goto next_round_prepare_to_save;
    }

    //calculate number to subtract
    xor_(REG26, REG5, REG23);
    mul_(REG27, REG6, REG24);
    mul_(REG28, REG7, REG25);

    push_(REG3);
    push_(REG1);
    push_(REG10);
    push_(REG11);

    add_(REG10, REG27, REG0);
    add_(REG11, REG28, REG0);
    beforeCallEuclid = proc->getProgramCounter();
    addi_(REG1, REG0, beforeCallEuclid + sizeof(uint64_t) * 3);
    br_(0);
    euclidAlgorithm();

    pop_(REG11);
    pop_(REG10);
    pop_(REG1);

    div_(REG27, REG27, REG3);
    div_(REG28, REG28, REG3);
    pop_(REG3);

    mul_(REG21, REG21, REG28);
    mul_(REG27, REG27, REG22);
    mul_(REG22, REG22, REG28);

    andi_(REG30, REG26, 0x01);
    addi_(REG31, REG0, 0x01);
    if (beq_(REG30, REG31, 0)) {
	goto add_not_subtract;
    }
    andi_(REG30, REG20, 0x01);
    if (beq_(REG30, REG31, 0)) {
	goto reverse_add;
    }
    //first is positive, second is negative
    //subtract, but change sign on overflow
do_subtract:
    sub_(REG21, REG21, REG27);
    getsw_(REG30);
    andi_(REG30, REG30, 0x02);
    addi_(REG31, REG31, 0x02);
    if (beq_(REG30, REG31, 0)) {
	goto sign_reversal;
    }
    br_(0);
    goto next_round_euclid_again;

sign_reversal:
    addi_(REG20, REG20, 0x01);
    br_(0);
    goto next_round_euclid_again;


add_not_subtract:
    //second is positive (after minus)
    //check sign of first
    //if neg then take first from second
    //otherwise add first and second
    andi_(REG30, REG20, 0x01);
    addi_(REG31, REG0, 0x01);
    if (beq_(REG30, REG31, 0)) {
	goto sub_first_from_second;
    }
    add_(REG21, REG21, REG27);
    br_(0);
    goto next_round_euclid_again;

sub_first_from_second:
    push_(REG27);
    push_(REG21);
    pop_(REG27);
    pop_(REG21);
    addi_(REG30, REG0, 0x01);
    xor_(REG20, REG20, REG30);
    br_(0);
    goto do_subtract;

reverse_add:
    //first is negative and so is second (after minus)
    //add both as negatives
    add_(REG21, REG21, REG27);
 
next_round_euclid_again:
    if (beq_(REG21, REG0, 0)) {
        goto next_round_prepare_to_save;
    }
    push_(REG3);
    push_(REG10);
    push_(REG11);
    push_(REG1);
    add_(REG10, REG21, REG0);
    add_(REG11, REG22, REG0);
    beforeSecondCallEuclid = proc->getProgramCounter();
    addi_(REG1, REG0, beforeSecondCallEuclid + sizeof(uint64_t) * 2);
    br_(0);
    euclidAlgorithm();
    pop_(REG1);
    pop_(REG11);
    pop_(REG10);
    div_(REG21, REG21, REG3);
    div_(REG22, REG22, REG3);
    pop_(REG3);


next_round_prepare_to_save:
    lw_(REG30, REG19, REG0);
    andi_(REG30, REG30, 0xFFFFFFFFFFFFFF00);
    or_(REG20, REG30, REG20);
    sw_(REG20, REG19, REG0);
    swi_(REG21, REG19, sizeof(uint64_t));
    swi_(REG22, REG19, (APNUMBERSIZE + 1) * sizeof(uint64_t));
    cout << "Stored: " << proc->getRegister(REG21) << "/" << proc->getRegister(REG22) << " : " << proc->getRegister(REG1) << ":" << proc->getRegister(REG12) << ":" << proc->getRegister(REG13) << endl;
    addi_(REG13, REG13, 0x01);
    sub_(REG30, REG14, REG13);
    if (beq_(REG30, REG0, 0)) {
        goto next_round_over;
    }
    br_(0);
    goto next_round_loop_start;

next_round_over:
    nop_();
}
