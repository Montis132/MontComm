#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QPushButton>
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // set MainMenu actions
    QAction *startAct = findChild<QAction*>("Start");
    QAction *stopAct = findChild<QAction*>("Stop");
    QAction *registerAct = findChild<QAction*>("Register");
    if (startAct) {
        connect(startAct, &QAction::triggered, this, &MainWindow::MenuAction_Start);
    }
    if (stopAct) {
        connect(stopAct, &QAction::triggered, this, &MainWindow::MenuAction_Stop);
    }
    if (registerAct) {
        connect(registerAct, &QAction::triggered, this, &MainWindow::MenuAction_Register);
    }
}

void MainWindow::MenuAction_Start()
{
    statusBar()->showMessage("Start success!");
}

void MainWindow::MenuAction_Stop()
{
    statusBar()->showMessage("Stop success!");
}

void MainWindow::MenuAction_Register()
{
    statusBar()->showMessage("Register success!");
}

MainWindow::~MainWindow()
{
    delete ui;
}

