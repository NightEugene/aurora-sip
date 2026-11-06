#include "keypadtone.h"
#include "dtmftone.h"
KeypadTone::KeypadTone(QObject *parent):QObject(parent){
    limit.setSingleShot(true);limit.setInterval(90);
    QObject::connect(&limit,&QTimer::timeout,this,&KeypadTone::stop);
    QObject::connect(&client,&Ngf::Client::connectionStatus,this,[this](bool value){available=value;emit changed();});
    QObject::connect(&client,&Ngf::Client::eventPlaying,this,[this](quint32 id){if(id==eventId){++playCount;emit changed();}});
    QObject::connect(&client,&Ngf::Client::eventFailed,this,[this](quint32 id){if(id==eventId){eventId=0;++failureCount;emit changed();}});
    QObject::connect(&client,&Ngf::Client::eventCompleted,this,[this](quint32 id){if(id==eventId)eventId=0;});
    client.connect();
}
KeypadTone::~KeypadTone(){stop();}
void KeypadTone::play(const QString &digit){
    const auto values=DtmfTone::properties(digit);if(values.isEmpty())return;
    stop();if(!client.isConnected())client.connect();
    eventId=client.play(QStringLiteral("dtmf"),values);limit.start();
}
void KeypadTone::stop(){limit.stop();if(eventId){client.stop(eventId);eventId=0;}}
