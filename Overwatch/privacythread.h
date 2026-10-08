#ifndef PRIVACYTHREAD_H
#define PRIVACYTHREAD_H

#include <QThread>
#include <QImage>
#include <QString>
#include <opencv2/core.hpp>

class PrivacyThread : public QThread
{
    Q_OBJECT
public:
    explicit PrivacyThread(QObject *parent = nullptr);
    ~PrivacyThread();
    void stop();

signals:
    void frameReady(const QImage &frame);
    void statusUpdate(const QString &status);

protected:
    void run() override;

private:
    bool m_running;
};

#endif // PRIVACYTHREAD_H
