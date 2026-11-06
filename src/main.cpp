#include <auroraapp.h>
#include <QtQuick>
#include "phone.h"
#include "appsettings.h"
#include "addressbook.h"
#ifdef AURORA_SIP_DIAGNOSTICS
#include "accountipc.h"
#include <QDBusConnection>
#endif
#include "keypadtone.h"
#include "shutdownguard.h"
int main(int argc, char **argv) {
 if(argc==2 && QByteArray(argv[1])=="--credential-worker")return CredentialStore::workerMain(argc,argv);
 ShutdownGuard shutdown;
 // CallService owns aliencall resources; disable Qt auto resource acquisition.
 qputenv("NEMO_RESOURCE_DISABLE", "1");
 QCA::Initializer qca;
 QScopedPointer<QGuiApplication> app(Aurora::Application::application(argc, argv));
 app->setOrganizationName("ru.nighteugene"); app->setApplicationName("aurorasip");
 app->setApplicationDisplayName(QLocale().language()==QLocale::Russian ? QStringLiteral("Аврора SIP") : QStringLiteral("Aurora SIP"));
 app->setApplicationVersion("1.0.0");
 AppSettings::dataDirectory();
 Phone phone;
 qmlRegisterUncreatableType<ContactSource>("AuroraSIP",1,0,"ContactSource","Owned by AddressBook");
 AddressBook addressBook;
 QTimer::singleShot(0,&addressBook,&AddressBook::refresh);
 KeypadTone keypadTone;
 #ifdef AURORA_SIP_DIAGNOSTICS
 AccountIpc accountIpc(&phone);
 accountIpc.setAddressBook(&addressBook);
 auto bus=QDBusConnection::sessionBus();
 if(bus.registerService("ru.nighteugene.aurorasip.Account"))
  bus.registerObject("/Account",&accountIpc,QDBusConnection::ExportAllSlots);
 #endif
 QScopedPointer<QQuickView> view(Aurora::Application::createView());
 view->rootContext()->setContextProperty("appNavigation",static_cast<QObject*>(nullptr));
 view->rootContext()->setContextProperty("keypadTone", &keypadTone);
 view->rootContext()->setContextProperty("phone", &phone);
 view->rootContext()->setContextProperty("addressBook", &addressBook);
 view->rootContext()->setContextProperty("appTitle", app->applicationDisplayName());
 view->rootContext()->setContextProperty("appVersion", app->applicationVersion());
 view->rootContext()->setContextProperty("appAuthorName", QStringLiteral("Eugene Todoruk"));
 view->rootContext()->setContextProperty("appAuthorEmail", QStringLiteral("nighteugene@ya.ru"));
 // Compile menu pages too: they are otherwise checked only on first navigation.
 const QStringList menuPages={"Settings.qml","ContactsSettings.qml","DialerPage.qml","HistoryPage.qml","ContactsPage.qml","FavoritesPage.qml","About.qml"};
 for(const QString &page:menuPages){
  QQmlComponent component(view->engine(),Aurora::Application::pathTo("qml/"+page));
  if(component.isError())qWarning().noquote()<<"QML page failed:"<<page<<component.errorString();
  else if(component.isReady())qInfo().noquote()<<"QML page ready:"<<page;
 }
 view->setSource(Aurora::Application::pathTo("qml/Main.qml"));
 view->rootContext()->setContextProperty("appNavigation",view->rootObject());
 #ifdef AURORA_SIP_DIAGNOSTICS
 accountIpc.setView(view.data());
 #endif
 view->show();
 const int result=app->exec();
 shutdown.arm();
 return result;
}
