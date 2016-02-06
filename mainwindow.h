#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLCDNumber>
#include <mutex>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    Ui::MainWindow *ui;
    uint64_t rows;
    uint64_t columns;
    uint64_t pageShift;
    uint64_t blockSize;
    uint64_t memoryBlocks;
    std::mutex hardFaultMutex;
    std::mutex smallFaultMutex;

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void setRows(const uint64_t r) {rows = r;}
    void setColumns(const uint64_t c) {columns = c;}
    void setPageShift(const uint64_t pS) {pageShift = pS;}
    void setBlockSize(const uint64_t bS) {blockSize = bS;}
    void setMemoryBlocks(const uint64_t mB) {memoryBlocks = mB;}
    int currentCycles;

private slots:
    void on_pushButton_clicked();

public slots:
    void updateHardFaults();
    void updateSmallFaults();
    void updateLCD();


};

#endif // MAINWINDOW_H
