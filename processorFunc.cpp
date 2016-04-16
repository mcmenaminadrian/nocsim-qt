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
static const uint64_t PAGE_ADDRESS_MASK = 0xFFFFFFFFFFFFF800;

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


//drop all the pages except current code
//return address in REG1
void ProcessorFunctor::cleanCaches() const
{
    push_(REG1);
    br_(0);
    proc->flushPagesStart();
    //REG1 points to start of page table
    addi_(REG1, REG0, PAGETABLESLOCAL + (1 << PAGE_SHIFT));
    //REG2 total pages
    addi_(REG2, REG0, TILE_MEM_SIZE >> PAGE_SHIFT);
    //REG4 - how many pages we have checked
    add_(REG4, REG0, REG0);
    //constants
    addi_(REG5, REG0, PAGETABLEENTRY);
    addi_(REG6, REG0, FLAGOFFSET);
    addi_(REG7, REG0, VOFFSET);
    addi_(REG21, REG0, 0x02);
    addi_(REG22, REG0, 0x01);

    uint64_t roundCacheLoop = proc->getProgramCounter();
 round_cache_loop:
    proc->setProgramCounter(roundCacheLoop);
    //REG8 offset in page table
    mul_(REG8, REG4, REG5);
    //check validity
    add_(REG9, REG8, REG6);
    add_(REG9, REG9, REG1);
    lw_(REG10, REG9, REG0);
    //is page valid?
    and_(REG11, REG10, REG22);
    if (beq_(REG11, REG0, 0)) {
        goto check_next_page_clean;
    }
    //is page flushable?
    and_(REG11, REG10, REG21);
    if (beq_(REG11, REG0, 0)) {
        goto check_page_address_clean;
    }
    br_(0);
    goto check_next_page_clean;

 check_page_address_clean:
    add_(REG9, REG8, REG7);
    add_(REG9, REG9, REG1);
    lw_(REG10, REG9, REG0);
    addi_(REG3, REG0, proc->getProgramCounter() + sizeof(uint64_t));
    andi_(REG3, REG3, PAGE_ADDRESS_MASK);
    sub_(REG11, REG10, REG3);
    if (beq_(REG11, REG0, 0)) {
        goto check_next_page_clean;
    }
    br_(0);
    goto clean_selected_page;

 check_next_page_clean:
    add_(REG4, REG4, REG22);
    sub_(REG11, REG2, REG4);
    if (beq_(REG11, REG0, 0)) {
        goto finish_cleaning_selected;
    }
    br_(0);
    goto round_cache_loop;

 clean_selected_page:
    push_(REG4);
    proc->dropPage(proc->getRegister(REG4));
    pop_(REG4);
    goto check_next_page_clean;

 finish_cleaning_selected:
     br_(0);
     proc->flushPagesEnd();
     pop_(REG1);
     proc->setProgramCounter(proc->getRegister(REG1));
     br_(0);
}

//drop the page referenced in REG3
//return address in REG1
void ProcessorFunctor::dropPage() const
{
    push_(REG1);
    br_(0);
    proc->flushPagesStart();
    //REG1 points to start of page table
    addi_(REG1, REG0, PAGETABLESLOCAL + (1 << PAGE_SHIFT));
    //REG3 page we are looking for
    andi_(REG3, REG3, PAGE_ADDRESS_MASK);
    //REG2 total pages
    addi_(REG2, REG0, TILE_MEM_SIZE >> PAGE_SHIFT);
    //REG4 - how many pages we have checked
    add_(REG4, REG0, REG0);
    //constants
    addi_(REG5, REG0, PAGETABLEENTRY);
    addi_(REG6, REG0, FLAGOFFSET);
    addi_(REG7, REG0, VOFFSET);
    addi_(REG21, REG0, 0x02);
    addi_(REG22, REG0, 0x01);

    uint64_t keepCheckingDropSelected = proc->getProgramCounter();
 keep_checking_drop_selected:
    proc->setProgramCounter(keepCheckingDropSelected);
    //REG8 offset in page table
    mul_(REG8, REG4, REG5);
    //check validity
    add_(REG9, REG8, REG6);
    add_(REG9, REG9, REG1);
    lw_(REG10, REG9, REG0);
    //is page valid?
    and_(REG11, REG10, REG22);
    if (beq_(REG11, REG0, 0)) {
        goto check_next_page_drop;
    }
    //is page flushable?
    and_(REG11, REG10, REG21);
    if (beq_(REG11, REG0, 0)) {
        goto check_page_address_drop;
    }
    br_(0);
    goto check_next_page_drop;

 check_page_address_drop:
    add_(REG9, REG8, REG7);
    add_(REG9, REG9, REG1);
    lw_(REG10, REG9, REG0);
    sub_(REG11, REG10, REG3);
    if (beq_(REG11, REG0, 0)) {
        goto drop_selected_page;
    }

 check_next_page_drop:
    add_(REG4, REG4, REG22);
    sub_(REG11, REG2, REG4);
    if (beq_(REG11, REG0, 0)) {
        goto finish_dropping_selected;
    }
    br_(0);
    goto keep_checking_drop_selected;

 drop_selected_page:
    proc->dropPage(proc->getRegister(REG4));

 finish_dropping_selected:
     br_(0);
     proc->flushPagesEnd();
     pop_(REG1);
     proc->setProgramCounter(proc->getRegister(REG1));
     br_(0);
}

//flush the page referenced in REG3
//return address in REG1 
void ProcessorFunctor::flushSelectedPage() const
{
   push_(REG1);
   br_(0);
   proc->flushPagesStart();
   //REG1 points to start of page table
   addi_(REG1, REG0, PAGETABLESLOCAL + (1 << PAGE_SHIFT));
   //REG3 page we are looking for
   andi_(REG3, REG3, PAGE_ADDRESS_MASK);
   //REG2 total pages
   addi_(REG2, REG0, TILE_MEM_SIZE >> PAGE_SHIFT);
   //REG4 - how many pages we have checked
   add_(REG4, REG0, REG0);
   //constants
   addi_(REG5, REG0, PAGETABLEENTRY);
   addi_(REG6, REG0, FLAGOFFSET);
   addi_(REG7, REG0, VOFFSET);
   addi_(REG21, REG0, 0x02);
   addi_(REG22, REG0, 0x01);

   uint64_t keepCheckingFlushSelected = proc->getProgramCounter();
keep_checking_flush_selected:
   proc->setProgramCounter(keepCheckingFlushSelected);
   //REG8 offset in page table
   mul_(REG8, REG4, REG5);
   //check validity
   add_(REG9, REG8, REG6);
   add_(REG9, REG9, REG1);
   lw_(REG10, REG9, REG0);
   //is page valid?
   and_(REG11, REG10, REG22);
   if (beq_(REG11, REG0, 0)) {
       goto check_next_page;
   }
   //is page flushable?
   and_(REG11, REG10, REG21);
   if (beq_(REG11, REG0, 0)) {
       goto check_page_address;
   }
   br_(0);
   goto check_next_page;

check_page_address:
   add_(REG9, REG8, REG7);
   add_(REG9, REG9, REG1);
   lw_(REG10, REG9, REG0);
   sub_(REG11, REG10, REG3);
   if (beq_(REG11, REG0, 0)) {
       goto flush_selected_page;
   }

check_next_page:
   add_(REG4, REG4, REG22);
   sub_(REG11, REG2, REG4);
   if (beq_(REG11, REG0, 0)) {
       goto finish_flushing_selected;
   }
   br_(0);
   goto keep_checking_flush_selected;

flush_selected_page:
   proc->writeBackMemory(proc->getRegister(REG4));

finish_flushing_selected:
    br_(0);
    proc->flushPagesEnd();
    pop_(REG1);
    proc->setProgramCounter(proc->getRegister(REG1));
    br_(0);
}

//return address in REG1
void ProcessorFunctor::flushPages() const
{
    push_(REG1);
    br_(0);
    proc->flushPagesStart();
    //REG1 points to start of page table
    addi_(REG1, REG0, PAGETABLESLOCAL + (1 << PAGE_SHIFT));
    //REG2 counts number of pages
    addi_(REG2, REG0, TILE_MEM_SIZE >> PAGE_SHIFT);
    //REG3 holds pages done so far
    add_(REG3, REG0, REG0);
    addi_(REG29, REG0, 0x02); //constant
    addi_(REG21, REG0, 0x01); //constant
    uint64_t aboutToCheck = proc->getProgramCounter();

check_page_status:
    proc->setProgramCounter(aboutToCheck);
    muli_(REG5, REG3, PAGETABLEENTRY);
    add_(REG17, REG1, REG5);
    //REG4 holds flags    
    lwi_(REG4, REG17, FLAGOFFSET);
    //don't flush an unmovable page
    and_(REG30, REG4, REG29);
    if (beq_(REG30, REG29, 0)) {
        goto next_pte;
    }
    //don't flush an invalid page
    and_(REG4, REG4, REG21);
    if (beq_(REG4, REG0, 0)) {
        goto next_pte;
    }
    //load virtual address in REG4
    lwi_(REG4, REG17, VOFFSET);
    //test if it is remote
    subi_(REG5, REG4, PAGETABLESLOCAL);
    getsw_(REG5);
    and_(REG5, REG5, REG29);
    if (beq_(REG5, REG29, 0)) {
        goto flush_page;
    }
    addi_(REG6, REG0, PAGETABLESLOCAL + TILE_MEM_SIZE);
    sub_(REG5, REG6, REG4);
    getsw_(REG5);
    and_(REG5, REG5, REG29);
    if (beq_(REG5, REG29, 0)) {
        goto flush_page;
    }
    //not a remote address
    br_(0);
    goto next_pte;

flush_page:
    //get frame number
    lwi_(REG5, REG17, FRAMEOFFSET);
    proc->writeBackMemory(proc->getRegister(REG5));

next_pte:
    add_(REG3, REG3, REG21);
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
    push_(REG9);
    addi_(REG9, REG0, 0x02); //constant
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
    and_(REG1, REG1, REG9);
    add_(REG3, REG0, REG9);
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
    pop_(REG9);
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
    cout << "Normalising line " << (proc->getRegister(REG2) & 0xFF);
    cout <<" - ticks: " << proc->getTicks() << endl;

    push_(REG1);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());

    //copy REG2 to REG30;
    add_(REG30, REG0, REG2);
    andi_(REG30, REG30, 0xFF);
    //read in the data
    //some constants
    addi_(REG1, REG0, SETSIZE + 1);
    addi_(REG2, REG0, sizeof(uint64_t));
    add_(REG3, REG0, REG30);
    //REG28 takes offset on line
    muli_(REG28, REG30, (APNUMBERSIZE * 2 + 1) * sizeof(uint64_t));
    //REG29 takes offset to line
    muli_(REG29, REG30, ((APNUMBERSIZE * 2 + 1) * sumCount)
		* sizeof(uint64_t));
    //REG4 takes address of start of numbers
    lwi_(REG4, REG0, sizeof(uint64_t) * 2);
    add_(REG4, REG4, REG29);
    add_(REG4, REG4, REG28);
    //REG5 reads in first 64 bit word - sign etc
    lw_(REG5, REG0, REG4);
    //set REG6 to 1
    addi_(REG6, REG0, 1);
    //read first numerator
    lw_(REG7, REG4, REG2);
    //read first denominator
    lwi_(REG19, REG4, (APNUMBERSIZE + 1) * sizeof(uint64_t)); 
    //convert number to 1
    sw_(REG6, REG4, REG2);
    swi_(REG6, REG4, (APNUMBERSIZE + 1) * sizeof(uint64_t));
    //increment loop counter
    add_(REG3, REG30, REG6);
    //set REG6 to sign
    //and store positive sign
    andi_(REG6, REG5, 0xFF);
    xor_(REG9, REG6, REG6);
    andi_(REG5, REG5, 0xFFFFFFFFFFFFFF00);
    or_(REG5, REG5, REG9);
    sw_(REG5, REG0, REG4);

    //correct for start of rest of numbers
    sub_(REG4, REG4, REG28);

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
    add_(REG9, REG9, REG2);
    lw_(REG10, REG9, REG4);
    if (beq_(REG10, REG0, 0)) {
        proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t));
        goto zero;
    }
    mul_(REG10, REG10, REG19);
    br_(0);
    proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t) * 3);
    goto notzero;

zero:
    addi_(REG11, REG0, 1);
    //set sign as positive
    sub_(REG9, REG9, REG2);
    lw_(REG31, REG9, REG4);
    andi_(REG31, REG31, 0xFFFFFFFFFFFFFF00);
    sw_(REG31, REG9, REG4);
    add_(REG9, REG9, REG2);
    br_(0);
    proc->setProgramCounter(proc->getProgramCounter() + 9 * sizeof(uint64_t));
    goto store;

notzero:
    //process
    lw_(REG11, REG4, REG8);
    mul_(REG11, REG11, REG7);
    push_(REG3);
    push_(REG1);
    addi_(REG1, REG0, proc->getProgramCounter());
    br_(0);
    euclidAlgorithm();
    pop_(REG1);
    //calculate
    div_(REG10, REG10, REG3);
    div_(REG11, REG11, REG3);
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
    br_(0);
    //flush results to global memory
    addi_(REG1, REG0, proc->getProgramCounter());
    flushPages();
    //update processor count
    lwi_(REG30, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3); 
    swi_(REG30, REG0, 0x110);
    addi_(REG3, REG0, 0x110);
    addi_(REG1, REG0, proc->getProgramCounter());
    br_(0);
    flushSelectedPage();
    pop_(REG1);
    br_(0);
    proc->setProgramCounter(proc->getRegister(REG1));
    return;
}

//'interrupt' function to force clean read of a page
//REG3 address targeted, returning value in REG4
//REG1 holds return address
void ProcessorFunctor::forcePageReload() const
{
    push_(REG1);
    //Enter interrupt context
    //find page, wipe TLB and page table,
    //then exit interrupt and read address
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
    //found page so get rid of it
    proc->dropPage(proc->getRegister(REG5));

page_walk_done:
    proc->flushPagesEnd();
    //force page back in
    lw_(REG4, REG3, REG0);
    pop_(REG1);
    br_(0);
    proc->setProgramCounter(proc->getRegister(REG1));
}

void ProcessorFunctor::operator()()
{
    uint64_t hangingPoint, waitingOnZero;
    uint64_t loopingWaitingForProcessorCount;
    uint64_t waitingForTurn;
    uint64_t tickReadingDown;
    uint64_t normaliseDelayLoop;
    uint64_t shortDelayLoop;
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
    addi_(REG1, REG0, SETSIZE);
    swi_(REG1, REG0, 0x110);
    addi_(REG3, REG0, 0x100);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    flushSelectedPage();
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    dropPage();
    //store processor number
    addi_(REG1, REG0, proc->getNumber());
    swi_(REG1, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3);

    const uint64_t readCommandPoint = proc->getProgramCounter();
read_command:
    proc->setProgramCounter(readCommandPoint);
    lwi_(REG1, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3);    
    addi_(REG3, REG0, 0x110);
    push_(REG1);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    forcePageReload();
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    dropPage();
    pop_(REG1);
    addi_(REG3, REG0, SETSIZE);
    if (beq_(REG3, REG4, 0)) {
        goto keep_reading_command;
    }

    addi_(REG30, REG0, SETSIZE);
    sub_(REG30, REG30, REG4);

    //wait longer if we have low processor number signalled
    muli_(REG3, REG30, 0x75);
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
    //REG3 holds instruction portion of signal
    andi_(REG3, REG2, 0xFF00);
    //REG7 holds instruction to match against
    addi_(REG7, REG0, 0xFF00);
    //REG4 holds register protion of signal
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
    push_(REG1);
    lwi_(REG1, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3);
    if (beq_(REG1, REG0, 0)) {
        goto now_for_normalise;
    }

    cout << "Waiting to begin normalisation" << endl;
    addi_(REG1, REG0, 0x1000);
    normaliseDelayLoop = proc->getProgramCounter();
wait_for_normalise:
    proc->setProgramCounter(normaliseDelayLoop);
    nop_();
    subi_(REG1, REG1, 1);
    if (beq_(REG1, REG0, 0)) {
        goto now_for_normalise;
    }
    br_(0);
    goto wait_for_normalise;

now_for_normalise:
    push_(REG15);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    normaliseLine();
    pop_(REG15);
    add_(REG3, REG0, REG15);
    ori_(REG3, REG3, 0xFE00);
    push_(REG15);
    swi_(REG3, REG0, 0x100);

    addi_(REG3, REG0, 0x100);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    flushSelectedPage();
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    dropPage();
    pop_(REG15);
    pop_(REG1);
    br_(0);
    goto prepare_to_normalise_next;

wait_for_next_signal:
    cout << "Processor " << proc->getNumber() << " now waiting." << endl;
    push_(REG15);
    //try a back off
    addi_(REG5, REG0, 0x400);
    addi_(REG6, REG0, 0x1000);

    waitingOnZero = proc->getProgramCounter();
wait_on_zero:
    proc->setProgramCounter(waitingOnZero);
    addi_(REG3, REG0, 0x100);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    forcePageReload(); //reads address in REG3, returning in REG4
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    dropPage();
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
    addi_(REG5, REG0, 0x10);
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
    add_(REG12, REG0, REG15);
    push_(REG1);
    nextRound();
    pop_(REG1);
    pop_(REG15);
prepare_to_normalise_next:
    cout << "Pass " << proc->getRegister(REG15) << " on processor " << proc->getRegister(REG1) << " complete";
    cout <<" - ticks: " << proc->getTicks() << endl;
    if (beq_(REG15, REG1, 0)) {
        goto work_here_is_done;
    }
    addi_(REG15, REG15, 0x01);

    waitingForTurn = proc->getProgramCounter();
wait_for_turn_to_complete:
    proc->setProgramCounter(waitingForTurn);
    addi_(REG3, REG0, 0x110);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    forcePageReload();
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    dropPage();
    lwi_(REG1, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3);
    if (beq_(REG4, REG1, 0)) {
        goto write_out_next_processor;
    }
    if (beq_(REG4, REG0, 0)) {
        goto standard_delay;
    }
    sub_(REG4, REG1, REG4);
    muli_(REG4, REG4, 0x45);
    br_(0);
    goto setup_loop_wait_processor_count;

standard_delay:
    addi_(REG4, REG0, 0x750);

setup_loop_wait_processor_count:
    loopingWaitingForProcessorCount = proc->getProgramCounter();
loop_wait_processor_count:
    proc->setProgramCounter(loopingWaitingForProcessorCount);
    nop_();
    subi_(REG4, REG4, 0x01);
    if (beq_(REG4, REG0, 0)) {
        goto wait_for_turn_to_complete;
    }
    br_(0);
    goto loop_wait_processor_count;

write_out_next_processor:
    swi_(REG0, REG0, 0x110);
    addi_(REG20, REG0, 0xFF00);
    or_(REG20, REG20, REG15);
    swi_(REG20, REG0, 0x100);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    push_(REG3);
    addi_(REG3, REG0, 0x100);
    flushSelectedPage();
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    dropPage();
    pop_(REG3);
    cout << "sending signal " << hex << proc->getRegister(REG20) << " from " 
        << dec << proc->getNumber();
    cout <<" - ticks: " << proc->getTicks() << endl;
    br_(0);
    goto read_command;
    //construct next signal

work_here_is_done:
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    flushPages();
    //some C++ to write out normalised line
    uint64_t myProcessor = proc->getNumber();
    uint64_t numberSize = (APNUMBERSIZE * 2 + 1) * sizeof(uint64_t);
    uint64_t lineOffset = numberSize * sumCount * myProcessor;
    uint64_t halfNumber = (APNUMBERSIZE + 1) * sizeof(uint64_t);
    startingPoint = masterTile->readLong(sizeof(uint64_t) * 2);
    for (int i = 0; i < sumCount; i++) {
        uint64_t position = startingPoint + lineOffset + i * numberSize;
        if (masterTile->readLong(position) & 0x01) {
            cout << "-";
        }
        cout << masterTile->readLong(position + sizeof(uint64_t));
        cout << "/";
        cout << masterTile->readLong(position + halfNumber);
        cout << ",";
    }
    cout << endl;
    cout <<"Ticks: " << proc->getTicks() << endl;

    //now scan for completed processes
    addi_(REG21, REG0, SETSIZE);
    addi_(REG22, REG0, 0x01);
    addi_(REG23, REG0, 0x110);
    lwi_(REG10, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3);

    uint64_t completeLoopDone = proc->getProgramCounter();
    uint64_t testProcUpdate;
complete_loop_done:
    proc->setProgramCounter(completeLoopDone);
    add_(REG10, REG10, REG22);
    if (beq_(REG10, REG21, 0)) {
        goto completed_wait;
    }
    sw_(REG10, REG0, REG23);
    add_(REG3, REG0, REG23);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    flushSelectedPage();
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    dropPage();
    
    testProcUpdate = proc->getProgramCounter();
test_proc_update:
    proc->setProgramCounter(testProcUpdate);
    add_(REG3, REG0, REG23);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    forcePageReload();
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    dropPage();

    if (beq_(REG4, REG0, 0)) {
        goto complete_loop_done;
    }
    addi_(REG7, REG0, 0x40);
    shortDelayLoop = proc->getProgramCounter();
short_delay_loop_nop:
    proc->setProgramCounter(shortDelayLoop);
    nop_();
    sub_(REG7, REG7, REG22);
    if (beq_(REG7, REG0, 0)) {
       goto test_proc_update;
    }
    br_(0);
    goto short_delay_loop_nop;

completed_wait:
    sw_(REG21, REG0, REG23);
    add_(REG3, REG0, REG23);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    flushSelectedPage();
    cout << proc->getNumber() << ": our work here is done" << endl;
    cout << "Ticks: " << proc->getTicks() << endl;
    masterTile->getBarrier()->decrementTaskCount();
 }  

//this function just to break code up
void ProcessorFunctor::nextRound() const
{
    uint64_t beforeCallEuclid;
    uint64_t beforeSecondCallEuclid;
    //clean out any junk
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    cleanCaches();
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
    //dump the page without writeback
    push_(REG3);
    push_(REG1);
    addi_(REG3, REG0, sizeof(uint64_t) * 2);
    br_(0);
    addi_(REG1, REG0, proc->getProgramCounter());
    dropPage();
    pop_(REG1);
    pop_(REG3);
    //REG29 points to first number in our reference line
    add_(REG29, REG0, REG3);
    add_(REG29, REG29, REG9);
    //REG4 - point to start of this processor's numbers
    mul_(REG4, REG11, REG1);
    add_(REG4, REG4, REG3);
    //add in offset to first usable number
    add_(REG4, REG4, REG16);
    //REG5 takes sign of first usable number
    lw_(REG5, REG4, REG0);
    andi_(REG5, REG5, 0xFF);
    //REG6 takes numerator of 1st usable
    //REG7 takes demoninator
    lwi_(REG6, REG4, sizeof(uint64_t));
    lwi_(REG7, REG4, (APNUMBERSIZE + 1) * sizeof(uint64_t));
    //remove offset
    sub_(REG4, REG4, REG16);
    
    //next set of numbers
    //REG13 progress
    //REG14 limit
    add_(REG13, REG0, REG12);
    addi_(REG14, REG0, sumCount);
    //now loop through all the numbers

    //some constants to save on cycles
    addi_(REG16, REG0, sizeof(uint64_t));
    addi_(REG15, REG0, 0x01);	
   
    const uint64_t nextRoundLoopStart = proc->getProgramCounter();
    //REG17 holds offset
    //REG18 holds calculated position on zero line
    //REG19 holds calculated position on processor line
 next_round_loop_start:
    proc->setProgramCounter(nextRoundLoopStart);
    mul_(REG17, REG13, REG2);

    //fetch 'bottom' row number
    add_(REG19, REG4, REG17);
    lw_(REG20, REG19, REG0);
    andi_(REG20, REG20, 0xFF);
    lw_(REG21, REG19, REG16);
    lwi_(REG22, REG19, (APNUMBERSIZE + 1) * sizeof(uint64_t));

    if (beq_(REG6, REG0, 0)) {
        goto next_round_prepare_to_save;
    }

    //fetch 'top' row number
    add_(REG18, REG29, REG17);
    lw_(REG23, REG18, REG0);
    andi_(REG23, REG23, 0xFF);
    lw_(REG24, REG18, REG16);
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

    and_(REG30, REG26, REG15);
    add_(REG31, REG0, REG15);
    if (beq_(REG30, REG31, 0)) {
	goto add_not_subtract;
    }
    and_(REG30, REG20, REG15);
    if (beq_(REG30, REG31, 0)) {
	goto reverse_add;
    }
    //first is positive, second is negative
    //subtract, but change sign on overflow
do_subtract:
    sub_(REG21, REG21, REG27);
    getsw_(REG30);
    andi_(REG30, REG30, 0x02);
    addi_(REG31, REG0, 0x02);
    if (beq_(REG30, REG31, 0)) {
        goto sign_reversal;
    }
    br_(0);
    goto next_round_euclid_again;

sign_reversal:
    add_(REG20, REG20, REG15);
    br_(0);
    goto next_round_euclid_again;


add_not_subtract:
    //second is positive (after minus)
    //check sign of first
    //if neg then take first from second
    //otherwise add first and second
    and_(REG30, REG20, REG15);
    add_(REG31, REG0, REG15);
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
    add_(REG30, REG0, REG15);
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
    sw_(REG21, REG19, REG16);
    swi_(REG22, REG19, (APNUMBERSIZE + 1) * sizeof(uint64_t));
    cout << "Stored: ";
    if (proc->getRegister(REG20) & 0x01) {
	cout <<"-";
    }
    cout << proc->getRegister(REG21) << "/" << proc->getRegister(REG22) << " : " << proc->getRegister(REG1) << ":" << proc->getRegister(REG12) << ":" << proc->getRegister(REG13) << endl;
    add_(REG13, REG13, REG15);
    sub_(REG30, REG14, REG13);
    if (beq_(REG30, REG0, 0)) {
        goto next_round_over;
    }
    br_(0);
    goto next_round_loop_start;

next_round_over:
    addi_(REG1, REG0, proc->getProgramCounter());
    br_(0);
    flushPages();
}
