#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <QFile>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ControlThread.hpp"
#include "memorypacket.hpp"
#include "mux.hpp"
#include "noc.hpp"
#include "tile.hpp"


using namespace std;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    currentCycles = 0;
}

MainWindow::~MainWindow()
{
    delete ui;
}

class ExecuteFunctor
{
private:
    uint64_t columns;
    uint64_t rows;
    uint64_t pageShift;
    uint64_t memoryBlocks;
    uint64_t blockSize;
    MainWindow *mW;

public:
    ExecuteFunctor(uint64_t c, uint64_t r, uint64_t pS, uint64_t mB, uint64_t bS, MainWindow *wind):
        columns(c), rows(r), pageShift(pS), memoryBlocks(mB), blockSize(bS), mW(wind) {}

    void operator() ()
    {
        Noc networkTiles(columns, rows, pageShift, memoryBlocks, blockSize, mW);
        //Let's Go!
        networkTiles.executeInstructions();
    }
};


void MainWindow::on_pushButton_clicked()
{
    //disable button before we start
    ui->label->setText("Counting...");

    long totalTiles = rows * columns;
    if ((totalTiles == 0) || (totalTiles & (totalTiles - 1))) {
        cerr << "Must have power of two for number of tiles." << endl;
        exit(EXIT_FAILURE);
    }
    ExecuteFunctor eF(columns, rows, pageShift, memoryBlocks, blockSize, this);
    std::thread t(eF);
    t.detach();

}

void MainWindow::updateLCD()
{
    ui->lcdNumber->display(currentCycles);
    ui->lcdNumber->update();
}

void MainWindow::updateHardFaults()
{
    hardFaultMutex.lock();
    int hardFaultCount = ui->lcdNumber_2->intValue();
    ui->lcdNumber_2->display(++hardFaultCount);
    hardFaultMutex.unlock();
    ui->lcdNumber_2->update();

}

void MainWindow::updateSmallFaults()
{
    smallFaultMutex.lock();
    int smallFaultCount = ui->lcdNumber_3->intValue();
    ui->lcdNumber_3->display(++smallFaultCount);
    smallFaultMutex.unlock();
    ui->lcdNumber_3->update();
}
