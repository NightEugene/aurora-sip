#pragma once
#include <QObject>
#include <QTimer>
#include <NgfClient>
class KeypadTone : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(int played READ played NOTIFY changed)
    Q_PROPERTY(int failures READ failures NOTIFY changed)
public:
    explicit KeypadTone(QObject *parent=nullptr);
    ~KeypadTone();
    bool connected() const {return available;}
    int played() const {return playCount;}
    int failures() const {return failureCount;}
    Q_INVOKABLE void play(const QString &digit);
    Q_INVOKABLE void stop();
signals:
    void changed();
private:
    Ngf::Client client;
    QTimer limit;
    quint32 eventId=0;
    bool available=false;
    int playCount=0,failureCount=0;
};
