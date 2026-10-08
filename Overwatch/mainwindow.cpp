#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Initialize the background privacy thread
    m_privacyThread = new PrivacyThread(this);

    // Connect the thread's signals to this window's slots
    // This allows safe communication between the background math and the GUI
    connect(m_privacyThread, &PrivacyThread::frameReady, this, &MainWindow::updateFrame);
    connect(m_privacyThread, &PrivacyThread::statusUpdate, this, &MainWindow::updateStatus);

    // Start the background monitoring loop
    m_privacyThread->start();
}

MainWindow::~MainWindow()
{
    // Clean up the thread safely before closing the app
    if (m_privacyThread->isRunning()) {
        m_privacyThread->stop();
        m_privacyThread->wait();
    }
    delete ui;
}

void MainWindow::updateFrame(const QImage &image)
{
    // Display the QImage on the QLabel and scale it to fit
    if (ui->videoLabel) {
        ui->videoLabel->setPixmap(QPixmap::fromImage(image).scaled(
            ui->videoLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
    }
}

void MainWindow::updateStatus(const QString &message)
{
    // Update the status text on the screen
    if (ui->statusLabel) {
        ui->statusLabel->setText(message);
    }
}
