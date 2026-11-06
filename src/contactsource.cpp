#include "contactsource.h"
#include "appsettings.h"
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QDir>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <algorithm>
#include <QNetworkConfigurationManager>

namespace {const int maxBytes=2*1024*1024;const int maxContacts=10000;}
ContactSource::ContactSource(QObject *parent,int requestTimeoutMs,int idleTimeoutMs):QObject(parent){
    savedSource=AppSettings().value("contacts/source").toString();
    message=savedSource.isEmpty()?QStringLiteral("Каталог отключён"):QStringLiteral("Контакты ещё не загружены");
    if(loadCache())message=cacheStatus();
    timeout.setSingleShot(true);timeout.setInterval(qMax(1,requestTimeoutMs));
    idleTimeout.setSingleShot(true);idleTimeout.setInterval(qMax(1,idleTimeoutMs));
    connect(&idleTimeout,&QTimer::timeout,this,[this]{fail(QStringLiteral("Сервер контактов не отвечает. Повторите обновление."));});
    auto networkState=new QNetworkConfigurationManager(this);
    connect(networkState,&QNetworkConfigurationManager::onlineStateChanged,this,[this](bool online){
        if(!online&&busy&&!sourceUrl(savedSource).isLocalFile())fail(QStringLiteral("Соединение потеряно. Повторите обновление после восстановления сети."));
    });
    connect(&timeout,&QTimer::timeout,this,[this]{fail(QStringLiteral("Время загрузки контактов истекло. Повторите обновление."));});
}
QUrl ContactSource::sourceUrl(const QString &value){
    QString text=value.trimmed();if(text.isEmpty())return {};
    if(text.startsWith("~/"))text=QDir::homePath()+text.mid(1);
    if(QDir::isAbsolutePath(text))return QUrl::fromLocalFile(QDir::cleanPath(text));
    QUrl url(text,QUrl::StrictMode);
    if(!url.isValid() || !url.userInfo().isEmpty() || url.hasFragment())return {};
    if(url.scheme()=="file"){
        if((!url.host().isEmpty()&&url.host()!="localhost") || url.hasQuery() || !QDir::isAbsolutePath(url.toLocalFile()))return {};
        return QUrl::fromLocalFile(url.toLocalFile());
    }
    if((url.scheme()!="https"&&url.scheme()!="http") || url.host().isEmpty())return {};
    return url;
}
QString ContactSource::dialNumber(const QString &value){
    static const QRegularExpression uriPattern("sips?:([^@;>]+)"),formatting("[\\s()\\-.]"),valid("^[+0-9*#]{1,64}$");
    QString number=value.trimmed();auto uri=uriPattern.match(number);
    if(uri.hasMatch())number=uri.captured(1);
    number.remove(formatting);
    return valid.match(number).hasMatch()?number:QString();
}
bool ContactSource::parseXml(const QByteArray &data,QVariantList *contacts,QString *error){
    if(data.size()>maxBytes){*error=QStringLiteral("XML-файл слишком большой (максимум 2 МБ).");return false;}
    QXmlStreamReader xml(data);QVariantList result;QSet<QString> seen;
    bool root=false,closed=false,inEntry=false;int depth=0,entryDepth=0,count=0;
    QString name;QStringList numbers;
    while(!xml.atEnd()){
        auto token=xml.readNext();
        // Do not expand entities or load a directory's linked URLs/files.
        if(token==QXmlStreamReader::DTD || token==QXmlStreamReader::EntityReference){xml.raiseError(QStringLiteral("DTD и сущности не поддерживаются"));break;}
        if(token==QXmlStreamReader::StartElement){
            ++depth;if(depth>16){xml.raiseError(QStringLiteral("Слишком глубокая структура XML"));break;}
            if(!root){
                if(xml.name()!=QLatin1String("YealinkIPPhoneDirectory")){xml.raiseError(QStringLiteral("Ожидается каталог YealinkIPPhoneDirectory"));break;}
                root=true;
            } else if(depth==2 && xml.name()==QLatin1String("DirectoryEntry")){
                inEntry=true;entryDepth=depth;name.clear();numbers.clear();
                if(++count>maxContacts){xml.raiseError(QStringLiteral("Слишком много контактов"));break;}
            } else if(inEntry && depth==entryDepth+1 && (xml.name()==QLatin1String("Name") || xml.name()==QLatin1String("Telephone"))){
                const bool isName=xml.name()==QLatin1String("Name");
                QString text=xml.readElementText(QXmlStreamReader::ErrorOnUnexpectedElement).trimmed();--depth;
                if(isName)name=text.left(256);else numbers.append(text);
            } else if(depth==2 && xml.name()==QLatin1String("MenuItem")){
                xml.raiseError(QStringLiteral("Укажите файл контактов, а не меню групп"));break;
            }
        } else if(token==QXmlStreamReader::EndElement){
            if(inEntry && depth==entryDepth){
                for(const auto &raw:numbers){const QString number=dialNumber(raw);if(number.isEmpty()||seen.contains(number))continue;
                    seen.insert(number);QVariantMap row;row["name"]=name.isEmpty()?number:name;row["number"]=number;result.append(row);
                    if(result.size()>maxContacts){xml.raiseError(QStringLiteral("Слишком много номеров"));break;}
                }
                inEntry=false;
            }
            --depth;if(depth==0)closed=true;
        }
    }
    if(xml.hasError()||!root||!closed){*error=QStringLiteral("Не удалось прочитать XML-каталог Yealink. Проверьте файл.");return false;}
    std::sort(result.begin(),result.end(),[](const QVariant &a,const QVariant &b){return QString::localeAwareCompare(a.toMap().value("name").toString(),b.toMap().value("name").toString())<0;});
    *contacts=result;return true;
}
void ContactSource::configure(const QString &value){
    if(busy)return;
    const auto url=sourceUrl(value);
    if(!value.trimmed().isEmpty()&&url.isEmpty()){fail(QStringLiteral("Укажите HTTP/HTTPS-ссылку или полный путь к XML-файлу."));return;}
    const QString next=url.isLocalFile()?url.toLocalFile():url.toString(QUrl::FullyEncoded);
    AppSettings settings;settings.setValue("contacts/source",next);settings.sync();
    if(settings.status()!=QSettings::NoError){fail(QStringLiteral("Не удалось сохранить источник контактов."));return;}
    if(savedSource!=next){
        savedSource=next;rows.clear();haveSnapshot=savedCopy=persistedSnapshot=false;snapshotAt=QDateTime();
        loadCache();emit contactsChanged();
    }
    savedSource=next;emit changed();refresh();
}
void ContactSource::refresh(){
    if(busy)return;
    if(savedSource.isEmpty()){message=QStringLiteral("Каталог отключён");emit changed();return;}
    const auto url=sourceUrl(savedSource);
    if(url.isEmpty()){fail(QStringLiteral("Некорректный адрес XML-файла."));return;}
    busy=true;message=QStringLiteral("Загрузка контактов…");received.clear();visited.clear();timeout.start();emit changed();
    // Defer I/O so the loading state is rendered before opening a local file.
    const auto generation=++loadGeneration;
    QTimer::singleShot(0,this,[this,url,generation]{if(generation==loadGeneration)loadUrl(url);});
}
void ContactSource::loadUrl(const QUrl &url){
    if(!busy)return;
    if(url.isLocalFile()){
        QFile file(url.toLocalFile());QFileInfo info(file);
        if(!info.isFile() || !file.open(QIODevice::ReadOnly)){fail(QStringLiteral("Не удалось открыть XML-файл. Проверьте путь и доступ к файлу."));return;}
        if(file.size()>maxBytes){fail(QStringLiteral("XML-файл слишком большой (максимум 2 МБ)."));return;}
        const auto data=file.read(maxBytes+1);
        if(file.error()!=QFile::NoError){fail(QStringLiteral("Ошибка чтения XML-файла."));return;}
        accept(data);return;
    }
    if(visited.contains(url)||visited.size()>=6){fail(QStringLiteral("Слишком много перенаправлений при загрузке XML."));return;}
    visited.insert(url);received.clear();
    QNetworkRequest request(url);request.setAttribute(QNetworkRequest::FollowRedirectsAttribute,false);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::AlwaysNetwork);
    request.setRawHeader("Accept","application/xml, text/xml;q=0.9, */*;q=0.5");
    idleTimeout.start();
    pending=network.get(request);auto reply=pending;
    connect(reply,&QIODevice::readyRead,this,[this,reply]{
        if(pending!=reply)return;
        const auto data=reply->read(maxBytes+1-received.size());
        if(!data.isEmpty())idleTimeout.start();
        received+=data;
        if(received.size()>maxBytes)fail(QStringLiteral("XML-файл слишком большой (максимум 2 МБ)."));
    });
    connect(reply,&QNetworkReply::downloadProgress,this,[this,reply](qint64 n,qint64 total){if(pending==reply&&(n>maxBytes||total>maxBytes))fail(QStringLiteral("XML-файл слишком большой (максимум 2 МБ)."));});
    connect(reply,&QNetworkReply::finished,this,[this,reply]{finish(reply);});
}
void ContactSource::finish(QNetworkReply *reply){
    if(pending!=reply){reply->deleteLater();return;}
    pending=nullptr;idleTimeout.stop();reply->deleteLater();
    const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(status>=300&&status<400){
        const auto target=reply->url().resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
        const auto checked=sourceUrl(target.toString(QUrl::FullyEncoded));
        if(checked.isEmpty() || checked.isLocalFile() || (reply->url().scheme()=="https"&&checked.scheme()!="https")) {fail(QStringLiteral("Недопустимое перенаправление XML-файла."));return;}
        loadUrl(checked);return;
    }
    if(reply->error()!=QNetworkReply::NoError||status!=200){
        if(status==401||status==403)fail(QStringLiteral("Сервер ограничил доступ к XML-файлу."));
        else if(reply->error()==QNetworkReply::SslHandshakeFailedError)fail(QStringLiteral("Не удалось проверить TLS-сертификат сервера контактов."));
        else fail(status?QStringLiteral("Не удалось загрузить XML: HTTP %1.").arg(status):QStringLiteral("Не удалось подключиться к серверу контактов."));
        return;
    }
    received+=reply->read(maxBytes+1-received.size());accept(received);
}
void ContactSource::accept(const QByteArray &data){
    QVariantList updated;QString error;
    if(!parseXml(data,&updated,&error)){fail(error);return;}
    const bool stored=saveCache(data);
    rows=updated;haveSnapshot=true;savedCopy=false;persistedSnapshot=stored;snapshotAt=QDateTime::currentDateTimeUtc();busy=false;timeout.stop();idleTimeout.stop();received.clear();
    message=rows.isEmpty()?QStringLiteral("В XML-каталоге нет номеров"):QStringLiteral("Контакты обновлены: %1").arg(rows.size());
    if(!stored)message+=QStringLiteral(". Не удалось сохранить локальную копию.");
    emit contactsChanged();emit changed();emit refreshFinished(true);
}
void ContactSource::fail(const QString &error){
    const bool wasLoading=busy;
    ++loadGeneration;timeout.stop();idleTimeout.stop();busy=false;received.clear();savedCopy=haveSnapshot;message=error;
    if(haveSnapshot)message+=QStringLiteral(" Показана ранее загруженная копия.");
    if(pending){auto reply=pending;pending=nullptr;QObject::disconnect(reply,nullptr,this,nullptr);reply->abort();reply->deleteLater();}
    emit changed();if(wasLoading)emit refreshFinished(false);
}

void ContactSource::cancel(){if(busy)fail(QStringLiteral("Обновление контактов отменено"));}

QString ContactSource::cachePath() const {
    const auto url=sourceUrl(savedSource);if(url.isEmpty())return {};
    const auto key=QCryptographicHash::hash(url.toEncoded(QUrl::FullyEncoded),QCryptographicHash::Sha256).toHex();
    return AppSettings::dataDirectory()+"/contacts-cache-v1/"+QString::fromLatin1(key)+".xml";
}
bool ContactSource::loadCache(){
    const auto path=cachePath();if(path.isEmpty())return false;
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>maxBytes)return false;
    QVariantList cached;QString error;const auto data=file.read(maxBytes+1);
    if(file.error()!=QFile::NoError||!parseXml(data,&cached,&error))return false;
    rows=cached;haveSnapshot=savedCopy=persistedSnapshot=true;snapshotAt=QFileInfo(file).lastModified().toUTC();return true;
}
bool ContactSource::saveCache(const QByteArray &xml){
    const auto path=cachePath();if(path.isEmpty())return false;
    const QString directory=QFileInfo(path).absolutePath();
    if(!QDir().mkpath(directory))return false;
    if(!QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner))return false;
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return false;
    if(!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner))return false;
    return file.write(xml)==xml.size()&&file.commit();
}
QString ContactSource::cacheStatus() const {
    if(!haveSnapshot)return {};
    QString text=persistedSnapshot?QStringLiteral("Сохранённая копия"):QStringLiteral("Последние загруженные контакты");
    if(snapshotAt.isValid())text+=QStringLiteral(" · ")+snapshotAt.toLocalTime().toString("dd.MM.yyyy HH:mm");
    return text;
}
