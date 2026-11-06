#include "addressbook.h"
#include "appsettings.h"
#include <QRegularExpression>

void FavoriteWriter::write(const QVariantList &rows){AppSettings settings;settings.setValue("favorites",rows);settings.sync();emit stored(settings.status()==QSettings::NoError);}
AddressBook::AddressBook(QObject *parent):QObject(parent){
    connect(&directorySource,&ContactSource::changed,this,&AddressBook::contactsChanged);
    connect(&directorySource,&ContactSource::contactsChanged,this,[this]{rebuildNames();emit contactsChanged();});
    connect(this,&AddressBook::favoritesChanged,this,&AddressBook::rebuildNames);
    for(const auto &entry:AppSettings().value("favorites").toList()){
        QVariantMap row=entry.toMap();QString number=dialNumber(row.value("number").toString());
        if(number.isEmpty()||favoriteNumbers.contains(number))continue;
        row["number"]=number;favoriteNumbers.insert(number);savedFavorites.append(row);
    }
    rebuildNames();
    writer=new FavoriteWriter;writer->moveToThread(&storageThread);
    connect(writer,&FavoriteWriter::stored,this,[this](bool ok){writeError=ok?QString():QStringLiteral("Не удалось сохранить избранное. Проверьте свободное место и повторите изменение.");emit storageErrorChanged();});
    connect(&storageThread,&QThread::finished,writer,&QObject::deleteLater);
    connect(this,&AddressBook::persistRequested,writer,&FavoriteWriter::write,Qt::QueuedConnection);
    persistTimer.setSingleShot(true);persistTimer.setInterval(150);
    connect(&persistTimer,&QTimer::timeout,this,[this]{emit persistRequested(savedFavorites);});
    storageThread.start();
}
AddressBook::~AddressBook(){
    persistTimer.stop();
    if(storageThread.isRunning()){
        QMetaObject::invokeMethod(writer,"write",Qt::BlockingQueuedConnection,Q_ARG(QVariantList,savedFavorites));
        storageThread.quit();storageThread.wait();
    }
}
QString AddressBook::dialNumber(const QString &value) const {
    return ContactSource::dialNumber(value);
}
bool AddressBook::isFavorite(const QString &number) const {return favoriteNumbers.contains(dialNumber(number));}
void AddressBook::toggleFavorite(const QString &name,const QString &number){
    const QString normalized=dialNumber(number);if(normalized.isEmpty())return;
    if(favoriteNumbers.remove(normalized)){
        for(int i=savedFavorites.size()-1;i>=0;--i)if(savedFavorites[i].toMap().value("number").toString()==normalized)savedFavorites.removeAt(i);
    } else {
        QVariantMap row;row["name"]=name.trimmed().isEmpty()?normalized:name.trimmed();row["number"]=normalized;
        favoriteNumbers.insert(normalized);savedFavorites.append(row);
    }
    ++revision;emit favoritesChanged();persistTimer.start();
}

void AddressBook::rebuildNames(){
    contactNames.clear();
    // The current directory takes precedence over saved favorite labels.
    for(const auto &entries:{contacts(),savedFavorites})for(const auto &entry:entries){
        const auto row=entry.toMap();const QString number=dialNumber(row.value("number").toString());
        const QString name=row.value("name").toString().trimmed();
        if(!number.isEmpty()&&!name.isEmpty()&&name!=number&&!contactNames.contains(number))contactNames.insert(number,name);
    }
    ++nameRevision;emit namesChanged();
}
QString AddressBook::nameForNumber(const QString &number) const {
    return contactNames.value(dialNumber(number));
}
