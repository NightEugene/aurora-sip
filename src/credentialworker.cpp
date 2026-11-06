#include "credentialworker.h"
#include "appsettings.h"
#include "credentialpayload.h"
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QSemaphore>
#include <QElapsedTimer>
#include <QDebug>

static const QString provider=QStringLiteral("qca-aurora-pkcs11");
static const QString keyName=QStringLiteral("Aurora SIP account encryption v1");
class CredentialEvents : public QThread {
public:
    QSemaphore ready;
    void run() override {
    QCA::EventHandler handler;
    QObject::connect(&handler,&QCA::EventHandler::eventReady,&handler,[&handler](int id,const QCA::Event &e){
        switch(e.type()) {
        case QCA::Event::Password: handler.submitPassword(id,QCA::SecureArray("0000")); break;
        case QCA::Event::KeyStoreId: handler.submitKeyStoreId(id,"USERAPP"); break;
        case QCA::Event::KeyStoreName: handler.submitKeyStoreName(id,"Aurora SIP"); break;
        case QCA::Event::KeyStoreEntryName: handler.submitKeyStoreEntryName(id,keyName); break;
        case QCA::Event::KeyType: handler.submitKeyType(id,"aes"); break;
        case QCA::Event::KeyStorePkcs11Provider:
            if(QFile::exists("/etc/kcapi/qca_aurora_pkcs11/cktee")) handler.submitKeyStorePkcs11Provider(id,"cktee");
            else if(QFile::exists("/etc/kcapi/qca_aurora_pkcs11/Aurora-SoftHSM")) handler.submitKeyStorePkcs11Provider(id,"Aurora-SoftHSM");
            else handler.reject(id);
            break;
        default: handler.reject(id); break;
        }
    });
    handler.start();
    ready.release();
    exec();
    }
};
CredentialWorker::CredentialWorker(QObject *parent):QThread(parent) {
    auto worker=new CredentialEvents;events=worker;worker->start();
}
CredentialWorker::~CredentialWorker(){if(isRunning()){QEventLoop loop;connect(this,&QThread::finished,&loop,&QEventLoop::quit);if(isRunning())loop.exec();wait();}events->quit();events->wait();delete events;}
void CredentialWorker::save(const QString &p,const QVariantMap &account){if(isRunning())return;pending=p;pendingAccount=account;writing=true;start();}
void CredentialWorker::load(){if(isRunning())return;writing=false;start();}
void CredentialWorker::run(){
    if(writing&&(!AccountSettings::validate(pendingAccount).isEmpty()||pending.isEmpty())){pending.fill(QChar(0));pending.clear();emit completed(false,"Некорректный аккаунт");return;}
    QElapsedTimer elapsed;elapsed.start();
    const auto report=[this,&elapsed](const QString &message){
        qInfo().noquote()<<"CredentialStore phase"<<elapsed.elapsed()<<message;
        emit progress(message);
    };
    // Wait in the crypto worker, never in the GUI constructor.
    static_cast<CredentialEvents *>(events)->ready.acquire();
    static_cast<CredentialEvents *>(events)->ready.release();
    report("Keystore: запуск провайдера…");
    auto fail=[this](const QString &message){pending.fill(QChar(0));pending.clear();emit completed(false,message);};
    QCA::SymmetricKey key;
    int matches=0;
    QCA::KeyStoreManager manager;
    QScopedPointer<QCA::KeyStore> store;
    QCA::KeyStoreManager::start(provider);
    manager.sync();
    report("Keystore: открытие USERAPP…");
    if(!manager.keyStores().contains("USERAPP")){fail("Keystore USERAPP недоступен");return;}
    store.reset(new QCA::KeyStore("USERAPP",&manager));
    store->switchOperatingMode(QCA::KeyStore::RWUserPKCS11);
    report("Keystore: проверка контейнера…");
    if(store->name()==QString(32,'*')){
        if(!writing){fail("Хранилище ключа пароля недоступно");return;}
        store.reset();
        if(manager.initKeyStore("USERAPP").isEmpty()){fail("Не удалось создать Keystore");return;}
        manager.sync();
        store.reset(new QCA::KeyStore("USERAPP",&manager));
        store->switchOperatingMode(QCA::KeyStore::RWUserPKCS11);
    }
    report("Keystore: открытие ключа…");
    if(!store->isValid()){fail("Keystore не готов");return;}
    // Official example documents incorrect entry.name() on this platform.
    // This application's USERAPP token owns a single AES-256 key.
    report("Keystore: список ключей…");
    for(auto e:store->entryList()) {
        if(e.type()!=QCA::KeyStoreEntry::TypeSymmetricKey)continue;
        report("Keystore: доступ к ключу…");
        if(!e.ensureAccess()){fail("Нет доступа к ключу аккаунта");return;}
        report("Keystore: чтение ключа…");
        const auto candidate=e.symmetricKey();
        if(candidate.size()==32){key=candidate;++matches;}
    }
    if(matches>1){fail("В Keystore найдено несколько ключей аккаунта");return;}
    report("Keystore: обработка пароля…");
    if((key.size()==0)&&writing)key=QCA::SymmetricKey(32,provider);
    if((key.size()==0)){fail("Ключ расшифровки пароля недоступен");return;}
    QString dir=AppSettings::dataDirectory();
    QDir().mkpath(dir);
    QString path=dir+"/omp-credentials.v1.json";
    if(writing){
        QCA::InitializationVector nonce(12);
        QCA::Cipher cipher("aes256",QCA::Cipher::GCM,QCA::Cipher::NoPadding,QCA::Encode,key,nonce,QCA::AuthTag(16),provider);
        QByteArray payload=CredentialPayload::encode(pending,pendingAccount);
        QByteArray encrypted=cipher.update(QCA::SecureArray(payload)).toByteArray();payload.fill(0);
        encrypted+=cipher.final().toByteArray();
        pending.fill(QChar(0));pending.clear();
        if(!cipher.ok()){fail("Не удалось зашифровать пароль в Keystore");return;}
        QJsonObject envelope;envelope["version"]=2;envelope["nonce"]=QString::fromLatin1(nonce.toByteArray().toBase64());
        envelope["ciphertext"]=QString::fromLatin1(encrypted.toBase64());envelope["tag"]=QString::fromLatin1(cipher.tag().toByteArray().toBase64());
        QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly)){fail("Не удалось сохранить аккаунт");return;}
        if(!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)){fail("Не удалось защитить файл аккаунта");return;}
        QByteArray data=QJsonDocument(envelope).toJson(QJsonDocument::Compact);
        if(file.write(data)!=data.size()||!file.commit()){fail("Ошибка записи аккаунта");return;}
        emit completed(true,"Пароль сохранён в Keystore");
    } else {
        QFile file(path);
        if(!file.exists()){fail("Сохранённого пароля нет");return;}
        if(!file.open(QIODevice::ReadOnly)){fail("Не удалось прочитать сохранённый пароль");return;}
        if(file.size()>16384){fail("Повреждён файл сохранённого пароля");return;}
        QJsonObject o=QJsonDocument::fromJson(file.readAll()).object();
        auto bytes=[&o](const char *name){return QByteArray::fromBase64(o[name].toString().toLatin1());};
        QByteArray nonce=bytes("nonce"),tag=bytes("tag"),data=bytes("ciphertext");
        if((o["version"].toInt()!=1&&o["version"].toInt()!=2)||nonce.size()!=12||tag.size()!=16||data.isEmpty()){fail("Повреждён файл аккаунта");return;}
        QCA::Cipher cipher("aes256",QCA::Cipher::GCM,QCA::Cipher::NoPadding,QCA::Decode,key,QCA::InitializationVector(nonce),QCA::AuthTag(tag),provider);
        QCA::SecureArray plain=cipher.update(QCA::SecureArray(data));
        plain.append(cipher.final());
        if(!cipher.ok()){fail("Не удалось проверить и расшифровать аккаунт");return;}
        QString password;QVariantMap account;QByteArray decoded=plain.toByteArray();
        const bool valid=CredentialPayload::decode(o["version"].toInt(),decoded,&password,&account);decoded.fill(0);
        if(!valid){fail("Повреждены параметры сохранённого аккаунта");return;}
        emit loaded(password,account);password.fill(QChar(0));
        qInfo()<<"CredentialStore loaded in ms"<<elapsed.elapsed();
        emit completed(true,"Пароль загружен");
    }
}

#include "credentialstore.h"
#include <QCoreApplication>
#include <cstdio>
int CredentialStore::workerMain(int argc,char **argv){
 QCoreApplication app(argc,argv);app.setOrganizationName("ru.nighteugene");app.setApplicationName("aurorasip");
 QCA::Initializer qca;
 QFile input;input.open(stdin,QIODevice::ReadOnly);
 const auto requestData=input.read(12001);if(requestData.size()>12000)return 2;
 const auto request=QJsonDocument::fromJson(requestData).object();
 if(!request.value("writing").isBool()||!request.value("password").isString())return 2;
 CredentialWorker worker;
 QJsonObject result;QString secret;QVariantMap account;
 QObject::connect(&worker,&CredentialWorker::loaded,&app,[&](const QString &password,const QVariantMap &profile){secret=password;account=profile;});
 QObject::connect(&worker,&CredentialWorker::completed,&app,[&](bool ok,const QString &message){result["success"]=ok;result["message"]=message;if(ok&&!request.value("writing").toBool()){result["password"]=secret;result["account"]=QJsonObject::fromVariantMap(account);}});
 QObject::connect(&worker,&QThread::finished,&app,&QCoreApplication::quit);
 if(request.value("writing").toBool())worker.save(request.value("password").toString(),request.value("account").toObject().toVariantMap());else worker.load();
 app.exec();worker.wait();
 QFile output;output.open(stdout,QIODevice::WriteOnly);output.write(QJsonDocument(result).toJson(QJsonDocument::Compact));output.flush();secret.fill(QChar(0));
 return result.value("success").toBool()?0:1;
}
