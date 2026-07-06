#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QPushButton>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ClientMsgBussiness.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_initialized(false)
{
    ui->setupUi(this);

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

    connect(ui->sendButton, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(ui->msgLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendClicked);

    ui->statusbar->showMessage("Ready");
    logMessage("Client started. Use 工作栏 -> 启动QXClient to connect.");
}

MainWindow::~MainWindow()
{
    if (m_initialized) {
        m_worker.Exit();
    }
    delete ui;
}

void MainWindow::MenuAction_Start()
{
    if (m_initialized) {
        statusBar()->showMessage("Already started!");
        return;
    }

    QFile file("ClientConf.json");
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Cannot open ClientConf.json");
        logMessage("ERROR: Cannot open ClientConf.json");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isNull()) {
        QMessageBox::warning(this, "Error", "Failed to parse ClientConf.json");
        return;
    }

    QJsonObject root = doc.object();
    C_WORKER_INIT_PARAM param;

    if (root.contains("ClientConf") && root["ClientConf"].isObject()) {
        QJsonObject clientConf = root["ClientConf"].toObject();
        if (clientConf.contains("Id"))
            param.ClientId = clientConf["Id"].toInt();
    }

    if (root.contains("ServerConf") && root["ServerConf"].isArray()) {
        QJsonArray servers = root["ServerConf"].toArray();
        for (const auto &s : servers) {
            QJsonObject serverObj = s.toObject();
            C_WORKER_SERVER_CONF sc;
            sc.Addr = serverObj["Addr"].toString().toStdString();
            if (serverObj.contains("Name"))
                sc.Name = serverObj["Name"].toString().toStdString();
            if (serverObj.contains("Id"))
                sc.Id = serverObj["Id"].toString().toStdString();
            param.Servers.push_back(sc);
            ui->serverListWidget->addItem(QString::fromStdString(sc.Addr));
        }
    }

    ERR_T ret = m_worker.Init(param);
    if (ret < SUCCESS) {
        QMessageBox::warning(this, "Error", QString("Init failed: %1").arg(ret));
        logMessage(QString("ERROR: Init failed with code %1").arg(ret));
        return;
    }

    m_initialized = true;
    ui->clientIdLabel->setText(QString("Client ID: %1").arg(param.ClientId));
    setupConnections();
    updateStatusBar();
    statusBar()->showMessage("Started successfully!");
    logMessage("Client initialized and connecting...");
}

void MainWindow::MenuAction_Stop()
{
    if (!m_initialized) {
        statusBar()->showMessage("Not started!");
        return;
    }

    m_worker.Exit();
    m_initialized = false;
    ui->sendButton->setEnabled(false);
    ui->connectionStatusLabel->setText("已停止");
    statusBar()->showMessage("Stopped.");
    logMessage("Client stopped.");
}

void MainWindow::MenuAction_Register()
{
    if (!m_initialized) {
        statusBar()->showMessage("Start the client first!");
        return;
    }
    statusBar()->showMessage("Register triggered manually.");
    logMessage("Manual register triggered.");
}

void MainWindow::onSendClicked()
{
    QString text = ui->msgLineEdit->text().trimmed();
    if (text.isEmpty())
        return;

    uint32_t recipientId = static_cast<uint32_t>(ui->recipientSpinBox->value());
    ERR_T ret = m_worker.SendMsg(recipientId, text.toStdString());
    if (ret < SUCCESS) {
        logMessage(QString("ERROR: Send failed (code %1)").arg(ret));
        statusBar()->showMessage(QString("Send failed: %1").arg(ret));
    } else {
        logMessage(QString("[%1 -> %2]: %3")
            .arg(m_worker.InitParam.ClientId)
            .arg(recipientId)
            .arg(text));
        ui->msgLineEdit->clear();
        statusBar()->showMessage("Message sent.");
    }
}

void MainWindow::onStateChanged(int state)
{
    updateStatusBar();
    ui->connectionStatusLabel->setText(stateToString(state));
    if (state == C_WORKER_STATS_REGISTERED) {
        ui->sendButton->setEnabled(true);
    }
}

void MainWindow::onMsgReceived(uint32_t fromClientId, QString msg)
{
    logMessage(QString("[%1 -> %2]: %3")
        .arg(fromClientId)
        .arg(m_worker.InitParam.ClientId)
        .arg(msg));
    statusBar()->showMessage(QString("Message received from client %1").arg(fromClientId));
}

void MainWindow::onErrorOccurred(QString error)
{
    logMessage("ERROR: " + error);
    statusBar()->showMessage("Error: " + error);
}

void MainWindow::onRegistered(uint32_t clientId)
{
    logMessage(QString("Client %1 registered successfully.").arg(clientId));
    ui->connectionStatusLabel->setText("已注册 (Active)");
    ui->sendButton->setEnabled(true);
}

void MainWindow::setupConnections()
{
    ClientMsgHandler *handler = m_worker.GetMsgHandler();
    if (!handler)
        return;

    connect(handler, &ClientMsgHandler::stateChanged, this, &MainWindow::onStateChanged);
    connect(handler, &ClientMsgHandler::msgReceived, this, &MainWindow::onMsgReceived);
    connect(handler, &ClientMsgHandler::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(handler, &ClientMsgHandler::registered, this, &MainWindow::onRegistered);
}

void MainWindow::updateStatusBar()
{
    QString stateStr = stateToString(m_worker.GetState());
    statusBar()->showMessage(stateStr);
}

void MainWindow::logMessage(const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    ui->msgLogEdit->appendPlainText(QString("[%1] %2").arg(timestamp, msg));
}

QString MainWindow::stateToString(int state) const
{
    switch (state) {
        case C_WORKER_STATS_UNSPEC: return "Unknown";
        case C_WORKER_STATS_INITED: return "Inited";
        case C_WORKER_STATS_CONNECTED: return "Connected";
        case C_WORKER_STATS_REGISTERED: return "Active";
        case C_WORKER_STATS_DISCONNECTED: return "Disconnected";
        case C_WORKER_STATS_EXIT: return "Exited";
        default: return "Unknown";
    }
}
