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
//	sw_	rA, rB, rC	: rA -> *(rB + rC)	store word
//	swi_	rA, rB, imm	: rA -> *(rB + imm)	store word immediate
//	lw_	rA, rB, rC	: rA <- *(rB + rC)	load word
//	lwi_	rA, rB, imm	: rA <-	*(rB + imm)	load word immediate
//	beq_	rA, rB, imm	: PC <- imm iff rA == rB	branch if equal
//	br_	imm		: PC <- imm		branch immediate
//	mul_	rA, rB, rC	: rA <- rB * rC		multiply
//	muli_	rA, rB, imm	: rA <- rB * imm	multiply immediate

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
	//do nothing
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
	proc->setRegister(regA, proc->multiplyWithCarry(regB, multiplier));
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

///End of instruction set ///

#define SETSIZE 256

ProcessorFunctor::ProcessorFunctor(Tile *tileIn):
	tile{tileIn}, proc{tileIn->tileProcessor}
{
}

void ProcessorFunctor::operator()()
{
	const uint64_t order = tile->getOrder();
	if (order >= SETSIZE) {
		return;
	}
	proc->start();
	addi_(REG1, REG0, 0x1);
	cout << "2";
	setsw_(REG1);
	cout << "3";
	lwi_(REG0, REG1, 0x1000);	
	lwi_(REG1, REG0, 0x10000);
	lwi_(REG1, REG0, 0x10014);
	lwi_(REG1, REG0, 0x10400);
	lwi_(REG1, REG0, 0x10800);
	lwi_(REG1, REG0, 0x10C00);
	lwi_(REG1, REG0, 0x11000);
	lwi_(REG1, REG0, 0x11400);
	lwi_(REG1, REG0, 0x11800);
	lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);
lwi_(REG1, REG0, 0x11C00);

	lwi_(REG1, REG0, 0x12000);
	lwi_(REG1, REG0, 0x12400);
	lwi_(REG1, REG0, 0x12800);
	lwi_(REG1, REG0, 0x12C00);
	lwi_(REG1, REG0, 0x13000);
	lwi_(REG1, REG0, 0x13400);
	lwi_(REG1, REG0, 0x13800);
	lwi_(REG1, REG0, 0x13C00);
	lwi_(REG1, REG0, 0x14000);
	lwi_(REG1, REG0, 0x14400);
	lwi_(REG1, REG0, 0x15400);

	swi_(REG1, REG0, 0x12000);
	cout << " - our work here is done" << endl;
	Tile *masterTile = proc->getTile();
	masterTile->getBarrier()->decrementTaskCount();
}

