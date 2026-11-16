#ifndef MYTHREAD_H
#define MYTHREAD_H

#include <QThread>

class Mythread : public QThread
{
    Q_OBJECT
public:
    explicit Mythread(QObject *parent = nullptr);
    virtual void run();
public slots:
    void bANGLE(int angle);
    void bDIST (void);
    void bSCAN(int debut,int fin,int pas);
    void ipchange(QString ip);
    void portchange(int port);
};

#endif // MYTHREAD_H
