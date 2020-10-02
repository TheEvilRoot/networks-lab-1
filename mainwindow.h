#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

#include <memory>
#include <Windows.h>

#include "ui_mainwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ServerThread : public QThread {
    Q_OBJECT

public:
    explicit ServerThread(HANDLE* serverHandle, QObject *parent = nullptr):
        QThread(parent), handle { serverHandle } { }

    int readInt() {
        COMSTAT serialStatus;
        DWORD serialErrors;

        ClearCommError(*handle, &serialErrors, &serialStatus);

        DWORD actualRead = 0;
        uint8_t bytes[4] = { 0x0 };
        if (ReadFile(*handle, bytes, 4, &actualRead, nullptr) && actualRead == 4) {
            int value = 0;
            std::memcpy(reinterpret_cast<std::uint8_t*>(&value), bytes, 4);
            return value;
        }
        emit error("Failed to read int");
        return 0;
    }

    char readByte() {
        COMSTAT serialStatus;
        DWORD serialErrors;

        ClearCommError(*handle, &serialErrors, &serialStatus);

        DWORD actualRead = 0;
        char b = 0;
        if (ReadFile(*handle, &b, 1, &actualRead, nullptr)) {
            return b;
        }
        emit error("Failed to read byte");
        return 0;
    }

    void run() override {
        while (true) {
            auto cmd = readByte();
            if (cmd == 0x1) {
                auto length = readInt();
                if (length <= 0) continue;
                std::string msg;
                for (int i = 0; i < length; i++) {
                    msg += readByte();
                }
                emit message(QString::fromUtf8(msg.c_str(), msg.size()));
            } else if (cmd == 0x2) {
                Sleep(1000);
            }
        }
    }

signals:
    void message(QString);
    void error(QString);

private:
    HANDLE* handle;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr): QMainWindow(parent) {
        ui = std::unique_ptr<Ui::MainWindow>(new Ui::MainWindow);
        ui->setupUi(this);
        initUi();
        updateState(false);
    }

    ~MainWindow() { thread->terminate(); }


   void connectChanged(QComboBox *box, std::function<void(int)> f) {
        connect(box, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), f);
   }

   void writeInt(int value) {
       uint8_t bytes[4] = {
           static_cast<uint8_t>(value & 0xff),
           static_cast<uint8_t>((value & 0xff00) >> 8),
           static_cast<uint8_t>((value & 0xff0000) >> 16),
           static_cast<uint8_t>((value & 0xff000000) >> 24) };

       DWORD actualSend = 0;

       WriteFile(clientHandle, bytes, 4, &actualSend, nullptr);

       COMSTAT serialStatus;
       DWORD serialErrors;
       ClearCommError(clientHandle, &serialErrors, &serialStatus);
   }

   void writeByte(uint8_t byte) {
       DWORD actualSend = 0;

       WriteFile(clientHandle, &byte, 1, &actualSend, nullptr);

       COMSTAT serialStatus;
       DWORD serialErrors;
       ClearCommError(clientHandle, &serialErrors, &serialStatus);
   }


    void initUi() {
        connectChanged(ui->serverPort, [&](auto  index) {
            auto clientIndex = ui->clientPort->currentIndex();
            if (index == clientIndex) {
                auto newIndex = (index + 1) % 4;
                ui->serverPort->setCurrentIndex(newIndex);
            }
        });
        connectChanged(ui->clientPort, [&](auto  index) {
            auto serverIndex = ui->serverPort->currentIndex();
            if (index == serverIndex) {
                auto newIndex = (index + 1) % 4;
                ui->clientPort->setCurrentIndex(newIndex);
            }
        });
        connectChanged(ui->baudRate, [&](auto) {
            if (!isInitialized) return;
            writeByte(0x2);
            initSerialPort(ui->serverPort->currentText(), serverHandle);
            initSerialPort(ui->clientPort->currentText(), clientHandle);
        });
        connect(ui->connectButton, &QPushButton::clicked, [&]() {
            if (isInitialized) {
                writeByte(0x2);
                thread->terminate();
                CloseHandle(clientHandle);
                CloseHandle(serverHandle);
                updateState(false);
                ui->stateView_2->setText("Disconnected");
            } else {
                if (openSerialPorts() && initSerialPort(ui->clientPort->currentText(), clientHandle) &&
                        initSerialPort(ui->serverPort->currentText(), serverHandle)) {
                    updateState(true);
                    ui->stateView_2->setText("Connected");
                    initServerThread();
                } else {
                    CloseHandle(clientHandle);
                    CloseHandle(serverHandle);
                }
            }
        });
        connect(ui->sendButton, &QPushButton::clicked, [&]() {
            auto stdtext = ui->clientInputView->text().toStdString();
            if (stdtext.empty()) return;
            ui->sendButton->setDisabled(true);
            ui->clientInputView->setDisabled(true);
            ui->clientInputView->clear();
            QtConcurrent::run([this, stdtext]() {
                writeByte(0x1);
                writeInt(stdtext.size());
                for (auto i = 0; i < stdtext.size(); i++)
                    writeByte(stdtext[i]);
            });
        });
    }

    void initServerThread() {
        thread = std::unique_ptr<ServerThread>(new ServerThread(&serverHandle, this));
        connect(thread.get(), &ServerThread::message, [&](auto msg) {
            QMetaObject::invokeMethod(this, [this, msg]() {
                ui->serverOutputView->addItem(msg);
                ui->clientInputView->setDisabled(false);
                ui->sendButton->setDisabled(false);
            });
        });
        connect(thread.get(), &ServerThread::error, [&](auto err) {
           QMetaObject::invokeMethod(this, [this, err]() {
               ui->stateView_2->setText("Server: " + err);
               ui->clientInputView->setDisabled(false);
               ui->sendButton->setDisabled(false);
           });
        });
        thread->start();
    }

    bool openSerialPorts() {
        auto clientPort = ui->clientPort->currentText();
        auto serverPort = ui->serverPort->currentText();

        clientHandle = CreateFileA(clientPort.toStdString().c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (clientHandle == INVALID_HANDLE_VALUE) {
            ui->stateView_2->setText("Failed to open " + clientPort + " " + QString::number(GetLastError()));
            return false;
        }

        serverHandle = CreateFileA(serverPort.toStdString().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (serverHandle == INVALID_HANDLE_VALUE) {
            ui->stateView_2->setText("Failed to open " + serverPort + " " + QString::number(GetLastError()));
            CloseHandle(clientHandle);
            clientHandle = INVALID_HANDLE_VALUE;
            return false;
        }
        return true;
    }

    BYTE getPairity() {
        auto index = ui->pairity->currentIndex();
        if (index == 0) return NOPARITY;
        if (index == 1) return ODDPARITY;
        return EVENPARITY;
    }

    BYTE getByteSize() {
        auto index = ui->byteSize->currentIndex();
        if (index == 0) return DATABITS_8;
        if (index == 1) return DATABITS_7;
        if (index == 2) return DATABITS_6;
        return DATABITS_5;
    }

    BYTE getStopBits() {
        auto index = ui->stopBits->currentIndex();
        if (index == 0) return STOPBITS_10;
        if (index == 1) return STOPBITS_15;
        return STOPBITS_20;
    }

    bool initSerialPort(QString name, HANDLE handle) {
        auto baudRate = ui->baudRate->currentText().toULong();
        auto pairity = getPairity();
        auto byteSize = getByteSize();
        auto stopBits = getStopBits();
        DCB params;
        if (GetCommState(handle, &params)) {
            params.BaudRate = baudRate;
            params.ByteSize = byteSize;
            params.StopBits = stopBits;
            params.Parity = pairity;
            params.fDtrControl = DTR_CONTROL_ENABLE;

            if (SetCommState(handle, &params)) {
                PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
                Sleep(500); // don't know why, but... whatever
                return true;
            } else {
                 ui->stateView_2->setText("Failed to init " + name);
            }
        } else {
            ui->stateView_2->setText("Failed to get state of " + name);
        }
        return false;
    }

    void updateState(bool newState) {
        isInitialized = newState;
        ui->clientPort->setDisabled(isInitialized);
        ui->serverPort->setDisabled(isInitialized);
        ui->byteSize->setDisabled(isInitialized);
        ui->stopBits->setDisabled(isInitialized);
        ui->pairity->setDisabled(isInitialized);
        ui->sendButton->setDisabled(!isInitialized);
        ui->connectButton->setText(isInitialized ? "Disconnect" : "Connect");
    }

private:
    std::unique_ptr<Ui::MainWindow> ui;
    std::unique_ptr<ServerThread> thread;
    HANDLE clientHandle { INVALID_HANDLE_VALUE };
    HANDLE serverHandle { INVALID_HANDLE_VALUE };
    bool isInitialized  { false };
};

#endif
