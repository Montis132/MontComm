#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>

#include "ClientWorker.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void MenuAction_Start();
    void MenuAction_Stop();
    void MenuAction_Register();
    void onSendClicked();
    void onStateChanged(int state);
    void onMsgReceived(uint32_t fromClientId, QString msg);
    void onErrorOccurred(QString error);
    void onRegistered(uint32_t clientId);

private:
    Ui::MainWindow *ui;
    ClientWorker m_worker;
    bool m_initialized;

    void setupConnections();
    void updateStatusBar();
    void logMessage(const QString &msg);
    QString stateToString(int state) const;
};
#endif // MAINWINDOW_H
