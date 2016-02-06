#include <QObject>
#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "mainwindow.h"

#ifndef __CONTROLTHREAD_
#define __CONTROLTHREAD_



class ControlThread: public QObject {
    Q_OBJECT

signals:
    void updateCycles();

private:
	uint64_t ticks;
	volatile uint16_t taskCount;
	volatile uint16_t signedInCount;
	std::mutex runLock;
	bool beginnable;
	std::condition_variable go;
	std::mutex taskCountLock;
    MainWindow *mainWindow;

public:
    ControlThread(unsigned long count = 0, MainWindow *pWind = nullptr);
	void incrementTaskCount();
	void decrementTaskCount();
	void run();
	void begin();
	void releaseToRun();
	void waitForBegin();
};

#endif
