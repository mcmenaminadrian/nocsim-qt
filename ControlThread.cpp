#include <QObject>
#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "mainwindow.h"
#include "ControlThread.hpp"

using namespace std;

ControlThread::ControlThread(unsigned long tcks, MainWindow *pWind):
    ticks(tcks), taskCount(0), beginnable(false), mainWindow(pWind)
{
    QObject::connect(this, SIGNAL(updateCycles()),
        pWind, SLOT(updateLCD()));
}

void ControlThread::releaseToRun()
{
	unique_lock<mutex> lck(runLock);
	taskCountLock.lock();
	signedInCount++;
	if (signedInCount >= taskCount) {
		lck.unlock();
		run();
		return;
	}
	taskCountLock.unlock();
	go.wait(lck);
}

void ControlThread::incrementTaskCount()
{
	unique_lock<mutex> lock(taskCountLock);
	taskCount++;
}

void ControlThread::incrementBlocks()
{
    unique_lock<mutex> lck(blockLock);
    blockedMem++;
}

void ControlThread::decrementTaskCount()
{
	unique_lock<mutex> lock(taskCountLock);
	taskCount--;
	if (signedInCount >= taskCount) {
		run();
	}
}

void ControlThread::run()
{
	unique_lock<mutex> lck(runLock);
    unique_lock<mutex> lck2(blockLock);
	signedInCount = 0;
    if (blockedMem > 0) {
        cout << "On tick " << ticks << " total blocks " << blockedMem;
        cout << endl;
    }
    blockedMem = 0;
    lck2.unlock();
	ticks++;
	go.notify_all();
    //update LCD display
    ++(mainWindow->currentCycles);
	taskCountLock.unlock();
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
