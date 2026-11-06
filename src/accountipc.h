#pragma once
#include "phone.h"
#include "addressbook.h"
#include <QQuickItemGrabResult>
#include <QStandardPaths>
#include <QDir>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QTouchDevice>
#include <QDateTime>
#include <QTimer>
#include "appsettings.h"
#include <QQuickView>
#include <QQuickItem>
#include <QGuiApplication>
#include <QInputMethod>
#include <QSslSocket>
#include <QSslCertificate>
class AccountIpc : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "ru.nighteugene.aurorasip.Account")
public:
    AccountIpc(Phone *phone, QObject *parent=nullptr):QObject(parent),phone(phone){testTouchDevice.setType(QTouchDevice::TouchScreen);testTouchDevice.setCapabilities(QTouchDevice::Position);}
    void setView(QQuickView *value){view=value;}
    void setAddressBook(AddressBook *value){book=value;}
public slots:
    QString TlsProbeStatus() const {return tlsProbeStatus;}
    void ProbeTls(){
        if(probe)return;
        tlsProbeStatus="Проверка TLS…";
        auto socket=new QSslSocket(this);probe=socket;
        connect(socket,static_cast<void(QSslSocket::*)(const QList<QSslError> &)>(&QSslSocket::sslErrors),this,[this](const QList<QSslError> &errors){
            QStringList messages;for(const auto &error:errors)messages.append(error.errorString());
            tlsProbeStatus="TLS: "+messages.join("; ");
        });
        connect(socket,&QSslSocket::encrypted,this,[this,socket]{
            const auto certificate=socket->peerCertificate();
            tlsProbeStatus="TLS OK; CN="+certificate.subjectInfo(QSslCertificate::CommonName).join(",")+"; DNS="+QStringList(certificate.subjectAlternativeNames().values(QSsl::DnsEntry)).join(",");
            socket->disconnectFromHost();
        });
        connect(socket,static_cast<void(QAbstractSocket::*)(QAbstractSocket::SocketError)>(&QAbstractSocket::error),this,[this,socket](QAbstractSocket::SocketError){
            if(tlsProbeStatus=="Проверка TLS…")tlsProbeStatus=socket->errorString();
        });
        connect(socket,&QSslSocket::disconnected,this,[this,socket]{if(probe==socket)probe=nullptr;socket->deleteLater();});
        QTimer::singleShot(15000,socket,[this,socket]{
            if(probe!=socket)return;
            if(tlsProbeStatus=="Проверка TLS…")tlsProbeStatus="TLS: время ожидания истекло";
            probe=nullptr;socket->abort();socket->deleteLater();
        });
        socket->connectToHostEncrypted(phone->setting("host","voice.omp.ru").toString(),phone->setting("port",5061).toUInt());
    }
    void SetMenuOpen(bool opened){
        if(!view||!view->rootObject())return;
        auto bar=findActiveNavigationBar(view->rootObject());
        if(bar)QMetaObject::invokeMethod(bar,opened?"showMenu":"hideMenu");
    }
    QVariantMap MenuState() const {
        QVariantMap state;if(!view||!view->rootObject())return state;
        auto bar=findActiveNavigationBar(view->rootObject());state["activeBarFound"]=bar!=nullptr;
        auto menu=bar?findVisual(bar,"navigationMenu"):nullptr;
        state["opened"]=menu&&menu->property("opened").toBool();
        state["mainPageVisible"]=view->rootObject()->property("mainPageVisible");
        return state;
    }
    void OpenAbout(){if(view&&view->rootObject())QMetaObject::invokeMethod(view->rootObject(),"openAbout");}
    QStringList MenuLayers() const {
        QStringList result;if(view&&view->rootObject())collectMenuLayers(view->rootObject(),result);return result;
    }
    void OpenAccount(){if(view&&view->rootObject())QMetaObject::invokeMethod(view->rootObject(),"openAccount");}
    void OpenContactsSettings(){if(view&&view->rootObject())QMetaObject::invokeMethod(view->rootObject(),"openContactsSettings");}
    void OpenContactsFilePicker(){
        if(!view||!view->rootObject())return;
        auto page=findVisual(view->rootObject(),"contactsSettingsPage");
        if(page)QMetaObject::invokeMethod(page,"chooseContactsFile");
    }
    void CloseAccount(){if(view&&view->rootObject())QMetaObject::invokeMethod(view->rootObject(),"closeAccount");}
    QVariantMap PasswordFieldState() const {
        QVariantMap state;if(!view||!view->rootObject())return state;
        auto field=findVisual(view->rootObject(),"accountPassword");
        state["fieldFound"]=field!=nullptr;
        if(field){state["hasPassword"]=!field->property("text").toString().isEmpty();state["manuallyEdited"]=field->property("edited");}
        state["loading"]=phone->credentialsBusy();state["status"]=phone->credentialStatus();return state;
    }
    void ToggleFavorite(const QString &name,const QString &number){if(book)book->toggleFavorite(name,number);}
    QVariantMap FavoriteState(const QString &number) const {
        QVariantMap state;if(!book)return state;state["selected"]=book->isFavorite(number);state["count"]=book->favorites().size();
        if(view&&view->rootObject()){
            auto item=findVisual(view->rootObject(),"favorite-"+number);
            state["buttonFound"]=item!=nullptr;
            if(item){state["filled"]=item->property("favoriteMarked");state["icon"]=item->property("favoriteIcon").toUrl().toString();}
        }return state;
    }
    void DragPages(bool forward){
        if(!view||!view->rootObject()||phone->active())return;
        const qreal from=view->width()*(forward?0.82:0.18);
        const qreal to=view->width()*(forward?0.18:0.82);
        const qreal y=view->height()*0.3;
        sendMouse(QEvent::MouseButtonPress,QPointF(from,y),Qt::LeftButton,Qt::LeftButton);
        for(int i=1;i<=8;++i)QTimer::singleShot(i*20,this,[this,from,to,y,i]{sendMouse(QEvent::MouseMove,QPointF(from+(to-from)*i/8.0,y),Qt::NoButton,Qt::LeftButton);});
        QTimer::singleShot(180,this,[this,to,y]{sendMouse(QEvent::MouseButtonRelease,QPointF(to,y),Qt::LeftButton,Qt::NoButton);});
    }
    void FlickPages(bool forward){
        if(!view||!view->rootObject())return;
        auto pager=findVisual(view->rootObject(),"sectionPager");
        if(pager)QMetaObject::invokeMethod(pager,"flick",Q_ARG(qreal,forward?-pager->width()*2:pager->width()*2),Q_ARG(qreal,0));
    }
    void ImportOmpAccount(const QString &user,const QString &secret) {
        if(user.isEmpty()||secret.isEmpty())return;
        AppSettings s;s.setValue("host","voice.omp.ru");s.setValue("domain","voice.omp.ru");
        s.setValue("port",5061);s.setValue("user",user);s.setValue("srtp",true);
        phone->savePassword(secret);
    }
    void SetRequireSrtp(bool required){if(phone->active())return;AppSettings settings;settings.setValue("srtp",required);settings.sync();}
    QVariantMap MediaDiagnostics(){return phone->mediaDiagnostics();}
    void DialNumber(const QString &number){phone->dial(number);}
    void HangupCall(){phone->hangup();}
    bool CallActive() const {return phone->active();}
    void CheckAccount(){phone->loadPassword();}
    void ConnectSavedAccount(){phone->loginSaved();}
    void TestKeyFeedback(const QString &digit){
        if(!view||!view->rootObject()||phone->active())return;
        auto feedback=findVisual(view->rootObject(),"keypadFeedback");
        if(feedback)QMetaObject::invokeMethod(feedback,"play",Q_ARG(QVariant,QVariant(digit)));
    }
    QVariantMap FeedbackState() const {
        QVariantMap state;if(!view||!view->rootObject())return state;
        auto feedback=findVisual(view->rootObject(),"keypadFeedback");state["found"]=feedback!=nullptr;
        if(feedback)for(const auto &key:{"toneConnected","hapticConnected","tonesPlayed","hapticsPlayed","failures"})state[key]=feedback->property(key);
        return state;
    }
    void PreviewNumber(const QString &number){
        if(!view||!view->rootObject()||phone->active()||!book)return;
        auto field=findVisual(view->rootObject(),"dialNumber");
        if(field)field->setProperty("text",book->dialNumber(number));
    }
    QVariantMap ContactNameState(const QString &number) const {
        QVariantMap result;if(!book)return result;
        const QString name=book->nameForNumber(number);result["hasMatch"]=!name.isEmpty();
        auto label=view&&view->rootObject()?findVisual(view->rootObject(),"dialContactName"):nullptr;
        result["labelFound"]=label!=nullptr;
        if(label){result["labelMatches"]=!name.isEmpty()&&label->property("text").toString()==name;result["labelVisible"]=label->isVisible();}
        int namedHistory=0;for(const auto &row:phone->history())if(!book->nameForNumber(row.toMap().value("peer").toString()).isEmpty())++namedHistory;
        result["namedHistoryCount"]=namedHistory;return result;
    }
    void TouchBottomTab(int index){
        if(!view||!view->rootObject()||index<0||index>=4||phone->active())return;
        auto nav=findVisual(view->rootObject(),"bottomNavigation");if(!nav)return;
        touchTap(nav->mapToScene(QPointF(nav->width()*(index+0.5)/4,nav->height()*0.4)));
    }
    void TouchOutsideMenu(){
        if(!view||phone->active())return;
        touchTap(QPointF(view->width()*0.04,view->height()*0.62));
    }
    void TouchBack(){
        if(!view||phone->active())return;
        QPointF from(view->width()*0.04,view->height()*0.45),to(view->width()*0.9,view->height()*0.45);
        sendTouch(QEvent::TouchBegin,from,Qt::TouchPointPressed);
        for(int i=1;i<=12;++i)QTimer::singleShot(i*20,this,[this,from,to,i]{sendTouch(QEvent::TouchUpdate,from+(to-from)*(i/12.0),Qt::TouchPointMoved);});
        QTimer::singleShot(260,this,[this,to]{sendTouch(QEvent::TouchEnd,to,Qt::TouchPointReleased);});
    }
    void TapBottomTab(int index){
        if(!view||!view->rootObject()||index<0||index>=4||phone->active())return;
        auto nav=findVisual(view->rootObject(),"bottomNavigation");if(!nav)return;
        const QPointF point=nav->mapToScene(QPointF(nav->width()*(index+0.5)/4,nav->height()*0.4));
        sendMouse(QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton);
        QTimer::singleShot(50,this,[this,point]{sendMouse(QEvent::MouseButtonRelease,point,Qt::LeftButton,Qt::NoButton);});
    }
    void TapOutsideMenu(){
        if(!view||!view->rootObject()||phone->active())return;
        const QPointF point(view->width()*0.04,view->height()*0.62);
        sendMouse(QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton);
        QTimer::singleShot(50,this,[this,point]{sendMouse(QEvent::MouseButtonRelease,point,Qt::LeftButton,Qt::NoButton);});
    }
    void SelectTab(int index){
        if(!view||!view->rootObject()||index<0||index>=4)return;
        auto nav=findVisual(view->rootObject(),"bottomNavigation");
        if(nav)QMetaObject::invokeMethod(nav,"selected",Q_ARG(int,index));
    }
    QVariantMap ContactsState() const {
        QVariantMap state;if(!book)return state;
        state["loading"]=book->loading();state["status"]=book->status();
        state["filePickerOpen"]=view&&view->rootObject()&&findVisual(view->rootObject(),"contactsFilePicker");
        state["usingSavedCopy"]=book->directory()->usingSavedCopy();
        state["contactsCount"]=book->contacts().size();state["favoritesCount"]=book->favorites().size();return state;
    }
    bool CaptureDialer(){
        if(!view||!view->rootObject()||phone->active()||view->rootObject()->property("currentTab").toInt()!=0)return false;
        if(!view->rootObject()->property("mainPageVisible").toBool())return false;
        auto field=findVisual(view->rootObject(),"dialNumber");
        if(!field||!field->property("text").toString().isEmpty())return false;
        capture=view->rootObject()->grabToImage();if(!capture)return false;
        connect(capture.data(),&QQuickItemGrabResult::ready,this,[this]{
            const QString dir=AppSettings::dataDirectory();QDir().mkpath(dir);
            capture->saveToFile(dir+"/dialer-preview.png");
        });return true;
    }
    QVariantMap UiLayout() const {
        QVariantMap info;if(!view||!view->rootObject())return info;
        auto root=view->rootObject();auto footer=findVisual(root,"connectionStatusBar");
        info["active"]=view->isActive();info["exposed"]=view->isExposed();
        info["mainPageVisible"]=root->property("mainPageVisible");
        auto field=findVisual(root,"dialNumber");info["dialFieldFound"]=field!=nullptr;
        if(field)info["dialFieldEmpty"]=field->property("text").toString().isEmpty();
        info["currentTab"]=root->property("currentTab");
        auto pager=findVisual(root,"sectionPager");
        if(pager){info["pagerIndex"]=pager->property("currentIndex");info["pageCount"]=pager->property("count");info["pageOffset"]=pager->property("contentX");info["pagerInteractive"]=pager->property("interactive");info["pagerWidth"]=pager->width();}
        auto call=findVisual(root,"callButton");if(call)info["callEnabled"]=call->property("enabled");
        info["windowHeight"]=view->height();info["keyboardVisible"]=QGuiApplication::inputMethod()->isVisible();
        if(footer){info["statusVisible"]=footer->isVisible();info["statusParent"]=footer->parentItem()->objectName();info["statusY"]=footer->mapToScene(QPointF(0,0)).y();info["statusHeight"]=footer->height();info["statusBottom"]=footer->mapToScene(QPointF(0,footer->height())).y();}
        return info;
    }
    void ShowKeyboard(){if(!view||!view->rootObject())return;auto field=findVisual(view->rootObject(),"dialNumber");if(field){field->forceActiveFocus();QGuiApplication::inputMethod()->show();}}
    void HideKeyboard(){QGuiApplication::inputMethod()->hide();}
    void DisconnectAccount(){phone->logout();}
    QVariantMap ReconnectState() const {return phone->reconnectState();}
    QString ConnectionState() const {return phone->disconnecting()?"disconnecting":phone->connecting()?"connecting":phone->registered()?"connected":"disconnected";}
    QString CredentialStatus() const {return phone->credentialStatus();}
    QString Status() const {return phone->status();}
private:
    QTouchDevice testTouchDevice;
    QPointF touchStart,touchLast;
    QSslSocket *probe=nullptr;
    QString tlsProbeStatus;
    static QQuickItem *findActiveNavigationBar(QQuickItem *item){
        if(item->objectName()=="navigationBar"&&item->isVisible()&&item->isEnabled()&&item->property("_pageActive").toBool())return item;
        for(auto child:item->childItems())if(auto bar=findActiveNavigationBar(child))return bar;
        return nullptr;
    }
    static void collectMenuLayers(QQuickItem *item,QStringList &result){
        const QString type=QString::fromLatin1(item->metaObject()->className());
        if(item->objectName()=="bottomNavigation"||item->objectName()=="navigationMenu"||type.contains("Popup")){
            QStringList path;
            for(auto ancestor=item;ancestor;ancestor=ancestor->parentItem())
                path.prepend(QStringLiteral("%1(%2,z=%3,visible=%4)").arg(QString::fromLatin1(ancestor->metaObject()->className()),ancestor->objectName()).arg(ancestor->z()).arg(ancestor->isVisible()));
            result.append(path.join(" / "));
        }
        for(auto child:item->childItems())collectMenuLayers(child,result);
    }
    void touchTap(const QPointF &point){
        sendTouch(QEvent::TouchBegin,point,Qt::TouchPointPressed);
        QTimer::singleShot(70,this,[this,point]{sendTouch(QEvent::TouchEnd,point,Qt::TouchPointReleased);});
    }
    void sendTouch(QEvent::Type type,const QPointF &point,Qt::TouchPointState state){
        if(!view)return;
        QTouchDevice *device=&testTouchDevice;
        for(auto available:QTouchDevice::devices())if(available->type()==QTouchDevice::TouchScreen){device=const_cast<QTouchDevice*>(available);break;}
        if(state==Qt::TouchPointPressed)touchStart=touchLast=point;
        QTouchEvent::TouchPoint p(0);p.setState(state);p.setPos(point);p.setScenePos(point);p.setScreenPos(view->mapToGlobal(point.toPoint()));p.setStartPos(touchStart);p.setLastPos(touchLast);p.setPressure(state==Qt::TouchPointReleased?0:1);
        QTouchEvent event(type,device,Qt::NoModifier,Qt::TouchPointStates(state),QList<QTouchEvent::TouchPoint>{p});event.setTimestamp(QDateTime::currentMSecsSinceEpoch());QCoreApplication::sendEvent(view,&event);touchLast=point;
    }
    void sendMouse(QEvent::Type type,const QPointF &point,Qt::MouseButton button,Qt::MouseButtons buttons){
        if(!view)return;QMouseEvent event(type,point,button,buttons,Qt::NoModifier);
        event.setTimestamp(QDateTime::currentMSecsSinceEpoch());QCoreApplication::sendEvent(view,&event);
    }
    static QQuickItem *findVisual(QQuickItem *root,const QString &name){
        if(!root)return nullptr;
        if(root->objectName()==name)return root;
        for(auto item:root->childItems())if(auto found=findVisual(item,name))return found;
        return nullptr;
    }
    Phone *phone;
    QQuickView *view=nullptr;
    AddressBook *book=nullptr;
    QSharedPointer<QQuickItemGrabResult> capture;
};
