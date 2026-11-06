#include "contactsource.h"
#include "addressbook.h"
#include "appsettings.h"
#include "accountsettings.h"
#include <QCryptographicHash>
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QSslKey>
#include <QTemporaryDir>
#include <QFile>
#include <QJSEngine>

class XmlServer : public QTcpServer {
public:
    QList<QByteArray> responses;
    QList<QByteArray> requests;
    bool tls=false,stall=false,slow=false;
    QSslCertificate cert;
    QSslKey key;
    XmlServer(bool ssl=false):tls(ssl){
        if(tls){QFile c(qgetenv("TEST_CERT")),k(qgetenv("TEST_KEY"));c.open(QIODevice::ReadOnly);k.open(QIODevice::ReadOnly);cert=QSslCertificate(c.readAll());key=QSslKey(k.readAll(),QSsl::Rsa);}
        listen(QHostAddress::LocalHost);
    }
    QString url() const {return QString(tls?"https://localhost:%1/book.xml":"http://127.0.0.1:%1/book.xml").arg(serverPort());}
    static QByteArray response(const QByteArray &body,int status=200,const QByteArray &headers={}){
        return "HTTP/1.1 "+QByteArray::number(status)+" Result\r\nContent-Type: text/xml\r\nConnection: close\r\n"+headers+"Content-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body;
    }
protected:
    void incomingConnection(qintptr handle) override {
        QTcpSocket *socket;
        if(tls){auto ssl=new QSslSocket(this);ssl->setSocketDescriptor(handle);ssl->setLocalCertificate(cert);ssl->setPrivateKey(key);ssl->setPeerVerifyMode(QSslSocket::VerifyNone);socket=ssl;}
        else {socket=new QTcpSocket(this);socket->setSocketDescriptor(handle);}
        auto data=QSharedPointer<QByteArray>::create();
        connect(socket,&QIODevice::readyRead,this,[this,socket,data]{
            *data+=socket->readAll();if(!data->contains("\r\n\r\n"))return;
            requests.append(*data);data->clear();
            if(stall)return;
            if(slow){
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: 100000\r\n\r\n");
                auto trickle=new QTimer(socket);trickle->setInterval(30);
                connect(trickle,&QTimer::timeout,socket,[socket]{socket->write("x");});trickle->start();return;
            }
            socket->write(responses.isEmpty()?response("",500):responses.takeFirst());socket->disconnectFromHost();
        });
        connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        if(tls)static_cast<QSslSocket*>(socket)->startServerEncryption();
    }
};
static const QByteArray book=R"(<?xml version="1.0" encoding="UTF-8"?><YealinkIPPhoneDirectory><DirectoryEntry><Name>Иван &amp; Анна</Name><Telephone>849</Telephone><Telephone>+7 (900) 123-45-67</Telephone></DirectoryEntry><DirectoryEntry><Name>Дубль</Name><Telephone>849</Telephone></DirectoryEntry><DirectoryEntry><Name>Bad</Name><Telephone>not-a-phone</Telephone></DirectoryEntry></YealinkIPPhoneDirectory>)";
class ContactSourceTest : public QObject {
    Q_OBJECT
    void waitReady(ContactSource &client){QTRY_VERIFY(!client.loading());}
private slots:
    void initTestCase(){
        const QString storage=QString::fromLocal8Bit(qgetenv("AURORA_TEST_STORAGE"));
        QVERIFY2(!storage.isEmpty()&&storage.startsWith(QDir::tempPath()+"/"),"Tests require a dedicated temporary storage");
        QVERIFY(AppSettings().fileName().startsWith(storage+"/"));
        QVERIFY(AppSettings::dataDirectory().startsWith(storage+"/"));
        QVERIFY(QSslSocket::supportsSsl());QFile c(qgetenv("TEST_CERT"));QVERIFY(c.open(QIODevice::ReadOnly));
        auto config=QSslConfiguration::defaultConfiguration();auto ca=config.caCertificates();ca.append(QSslCertificate(c.readAll()));config.setCaCertificates(ca);QSslConfiguration::setDefaultConfiguration(config);
    }
    void init(){AppSettings settings;settings.setValue("contacts/source",QString());settings.sync();}
    QString cachedPath(const QString &source){
        auto key=QCryptographicHash::hash(ContactSource::sourceUrl(source).toEncoded(QUrl::FullyEncoded),QCryptographicHash::Sha256).toHex();
        return AppSettings::dataDirectory()+"/contacts-cache-v1/"+QString::fromLatin1(key)+".xml";
    }
    void freshInstallHasNoRemoteDirectory(){
        AppSettings settings;settings.remove("contacts/source");settings.sync();
        ContactSource client;QVERIFY(client.source().isEmpty());
        client.refresh();QVERIFY(!client.loading());QVERIFY(client.contacts().isEmpty());
    }
    void favoriteWriteFailureAndRetry(){
        const QString path=AppSettings().fileName();
        QVERIFY(path.startsWith(QString::fromLocal8Bit(qgetenv("AURORA_TEST_STORAGE"))+"/"));
        struct Restore {QString path;bool moved=false;~Restore(){QDir().rmdir(path);if(moved&&!QFile::exists(path))QFile::rename(path+".test-backup",path);}} restore{path};
        if(QFile::exists(path)){QVERIFY(QFile::rename(path,path+".test-backup"));restore.moved=true;}
        QVERIFY(QDir().mkdir(path));
        AddressBook book;book.toggleFavorite("fixture-error","7890123");
        QTRY_VERIFY(!book.storageError().isEmpty());QVERIFY(book.isFavorite("7890123"));
        QVERIFY(QDir().rmdir(path));
        book.retrySave();QTRY_VERIFY(book.storageError().isEmpty());
        QVERIFY(AppSettings().value("favorites").toList().size()>0);
    }
    void saveAccountWithoutConnecting(){
        QVERIFY(AccountSettings::save("sip.example.test","example.test",5061,"user",true).isEmpty());
        AppSettings settings;QCOMPARE(settings.value("host").toString(),QString("sip.example.test"));QCOMPARE(settings.value("user").toString(),QString("user"));QCOMPARE(settings.value("port").toInt(),5061);
        QVERIFY(!AccountSettings::save("bad;host","example.test",5061,"user",true).isEmpty());QCOMPARE(AppSettings().value("host").toString(),QString("sip.example.test"));
        QVERIFY(!AccountSettings::save("example.test","example.test",0,"user",true).isEmpty());
    }
    void savedAudioPreferenceSurvivesPasswordLoad(){
        const auto encryptedProfile=AccountSettings::profile("sip.example.test","example.test",5061,"user",false);
        QVERIFY(AccountSettings::save("sip.example.test","example.test",5061,"user",true).isEmpty());
        QVERIFY(AccountSettings::withLocalAudioPreference(encryptedProfile).value("srtp").toBool());
        auto otherAccount=encryptedProfile;otherAccount["user"]="other";
        QVERIFY(!AccountSettings::withLocalAudioPreference(otherAccount).value("srtp").toBool());
        QVERIFY(AccountSettings::save("sip.example.test","example.test",5061,"user",false).isEmpty());
        auto oldProfile=encryptedProfile;oldProfile["srtp"]=true;
        QVERIFY(!AccountSettings::withLocalAudioPreference(oldProfile).value("srtp").toBool());
    }
    void sources(){
        QVERIFY(ContactSource::sourceUrl("https://example.com/dir/book.xml?q=one").isValid());
        QVERIFY(ContactSource::sourceUrl("http://example.com/dir/book.xml").isValid());
        QVERIFY(ContactSource::sourceUrl("/tmp/контакты 1.xml").isLocalFile());
        QCOMPARE(ContactSource::sourceUrl("file:///tmp/a%20b.xml").toLocalFile(),QString("/tmp/a b.xml"));
        for(auto value:{"ftp://host/file.xml","https://user:password@host/book.xml","file://remote/secret.xml","relative.xml"})QVERIFY(ContactSource::sourceUrl(value).isEmpty());
    }
    void parser(){
        QVariantList rows;QString error;QVERIFY(ContactSource::parseXml(book,&rows,&error));QCOMPARE(rows.size(),2);
        QSet<QString> numbers;for(auto row:rows){numbers.insert(row.toMap()["number"].toString());QCOMPARE(row.toMap()["name"].toString(),QString::fromUtf8("Иван & Анна"));}
        QVERIFY(numbers.contains("849"));QVERIFY(numbers.contains("+79001234567"));
        QVERIFY(ContactSource::parseXml("<YealinkIPPhoneDirectory/>",&rows,&error));QVERIFY(rows.isEmpty());
        QVERIFY(ContactSource::parseXml("<YealinkIPPhoneDirectory><DirectoryEntry><Telephone>850</Telephone></DirectoryEntry></YealinkIPPhoneDirectory>",&rows,&error));QCOMPARE(rows[0].toMap()["name"].toString(),QString("850"));
    }
    void badXml_data(){
        QTest::addColumn<QByteArray>("xml");
        QTest::newRow("html")<<QByteArray("<html><body>login</body></html>");
        QTest::newRow("truncated")<<book.left(book.size()-20);
        QTest::newRow("trailing-data")<<book+"<bad/>";
        QTest::newRow("dtd")<<QByteArray("<!DOCTYPE YealinkIPPhoneDirectory [<!ENTITY secret SYSTEM 'file:///etc/passwd'>]><YealinkIPPhoneDirectory><DirectoryEntry><Name>&secret;</Name><Telephone>123</Telephone></DirectoryEntry></YealinkIPPhoneDirectory>");
        QTest::newRow("menu")<<QByteArray("<YealinkIPPhoneDirectory><MenuItem><URL>http://host/other.xml</URL></MenuItem></YealinkIPPhoneDirectory>");
        QTest::newRow("oversize")<<QByteArray(2*1024*1024+1,'x');
    }
    void badXml(){QFETCH(QByteArray,xml);QVariantList rows;QString error;QVERIFY(!ContactSource::parseXml(xml,&rows,&error));QVERIFY(!error.isEmpty());}
    void localFiles(){
        QTemporaryDir dir;QString path=dir.path()+"/контакты test.xml";QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(book);file.close();
        ContactSource client;client.configure(path);waitReady(client);QCOMPARE(client.contacts().size(),2);QCOMPARE(client.source(),path);
        QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));file.write("<bad");file.close();
        client.refresh();waitReady(client);QCOMPARE(client.contacts().size(),2);QVERIFY(client.status().contains("XML"));
        client.configure("");QVERIFY(client.contacts().isEmpty());QVERIFY(client.source().isEmpty());
        client.configure(dir.path()+"/missing.xml");waitReady(client);QVERIFY(client.status().contains(QString::fromUtf8("открыть")));
    }
    void refreshCompletionForGesture(){
        XmlServer server;server.responses={XmlServer::response(book),XmlServer::response("",503)};
        ContactSource client(nullptr,1000,120);QSignalSpy completed(&client,&ContactSource::refreshFinished);
        client.configure(server.url());QTRY_COMPARE(completed.count(),1);QVERIFY(completed.at(0).at(0).toBool());
        client.refresh();QTRY_COMPARE(completed.count(),2);QVERIFY(!completed.at(1).at(0).toBool());QCOMPARE(client.contacts().size(),2);
        server.stall=true;client.refresh();QVERIFY(client.loading());client.cancel();QCOMPARE(completed.count(),3);QVERIFY(!completed.at(2).at(0).toBool());
        client.refresh();QTRY_COMPARE(completed.count(),4);QVERIFY(!completed.at(3).at(0).toBool());QVERIFY(!client.loading());
        QTest::qWait(80);QCOMPARE(completed.count(),4);
    }
    void networkAndRedirect(){
        XmlServer server;server.responses={XmlServer::response("",302,"Location: /contacts.xml\r\n"),XmlServer::response(book),XmlServer::response("",503),XmlServer::response(book)};
        ContactSource client;client.configure(server.url());waitReady(client);QCOMPARE(client.contacts().size(),2);QCOMPARE(server.requests.size(),2);QVERIFY(server.requests[1].startsWith("GET /contacts.xml "));
        for(auto request:server.requests)QVERIFY(!request.toLower().contains("authorization:"));
        client.refresh();waitReady(client);QCOMPARE(client.contacts().size(),2);QVERIFY(client.status().contains("503"));
        client.refresh();waitReady(client);QCOMPARE(client.contacts().size(),2);QVERIFY(client.status().contains(QString::fromUtf8("обновлены")));
    }
    void redirectProtection(){
        XmlServer server;server.responses={XmlServer::response("",302,"Location: file:///etc/passwd\r\n")};
        ContactSource client;client.configure(server.url());waitReady(client);QVERIFY(client.contacts().isEmpty());QVERIFY(client.status().contains(QString::fromUtf8("перенаправление")));
    }
    void tlsAndDowngrade(){
        XmlServer http,https(true);https.responses={XmlServer::response(book),XmlServer::response("",302,"Location: "+http.url().toUtf8()+"\r\n")};
        ContactSource client;client.configure(https.url());waitReady(client);QCOMPARE(client.contacts().size(),2);
        client.refresh();waitReady(client);QCOMPARE(client.contacts().size(),2);QCOMPARE(http.requests.size(),0);
        client.configure(https.url().replace("localhost","127.0.0.1"));waitReady(client);QVERIFY(client.status().contains("TLS"));QCOMPARE(https.requests.size(),2);
    }
    void stalledServerAndRecovery(){
        XmlServer server;server.stall=true;ContactSource client(nullptr,1000,120);
        int heartbeat=0;QTimer timer;timer.setInterval(10);connect(&timer,&QTimer::timeout,this,[&]{++heartbeat;});timer.start();
        client.configure(server.url());QTRY_VERIFY(!client.loading());
        QVERIFY(heartbeat>3);QVERIFY(client.status().contains(QString::fromUtf8("не отвечает")));
        server.stall=false;server.responses={XmlServer::response(book)};
        client.refresh();QTRY_VERIFY(!client.loading());QCOMPARE(client.contacts().size(),2);
    }
    void absoluteDeadline(){
        XmlServer server;server.slow=true;ContactSource client(nullptr,300,150);
        client.configure(server.url());QTRY_VERIFY(!client.loading());
        QVERIFY(client.status().contains(QString::fromUtf8("истекло")));
    }
    void cancelAndRestart(){
        XmlServer server;server.stall=true;ContactSource client(nullptr,1000,500);
        client.configure(server.url());QTRY_COMPARE(server.requests.size(),1);QVERIFY(client.loading());
        client.cancel();QVERIFY(!client.loading());
        server.stall=false;server.responses={XmlServer::response(book)};client.refresh();
        QTRY_VERIFY(!client.loading());QCOMPARE(client.contacts().size(),2);
        QTest::qWait(550);QCOMPARE(client.contacts().size(),2);QVERIFY(client.status().contains(QString::fromUtf8("обновлены")));
    }
    void cancelBeforeDeferredLoad(){
        XmlServer server;server.stall=true;ContactSource client;
        client.configure(server.url());client.cancel();
        QTemporaryDir dir;QFile file(dir.path()+"/book.xml");QVERIFY(file.open(QIODevice::WriteOnly));file.write(book);file.close();
        client.configure(file.fileName());QTRY_VERIFY(!client.loading());QCOMPARE(client.contacts().size(),2);QCOMPARE(server.requests.size(),0);
    }
    void persistentCopySurvivesRestartAndErrors(){
        XmlServer server;server.responses={XmlServer::response(book)};
        {ContactSource first;first.configure(server.url());waitReady(first);QCOMPARE(first.contacts().size(),2);QVERIFY(!first.usingSavedCopy());}
        QFile cache(cachedPath(server.url()));QVERIFY(cache.exists());
        QVERIFY(!(cache.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)));
        server.stall=true;ContactSource second(nullptr,1000,100);
        QCOMPARE(second.contacts().size(),2);QVERIFY(second.usingSavedCopy());QVERIFY(!second.cacheStatus().isEmpty());
        second.refresh();waitReady(second);QCOMPARE(second.contacts().size(),2);QVERIFY(second.usingSavedCopy());
        server.stall=false;server.responses={XmlServer::response("<broken/>")};second.refresh();waitReady(second);
        {ContactSource third;QCOMPARE(third.contacts().size(),2);QVERIFY(third.usingSavedCopy());}
    }
    void copiesAreBoundToSource(){
        XmlServer a,b;a.responses={XmlServer::response(book)};
        b.responses={XmlServer::response("<YealinkIPPhoneDirectory><DirectoryEntry><Name>Other</Name><Telephone>850</Telephone></DirectoryEntry></YealinkIPPhoneDirectory>")};
        ContactSource client(nullptr,1000,100);client.configure(a.url());waitReady(client);QCOMPARE(client.contacts().size(),2);
        client.configure(b.url());waitReady(client);QCOMPARE(client.contacts().size(),1);
        a.stall=b.stall=true;
        client.configure(a.url());QCOMPARE(client.contacts().size(),2);waitReady(client);QVERIFY(client.usingSavedCopy());
        client.configure(b.url());QCOMPARE(client.contacts().size(),1);waitReady(client);QVERIFY(client.usingSavedCopy());
        XmlServer unknown;unknown.stall=true;client.configure(unknown.url());waitReady(client);QVERIFY(client.contacts().isEmpty());QVERIFY(!client.usingSavedCopy());
    }
    void emptyCopyReplacesOldDirectory(){
        XmlServer server;server.responses={XmlServer::response(book),XmlServer::response("<YealinkIPPhoneDirectory/>")};
        ContactSource client;client.configure(server.url());waitReady(client);client.refresh();waitReady(client);QVERIFY(client.contacts().isEmpty());
        ContactSource restarted;QVERIFY(restarted.contacts().isEmpty());QVERIFY(restarted.usingSavedCopy());
    }
    void corruptCopyIsIgnored(){
        XmlServer server;server.responses={XmlServer::response(book)};
        {ContactSource first;first.configure(server.url());waitReady(first);}
        QFile file(cachedPath(server.url()));QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));file.write("<broken");file.close();
        ContactSource next;QVERIFY(next.contacts().isEmpty());QVERIFY(!next.usingSavedCopy());
        server.responses={XmlServer::response(book)};next.refresh();waitReady(next);QCOMPARE(next.contacts().size(),2);
        ContactSource recovered;QCOMPARE(recovered.contacts().size(),2);QVERIFY(recovered.usingSavedCopy());
    }
    void contactNames(){
        QTemporaryDir dir;QString path=dir.path()+"/names.xml";QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(book);file.close();
        AddressBook addressBook;
        addressBook.toggleFavorite("Saved favorite","849");
        QCOMPARE(addressBook.nameForNumber("849"),QString("Saved favorite"));
        auto revision=addressBook.namesRevision();
        addressBook.directory()->configure(path);QTRY_VERIFY(!addressBook.loading());
        QVERIFY(addressBook.namesRevision()>revision);
        QCOMPARE(addressBook.nameForNumber("849"),QString::fromUtf8("Иван & Анна"));
        QCOMPARE(addressBook.nameForNumber("\"Caller\" <sip:849@pbx.example>"),QString::fromUtf8("Иван & Анна"));
        QCOMPARE(addressBook.nameForNumber("+7 (900) 123-45-67"),QString::fromUtf8("Иван & Анна"));
        QVERIFY(addressBook.nameForNumber("84").isEmpty());QVERIFY(addressBook.nameForNumber("1849").isEmpty());
        QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));file.write("<YealinkIPPhoneDirectory><DirectoryEntry><Name>Renamed contact</Name><Telephone>849</Telephone></DirectoryEntry></YealinkIPPhoneDirectory>");file.close();
        addressBook.refresh();QTRY_VERIFY(!addressBook.loading());QCOMPARE(addressBook.nameForNumber("849"),QString("Renamed contact"));
        addressBook.directory()->configure("");QCOMPARE(addressBook.nameForNumber("849"),QString("Saved favorite"));
        addressBook.toggleFavorite("Saved favorite","849");QVERIFY(addressBook.nameForNumber("849").isEmpty());
    }
    void durationFormatting(){
        QFile file(qgetenv("HISTORY_JS"));QVERIFY(file.open(QIODevice::ReadOnly));QString script=QString::fromUtf8(file.readAll());script.remove(".pragma library");QJSEngine js;QVERIFY(!js.evaluate(script).isError());
        QCOMPARE(js.evaluate("duration(0)").toString(),QString("0:00"));QCOMPARE(js.evaluate("duration(65)").toString(),QString("1:05"));QCOMPARE(js.evaluate("duration(3661)").toString(),QString("1:01:01"));
        QCOMPARE(js.evaluate(QString::fromUtf8("details({result:'Разговор',durationSeconds:125})")).toString(),QString::fromUtf8("Разговор · 2:05"));
        QCOMPARE(js.evaluate(QString::fromUtf8("details({result:'Разговор'})")).toString(),QString::fromUtf8("Разговор"));
        QCOMPARE(js.evaluate(QString::fromUtf8("details({result:'Без ответа',durationSeconds:0})")).toString(),QString::fromUtf8("Без ответа"));
    }
};
QTEST_GUILESS_MAIN(ContactSourceTest)
#include "contactsource_test.moc"
