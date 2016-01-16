#include <cstdlib>
#include <iostream>
#include <vector>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <bitset>
#include <map>
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
//  shiftli_ rA, imm    : rA << imm         shift left
//  shiftr_ rA          : rA >> 1           shift right
//  shiftri_ rA, imm    : rA >> imm         shift right
//  div_    rA, rB, rC  : rA = rB/rC        integer division
//  divi_   rA, rB, imm : rA = rB/imm       integer division by immediate
//  sub_    rA, rB, rC  : rA = rB - rC      subtract (with carry)
//  subi_   rA, rB, rC  : rA = rB - imm     subtract using immediate (with carry)
//  nop                 : no operation

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
	proc->setRegister(regA, proc->getRegister(regB) + imm);
	proc->pcAdvance();
}

void ProcessorFunctor::addm_(const uint64_t& regA,
	const uint64_t& regB, const uint64_t& address) const
{
	proc->setRegister(regA,
		proc->getRegister(regB) + proc->getLongAddress(address));
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
	proc->setRegister(regA, proc->getLongAddress(
		proc->getRegister(regB) + address)); 
	proc->pcAdvance();
}

bool ProcessorFunctor::beq_(const uint64_t& regA,
	const uint64_t& regB, const uint64_t& address) const
{
	if (proc->getRegister(regA) == proc->getRegister(regB)) {
		return true;
	} else {
		proc->pcAdvance();
		return false;
	}
}

void ProcessorFunctor::br_(const uint64_t& address) const
{
    proc->pcAdvance();
    //do nothing else
}

void ProcessorFunctor::nop_() const
{
    proc->waitATick();
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
    proc->setRegister(regA, proc->getLongAddress(proc->getStackPointer()));
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
    proc->setRegister(regA, proc->getRegister(regA) << imm);
    proc->pcAdvance();
}

void ProcessorFunctor::shiftr_(const uint64_t& regA) const
{
    proc->setRegister(regA, proc->getRegister(regA) >> 1);
    proc->pcAdvance();
}

void ProcessorFunctor::shiftri_(const uint64_t& regA, const uint64_t& imm)
    const
{
    proc->setRegister(regA, proc->getRegister(regA) >> imm);
    proc->pcAdvance();
}

///End of instruction set ///

#define SETSIZE 256

ProcessorFunctor::ProcessorFunctor(Tile *tileIn):
	tile{tileIn}, proc{tileIn->tileProcessor}
{
}

//returns GDB in REG3, return address in REG1
void ProcessorFunctor::euclidAlgorithm(const uint64_t& regA,
    const uint64_t& regB) const
{
    push_(REG1);
    push_(regA);
    push_(regB);
    push_(REG4);
    uint64_t anchor1 = proc->getProgramCounter();
test:
    proc->setProgramCounter(anchor1);
    sub_(REG4, regA, regB);
    if (beq_(REG4, REG0, 0)) {
        proc->setProgramCounter(proc->getProgramCounter() +
            sizeof(uint64_t) * 18);
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
    push_(regB);
    push_(regA);
    pop_(regB);
    pop_(regA);
divide:
    div_(REG4, regA, regB);
    mul_(REG1, REG4, regB);
    push_(REG5);
    sub_(REG5, regA, REG1);
    if (beq_(REG5, REG0, 0)) {
        proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t));
        goto multiple;
    }
    br_(0);
    proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t));
    goto remainder;
multiple:
    pop_(REG5);
    proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t) * 2);
    goto answer;
remainder:
    sub_(regA, regA, REG5);
    pop_(REG5);
    goto test;

answer:
    addi_(REG3, REG0, regB);
    pop_(REG4);
    pop_(regB);
    pop_(regA);
    pop_(REG1);
    br_(0); //simulate return
    return;
}

//return address in REG1
void ProcessorFunctor::executeZeroCPU() const
{
    push_(REG1);
    //read in the data
    addi_(REG1, REG0, SETSIZE);
    addi_(REG2, REG0, APNUMBERSIZE);
    addi_(REG3, REG0, 0);
    //REG4 takes address of start of numbers
    lwi_(REG4, REG0, sizeof(uint64_t) * 2);
    lw_(REG5, REG0, REG4);
    //set REG6 to 1
    addi_(REG6, REG0, 1);
    //read number
    lwi_(REG7, REG4, sizeof(uint64_t));
    //and write this to address
    swi_(REG6, REG4, sizeof(uint64_t));
    addi_(REG3, REG0, 1);
    uint64_t anchor1 = proc->getProgramCounter();
loop1:
    proc->setProgramCounter(anchor1);
    //loop through code
    muli_(REG8, REG3, (APNUMBERSIZE + 2) * 2);
    sw_(REG5, REG4, REG8);
    addi_(REG8, REG8, sizeof(uint64_t));
    sw_(REG7, REG4, REG8);
    addi_(REG3, REG3, 1);
    if (beq_(REG3, REG1, 0)) {
        proc->setProgramCounter(proc->getProgramCounter() + sizeof(uint64_t));
        goto ending;
    }
    br_(0);
    goto loop1;
ending:
    return;
    
}

void ProcessorFunctor::operator()()
{
	const uint64_t order = tile->getOrder();
	if (order >= SETSIZE) {
		return;
	}
	proc->start();
	addi_(REG1, REG0, 0x1);
	setsw_(REG1);
    addi_(REG1, REG0, proc->getNumber());
    swi_(REG1, REG0, PAGETABLESLOCAL + sizeof(uint64_t) * 3);
    //beq_ address is dummy
    if (beq_(REG1, REG0, 0)) {
        executeZeroCPU();
    }
	cout << " - our work here is done" << endl;
	Tile *masterTile = proc->getTile();
	masterTile->getBarrier()->decrementTaskCount();
}

