#pragma once
#include <QObject>
#include <QTimer>
class ReconnectPolicy : public QObject {
    Q_OBJECT
public:
    explicit ReconnectPolicy(QObject *parent=nullptr,int firstDelay=1000,int maxDelay=30000):QObject(parent),first(firstDelay),maximum(maxDelay){
        timer.setSingleShot(true);timer.setTimerType(Qt::PreciseTimer);
        connect(&timer,&QTimer::timeout,this,[this]{if(want&&!paused){++attempt;emit retryRequested();}});
    }
    void begin(){want=true;paused=false;attempt=0;timer.stop();}
    void connected(){paused=false;attempt=0;timer.stop();}
    void cancel(){want=false;paused=false;attempt=0;timer.stop();}
    void block(){paused=true;timer.stop();}
    void schedule(){
        if(!want||paused||timer.isActive())return;
        qint64 delay=qMax(1,first);
        for(int i=0;i<attempt&&delay<maximum;++i)delay=qMin<qint64>(maximum,delay*2);
        timer.start(int(qMin<qint64>(maximum,delay)));
    }
    void retryNow(){if(want&&!paused){attempt=0;timer.start(qMax(1,first));}}
    bool wanted() const {return want;}
    bool blocked() const {return paused;}
    int attempts() const {return attempt;}
    int remaining() const {return timer.remainingTime();}
signals:
    void retryRequested();
private:
    QTimer timer;
    int first,maximum,attempt=0;
    bool want=false,paused=false;
};
