#include <QObject>
#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "mainwindow.h"
#include "processor.hpp"
#include "ControlThread.hpp"

using namespace std;

static uint POWER_MAX = 24; //maximum number of active cores

ControlThread::ControlThread(unsigned long tcks, MainWindow *pWind):
    ticks(tcks), taskCount(0), beginnable(false), mainWindow(pWind)
{
    QObject::connect(this, SIGNAL(updateCycles()),
        pWind, SLOT(updateLCD()));
    blockedInTree = 0;
}

void ControlThread::releaseToRun()
{
	unique_lock<mutex> lck(runLock);
	taskCountLock.lock();
	signedInCount++;
	if (signedInCount >= taskCount) {
		taskCountLock.unlock();
		lck.unlock();
		powerLock.lock();
		powerCount = 0;
		powerLock.unlock();
		run();
		return;
	}
	taskCountLock.unlock();
	go.wait(lck);
}

void ControlThread::sufficientPower(Processor *pActive)
{
	unique_lock<mutex> lck(powerLock);
	bool statePower = pActive->getTile()->getPowerState();
	if (statePower) {
		lck.unlock();
		return;
	}
	powerCount++;
	if (powerCount > POWER_MAX) {
		lck.unlock();
		if (!pActive->isInInterrupt()) {
			pActive->getTile()->setPowerStateOff();
		}
		return;
	}
	lck.unlock();
	return;
}

void ControlThread::incrementTaskCount()
{
	unique_lock<mutex> lock(taskCountLock);
	taskCount++;
}

void ControlThread::decrementTaskCount()
{
	unique_lock<mutex> lck(runLock);
	unique_lock<mutex> lock(taskCountLock);
	taskCount--;
    lock.unlock();
    lck.unlock();
	if (signedInCount >= taskCount) {
		run();
	}
}

void ControlThread::incrementBlocks()
{
	unique_lock<mutex> lck(runLock);
	unique_lock<mutex> lckBlock(blockLock);
	blockedInTree++;
	lckBlock.unlock();
}

void ControlThread::run()
{
	unique_lock<mutex> lck(runLock);
	unique_lock<mutex> lckBlock(blockLock);
	if (blockedInTree > 0) {
		cout << "On tick " << ticks << " total blocks ";
		cout << blockedInTree << endl;
		blockedInTree = 0;
	}
	lckBlock.unlock();
	signedInCount = 0;
	ticks++;
	go.notify_all();
	//update LCD display
	++(mainWindow->currentCycles);
	emit updateCycles();
}

void ControlThread::waitForBegin()
{
	unique_lock<mutex> lck(runLock);
	go.wait(lck, [&]() { return this->beginnable;});
}

void ControlThread::begin()
{
	runLock.lock();
	beginnable = true;
	go.notify_all();
	runLock.unlock();
}

bool ControlThread::tryCheatLock()
{
	return cheatLock.try_lock();
}

void ControlThread::unlockCheatLock()
{
	cheatLock.unlock();
}
