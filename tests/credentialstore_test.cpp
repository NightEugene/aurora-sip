#include "credentialstore.h"
#include "credentialpayload.h"
#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <cstdio>
static QVariantMap fixtureAccount(){return AccountSettings::profile("sip.example.test","example.test",5061,"fixture-user",true);}
class CredentialTest : public QObject {
 Q_OBJECT
private slots:
 void success(){
  qputenv("FAKE_KEYSTORE_MODE","ok");CredentialStore store;QSignalSpy loaded(&store,&CredentialStore::loaded),done(&store,&CredentialStore::completed);
  store.load();QVERIFY(store.isRunning());QTRY_COMPARE(done.count(),1);QVERIFY(done.at(0).at(0).toBool());QCOMPARE(loaded.at(0).at(0).toString(),QString("fixture"));QVERIFY(!store.isRunning());
 }
 void boundProfileCrossesWorkerBoundary(){qputenv("FAKE_KEYSTORE_MODE","bound");CredentialStore store;QSignalSpy loaded(&store,&CredentialStore::loaded),done(&store,&CredentialStore::completed);store.load();QTRY_COMPARE(done.count(),1);QVERIFY(done.at(0).at(0).toBool());QCOMPARE(loaded.at(0).at(1).toMap(),fixtureAccount());}
 void failure(){qputenv("FAKE_KEYSTORE_MODE","fail");CredentialStore store;QSignalSpy done(&store,&CredentialStore::completed),loaded(&store,&CredentialStore::loaded);store.load();QTRY_COMPARE(done.count(),1);QVERIFY(!done.at(0).at(0).toBool());QCOMPARE(loaded.count(),0);}
 void timeoutAndRetry(){
  qputenv("FAKE_KEYSTORE_MODE","hang");CredentialStore store(nullptr,100);QSignalSpy done(&store,&CredentialStore::completed);int ticks=0;QTimer heartbeat;heartbeat.setInterval(5);connect(&heartbeat,&QTimer::timeout,this,[&]{++ticks;});heartbeat.start();
  store.load();QTRY_COMPARE(done.count(),1);QVERIFY(!done.at(0).at(0).toBool());QVERIFY(ticks>5);QTRY_VERIFY(!store.isRunning());QTest::qWait(80);QCOMPARE(done.count(),1);
  qputenv("FAKE_KEYSTORE_MODE","ok");store.load();QTRY_COMPARE(done.count(),2);QVERIFY(done.at(1).at(0).toBool());
 }
 void closeDuringHang(){qputenv("FAKE_KEYSTORE_MODE","hang");QElapsedTimer clock;clock.start();{CredentialStore store;store.load();QTest::qWait(30);}QVERIFY(clock.elapsed()<1500);}
 void invalidSaveDoesNotLoadOldPassword(){qputenv("FAKE_KEYSTORE_MODE","ok");CredentialStore store;QSignalSpy done(&store,&CredentialStore::completed),loaded(&store,&CredentialStore::loaded);store.save(QString(),fixtureAccount());QCOMPARE(done.count(),1);QVERIFY(!done.at(0).at(0).toBool());store.save(QString(20000,'x'),fixtureAccount());QCOMPARE(done.count(),2);QVERIFY(!done.at(1).at(0).toBool());QCOMPARE(loaded.count(),0);QVERIFY(!store.isRunning());}
 void payloadBindsAccountAndPassword(){
  const auto account=fixtureAccount();const auto payload=CredentialPayload::encode("пароль-\"fixture",account);QString password;QVariantMap restored;
  QVERIFY(CredentialPayload::decode(2,payload,&password,&restored));QCOMPARE(password,QString::fromUtf8("пароль-\"fixture"));QCOMPARE(restored,account);
  QVERIFY(!CredentialPayload::decode(1,payload,&password,&restored));QVERIFY(password.isEmpty());QVERIFY(restored.isEmpty());
  QVERIFY(CredentialPayload::decode(1,QByteArray("{legacy-password}"),&password,&restored));QCOMPARE(password,QString("{legacy-password}"));QVERIFY(restored.isEmpty());
  QVERIFY(!CredentialPayload::decode(2,QByteArray("old-password"),&password,&restored));
  QVERIFY(!CredentialPayload::decode(2,CredentialPayload::prefix()+"{}",&password,&restored));
  QVERIFY(!CredentialPayload::decode(2,CredentialPayload::encode("fixture",{}),&password,&restored));
 }
 void profileRejectsNewlineInjection(){auto account=fixtureAccount();account["host"]="sip.example.test\n";QVERIFY(!AccountSettings::validate(account).isEmpty());account=fixtureAccount();account["user"]="fixture-user\n";QVERIFY(!AccountSettings::validate(account).isEmpty());}
 void invalidAccountNeverStartsWorker(){qputenv("FAKE_KEYSTORE_MODE","save");CredentialStore store;QSignalSpy done(&store,&CredentialStore::completed);store.save("fixture-save",{});QCOMPARE(done.count(),1);QVERIFY(!done.at(0).at(0).toBool());QVERIFY(!store.isRunning());}
 void secretUsesStdin(){qputenv("FAKE_KEYSTORE_MODE","save");CredentialStore store;QSignalSpy done(&store,&CredentialStore::completed);store.save("fixture-save",fixtureAccount());QTRY_COMPARE(done.count(),1);QVERIFY(done.at(0).at(0).toBool());}
};
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);
 if(argc==2&&QByteArray(argv[1])=="--credential-worker"){
  QFile in;in.open(stdin,QIODevice::ReadOnly);auto req=QJsonDocument::fromJson(in.readAll()).object();auto mode=qgetenv("FAKE_KEYSTORE_MODE");
  if(mode=="hang"){QThread::sleep(30);return 1;}
  QJsonObject out;out["success"]=mode!="fail"&&(mode!="save"||(req.value("password").toString()=="fixture-save"&&req.value("account").toObject().toVariantMap()==fixtureAccount()));out["message"]="fixture";if(mode=="ok"||mode=="bound")out["password"]="fixture";if(mode=="bound")out["account"]=QJsonObject::fromVariantMap(fixtureAccount());
  QFile f;f.open(stdout,QIODevice::WriteOnly);f.write(QJsonDocument(out).toJson());f.flush();return 0;
 }
 CredentialTest test;return QTest::qExec(&test,argc,argv);
}
#include "credentialstore_test.moc"
