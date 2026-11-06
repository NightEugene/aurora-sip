#include "phone.h"
#include "appsettings.h"
#include "accountsettings.h"
#include <QDebug>
#include <QStandardPaths>
#include <pjsip/sip_transport_tls.h>
#include <QDateTime>
#include <QRegularExpression>
#include <QFile>
#include <QAudioDeviceInfo>
#include <cstring>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDir>
#include <QSaveFile>
#include <QNetworkConfigurationManager>
#include <QNetworkInterface>
#include <QSharedPointer>
#include <QtEndian>
#include <pjmedia/transport_srtp.h>
Phone *Phone::self=nullptr;
struct AudioBridgePort {pjmedia_port base;pj_pool_t *ownerPool;};
static pj_str_t str(QByteArray &s) {return pj_str(s.data());}
Phone::Phone(QObject *parent):QObject(parent) {
 self=this;
 registrationTimeout.setSingleShot(true);registrationTimeout.setInterval(20000);
 connect(&registrationTimeout,&QTimer::timeout,this,[this]{
  if(!m_connecting||m_stopping)return;
  stopSession();m_status="Сервер SIP не ответил вовремя. Повторите подключение.";emit changed();
 });
 playbackTimer.setInterval(20);
 connect(&playbackTimer,&QTimer::timeout,this,&Phone::pumpPlayback);
 connect(&credentials,&CredentialStore::loaded,this,&Phone::acceptPassword);
 connect(&credentials,&CredentialStore::progress,this,[this](const QString &message){m_credentialStatus=message;emit changed();});
 connect(&credentials,&CredentialStore::completed,this,[this](bool ok,const QString &message){
  m_credentialStatus=message;
  if(ok&&!writingAccount.isEmpty()){
   const auto error=AccountSettings::save(writingAccount);
   if(!error.isEmpty())m_credentialStatus="Аккаунт сохранён в Keystore, но не удалось обновить настройки: "+error;
  }
  writingAccount.clear();if(!ok)pendingLogin=false;emit changed();
 });
 connect(&credentials,&CredentialStore::finished,this,&Phone::changed);
 auto network=new QNetworkConfigurationManager(this);
 auto retry=new QTimer(this);retry->setSingleShot(true);retry->setInterval(1500);
 auto lastAddresses=QSharedPointer<QString>::create();
 connect(network,&QNetworkConfigurationManager::configurationChanged,this,[retry]{retry->start();});
 connect(network,&QNetworkConfigurationManager::onlineStateChanged,this,[this,retry,lastAddresses](bool online){
  if(!online){
   registrationTimeout.stop();*lastAddresses="offline";m_registered=false;m_connecting=false;m_status="Нет сетевого подключения";emit changed();
   if(reconnect.wanted()&&!active())stopSession();
  } else {retry->start();if(reconnect.wanted()&&!m_registered)reconnect.retryNow();}
 });
 connect(retry,&QTimer::timeout,this,[this,network,lastAddresses]{
  if(!network->isOnline())return;
  QStringList addresses;
  for(const auto &iface:QNetworkInterface::allInterfaces()){
   if(!(iface.flags()&QNetworkInterface::IsUp)||!(iface.flags()&QNetworkInterface::IsRunning)||(iface.flags()&QNetworkInterface::IsLoopBack))continue;
   for(const auto &entry:iface.addressEntries())if(!entry.ip().isNull())addresses.append(iface.name()+":"+entry.ip().toString());
  }
  addresses.sort();const QString signature=addresses.join(";");
  if(signature==*lastAddresses)return;
  const bool initial=lastAddresses->isEmpty();*lastAddresses=signature;
  if(initial||!reconnect.wanted())return;
  m_registered=false;m_connecting=false;m_status="Восстановление соединения…";emit changed();reconnect.retryNow();
 });
 retry->start();
 connect(&reconnect,&ReconnectPolicy::retryRequested,this,[this,network]{
  if(!reconnect.wanted()||reconnect.blocked()||m_registered)return;
  if(!network->isOnline()){m_status="Нет сетевого подключения";emit changed();reconnect.schedule();return;}
  if(m_stopping)return;
  if(active()){
   if(sipThread&&guiSipRegistered){m_connecting=true;registrationTimeout.start();sipThread->requestIpChange();}
   else reconnect.schedule();
   return;
  }
  if(sipThread||initialized){stopSession();afterStop=[this]{attemptReconnect();};}
  else attemptReconnect();
 });
 AppSettings s; m_history=s.value("history").toList();
 QTimer::singleShot(0,this,[this]{
  if(!hasSavedPassword())return;
  qInfo()<<"Automatic account registration requested";
  loginSaved();
 });
}
Phone::~Phone(){
 QObject::disconnect(&credentials,nullptr,this,nullptr);
 logout();
 if(sipThread){QObject::disconnect(sipThread,nullptr,this,nullptr);sipThread->wait();finishLogout();}
 self=nullptr;
}
QString Phone::accountName() const {return AppSettings().value("accountName",QStringLiteral("Аккаунт")).toString();}
void Phone::setAccountName(const QString &name){QString value=name.trimmed();if(value.isEmpty())value=QStringLiteral("Аккаунт");if(value==accountName()&&m_storageError.isEmpty())return;AppSettings settings;settings.setValue("accountName",value);settings.sync();m_storageError=settings.status()==QSettings::NoError?QString():QStringLiteral("Не удалось сохранить название аккаунта");emit changed();}
void Phone::setRequireSecureAudio(bool required){
 AppSettings settings;settings.setValue("srtp",required);settings.sync();
 m_storageError=settings.status()==QSettings::NoError?QString():QStringLiteral("Не удалось сохранить требование шифрования аудио");
 if(!savedAccount.isEmpty())savedAccount["srtp"]=required;
 emit changed();
}
QVariant Phone::setting(const QString &key,const QVariant &fallback) const {return AppSettings().value(key,fallback);}
void Phone::fail(pj_status_t code,const QString &what) {
 char msg[PJ_ERR_MSG_SIZE]; pj_strerror(code,msg,sizeof(msg)); stopSession(); m_status=what+": "+QString::fromUtf8(msg); emit changed();
}
void Phone::login(QString host,QString domain,int tlsPort,QString user,QString password,bool secureAudio) {
 host=host.trimmed();domain=domain.trimmed();user=user.trimmed();
 if(active()||m_stopping)return;
 if(initialized||sipThread){
  logout();
  afterStop=[this,host,domain,tlsPort,user,password,secureAudio]{login(host,domain,tlsPort,user,password,secureAudio);};
  return;
 }
 m_transportError.clear();
 if(password.isEmpty()){m_status="Пароль аккаунта не загружен";emit changed();return;}
 const auto settingsError=AccountSettings::save(host,domain,tlsPort,user,secureAudio);
 if(!settingsError.isEmpty()){m_status=settingsError;emit changed();return;}
 if(!automaticRetry){reconnect.begin();sessionPassword=QCA::SecureArray(password.toUtf8());sessionAccount=AccountSettings::profile(host,domain,tlsPort,user,secureAudio);}
 m_connecting=true; m_status="Регистрация…"; emit changed();
 nativeGeneration.store(sessionGeneration.load());domainName=domain;guiSipRegistered=false;
 registrationTimeout.start();
 startSipThread([this,host,domain,tlsPort,user,password,secureAudio]()mutable{return initializeNative(host,domain,tlsPort,user,password,secureAudio);});
}
bool Phone::initializeNative(QString host,QString domain,int tlsPort,QString user,QString password,bool secureAudio){
 const auto failed=[this](pj_status_t code,const QString &what){
  char text[PJ_ERR_MSG_SIZE];pj_strerror(code,text,sizeof(text));const QString message=what+": "+QString::fromUtf8(text);
  QMetaObject::invokeMethod(this,"nativeFailed",Qt::QueuedConnection,Q_ARG(QString,message),Q_ARG(qulonglong,nativeGeneration.load()));return false;
 };
 pj_status_t rc=pjsua_create(); if(rc!=PJ_SUCCESS){return failed(rc,"SIP");} initialized=true;
 pjsua_config cfg;pjsua_config_default(&cfg);cfg.thread_cnt=0;cfg.max_calls=1;
 cfg.cb.on_call_sdp_created=sdpCreated;cfg.cb.on_call_state=callState;cfg.cb.on_call_media_state=mediaState;cfg.cb.on_incoming_call=incomingCall;cfg.cb.on_reg_state2=registration;cfg.cb.on_transport_state=transportState;
 pjsua_logging_config log;pjsua_logging_config_default(&log);log.level=0;log.console_level=0;log.msg_logging=PJ_FALSE;
 pjsua_media_config media;pjsua_media_config_default(&media);media.clock_rate=16000;media.snd_clock_rate=16000;media.channel_count=1;media.audio_frame_ptime=20;
 rc=pjsua_init(&cfg,&log,&media);if(rc){return failed(rc,"Инициализация");}
 pjsua_transport_config tc;pjsua_transport_config_default(&tc);tc.port=0;tc.tls_setting.verify_server=PJ_TRUE;
 QByteArray ca="/etc/pki/tls/certs/ca-bundle.crt";tc.tls_setting.ca_list_file=str(ca);
 rc=pjsua_transport_create(PJSIP_TRANSPORT_TLS,&tc,&transport);if(rc){return failed(rc,"TLS");}
 rc=pjsua_start();if(rc){return failed(rc,"Запуск SIP");}
 pj_status_t audioStatus=pjsua_set_null_snd_dev();
 if(audioStatus!=PJ_SUCCESS){return failed(audioStatus,"Аудиоустройство");}
 pool=pjsua_pool_create("qt-audio",4096,4096);
 auto bridge=PJ_POOL_ZALLOC_T(pool,AudioBridgePort);bridge->ownerPool=pool;port=&bridge->base;
 pj_str_t name=pj_str(const_cast<char*>("qt-audio"));pjmedia_port_info_init(&port->info,&name,PJMEDIA_SIG_CLASS_PORT_AUD('Q','T'),16000,1,16,320);
 port->get_frame=getFrame;port->put_frame=putFrame;port->on_destroy=destroyAudioPort;
 rc=pjmedia_port_init_grp_lock(port,pool,nullptr);
 if(rc){pj_pool_release(pool);pool=nullptr;port=nullptr;return failed(rc,"Аудиопорт");}
 rc=pjsua_conf_add_port(pool,port,&slot);if(rc){return failed(rc,"Аудиомост");}
 pjsua_acc_config ac;pjsua_acc_config_default(&ac);
 QByteArray identity=("sip:"+user+"@"+domain).toUtf8();
 QByteArray registrar=("sip:"+host+":"+QString::number(tlsPort)+";transport=tls").toUtf8();
 QByteArray proxy=("sip:"+host+":"+QString::number(tlsPort)+";transport=tls;lr").toUtf8();
 QByteArray login=user.toUtf8(),secret=password.toUtf8(),realm="*",scheme="digest";
 ac.id=str(identity);ac.reg_uri=str(registrar);ac.transport_id=transport;ac.proxy_cnt=1;ac.proxy[0]=str(proxy);
 ac.cred_count=1;ac.cred_info[0].realm=str(realm);ac.cred_info[0].scheme=str(scheme);ac.cred_info[0].username=str(login);ac.cred_info[0].data_type=PJSIP_CRED_DATA_PLAIN_PASSWD;ac.cred_info[0].data=str(secret);
 ac.use_srtp=secureAudio?PJMEDIA_SRTP_MANDATORY:PJMEDIA_SRTP_OPTIONAL;ac.srtp_secure_signaling=1;
 ac.allow_contact_rewrite=PJ_TRUE;ac.reg_retry_interval=0;ac.reg_first_retry_interval=0;
 rc=pjsua_acc_add(&ac,PJ_TRUE,&account);secret.fill(0);password.fill(QChar(0));
 if(rc){return failed(rc,"Аккаунт");}QMetaObject::invokeMethod(this,"nativeReady",Qt::QueuedConnection,Q_ARG(qulonglong,nativeGeneration.load()));return true;
}
pj_status_t Phone::destroyAudioPort(pjmedia_port *mediaPort){
 auto bridge=reinterpret_cast<AudioBridgePort*>(mediaPort);
 pj_pool_release(bridge->ownerPool);return PJ_SUCCESS;
}
void Phone::startSipThread(std::function<bool()> initialize){
 if(sipThread)return;
 sipThread=new SipThread(std::move(initialize),[this](unsigned flags){
  if(!initialized.exchange(false))return;
  if(port){pjsua_set_no_snd_dev();pjmedia_port_destroy(port);}
  pjsua_destroy2(flags);
 },this);
 auto thread=sipThread;
 connect(thread,&QThread::finished,this,[this,thread]{if(sipThread==thread)finishLogout();});
 const auto generation=sessionGeneration.load();
 connect(thread,&SipThread::ipChangeFinished,this,[this,generation](int status){
  if(m_stopping||!initialized||generation!=sessionGeneration.load())return;
  if(status!=PJ_SUCCESS){registrationTimeout.stop();m_connecting=false;m_status="Не удалось восстановить SIP-соединение";emit changed();reconnect.schedule();}
  else if(active())startMedia(call);
 });
 thread->start();
}
void Phone::nativeReady(qulonglong generation){
 if(m_stopping||!initialized||generation!=sessionGeneration.load())return;
 guiSipRegistered=true;
 auto pending=std::move(pendingRegistration);pendingRegistration={};if(pending)pending();
}
void Phone::nativeFailed(const QString &message,qulonglong generation){
 if(m_stopping||generation!=sessionGeneration.load())return;
 registrationTimeout.stop();m_connecting=false;m_status=message;emit changed();
 if(sipThread)sipThread->stop();
}
void Phone::logout(){
 reconnect.cancel();sessionPassword=QCA::SecureArray();sessionAccount.clear();afterStop={};stopSession();
}
void Phone::stopSession(){
 registrationTimeout.stop();
 if(m_stopping)return;
 const bool sendUnregister=m_registered&&m_transportError.isEmpty();
 m_stopping=true;pendingLogin=false;++sessionGeneration;
 if(active()){
  dialPending=false;pendingCallEvents.clear();
  if(answered&&conversationClock.isValid())talkSeconds=conversationClock.elapsed()/1000;
  saveHistory();call=PJSUA_INVALID_ID;
 }
 m_registered=false;m_connecting=false;m_status="Отключение…";emit changed();
 stopAudio();releaseSystemCall();
 if(sipThread){sipThread->stop(sendUnregister);return;}
 finishLogout();
}
void Phone::finishLogout(){
 if(sipThread){auto finished=sipThread;sipThread=nullptr;finished->deleteLater();}
 initialized=false;guiSipRegistered=false;pendingRegistration={};
 delete manager;manager=nullptr;systemCall=nullptr;context=nullptr;
 port=nullptr;pool=nullptr;m_transportError.clear();
 account=call=transport=slot=PJSUA_INVALID_ID;m_incoming=m_muted=false;
 if(m_status=="Отключение…")m_status="Не подключён";
 m_stopping=false;emit changed();
 qInfo()<<"SIP session closed";
 auto next=std::move(afterStop);afterStop={};if(next)next();else if(reconnect.wanted())reconnect.schedule();
}
void Phone::transportState(pjsip_transport *transport,pjsip_transport_state state,const pjsip_transport_state_info *info){
 if(!self||self->m_stopping||!self->initialized||!info||info->status==PJ_SUCCESS)return;
 QString message;
 if(transport&&(transport->key.type==PJSIP_TRANSPORT_TLS||transport->key.type==PJSIP_TRANSPORT_TLS6)&&info->ext_info){
  auto tls=static_cast<pjsip_tls_state_info*>(info->ext_info);
  if(tls->ssl_sock_info){
   unsigned flags=tls->ssl_sock_info->verify_status;
   qInfo()<<"SIP TLS state"<<int(state)<<"status"<<info->status<<"verify flags"<<flags;
   if(flags&PJ_SSL_CERT_EVALIDITY_PERIOD)message="TLS: срок действия сертификата сервера недействителен. Требуется проверить сертификат и дату устройства.";
   else if(flags&PJ_SSL_CERT_EIDENTITY_NOT_MATCH)message="TLS: имя сервера не совпадает с сертификатом.";
   else if(flags)message="TLS: сертификат сервера не прошёл проверку доверия.";
  }
 }
 if(message.isEmpty()){char error[PJ_ERR_MSG_SIZE];pj_strerror(info->status,error,sizeof(error));message="Ошибка соединения: "+QString::fromUtf8(error);}
 QMetaObject::invokeMethod(self,"transportFailure",Qt::QueuedConnection,Q_ARG(QString,message),Q_ARG(qulonglong,self->nativeGeneration.load()));
}
void Phone::transportFailure(const QString &message,qulonglong generation){
 if(m_stopping||!initialized||generation!=sessionGeneration)return;
 // Shutdown can produce PJ_ECANCELLED after the certificate failure.
 // Preserve the actionable TLS diagnosis instead of replacing it.
 if(m_transportError.startsWith("TLS:")&&!message.startsWith("TLS:"))return;
 if(message.contains("PJ_ECANCELLED")&&(m_registered||!m_transportError.isEmpty()))return;
 m_transportError=message;
 m_registered=false;registrationTimeout.stop();m_connecting=false;m_status=message;emit changed();
 if(message.startsWith("TLS:"))reconnect.block();else reconnect.schedule();
 qInfo().noquote()<<"SIP transport:"<<message;
}
void Phone::registration(pjsua_acc_id id,pjsua_reg_info *details){
 if(!self||self->m_stopping||!self->initialized)return;
 pjsua_acc_info info;if(pjsua_acc_get_info(id,&info)!=PJ_SUCCESS)return;
 const int error=details&&details->cbparam?details->cbparam->status:PJ_SUCCESS;
 const QString reason=QString::fromUtf8(info.status_text.ptr,info.status_text.slen);
 QMetaObject::invokeMethod(self,"applyRegistration",Qt::QueuedConnection,Q_ARG(int,id),Q_ARG(int,int(info.status)),Q_ARG(int,int(info.expires)),Q_ARG(int,error),Q_ARG(QString,reason),Q_ARG(qulonglong,self->nativeGeneration.load()));
}
void Phone::applyRegistration(int id,int status,int expires,int error,const QString &reason,qulonglong generation){
 if(m_stopping||!initialized||generation!=sessionGeneration.load())return;
 if(!guiSipRegistered){pendingRegistration=[this,id,status,expires,error,reason,generation]{applyRegistration(id,status,expires,error,reason,generation);};return;}
 if(id!=account)return;
 this->m_registered=status==200&&expires>0;
 if(status>=200){registrationTimeout.stop();this->m_connecting=false;}
 if(this->m_registered){
  reconnect.connected();this->m_transportError.clear();this->m_status="Подключён";
  if(!this->manager){
   this->manager=new ac::CallManager("OMPPhone",this);
   this->context=new ac::AudioContext(this->manager);
   this->context->setDefaultContext(ac::AudioContext::Type::Denied);
   auto phone=this;
   QPointer<ac::AudioContext> audioContext=this->context;
   connect(this->context,&ac::AudioContext::contextChanged,this,[phone,audioContext](ac::AudioContext::Type type){
    qInfo()<<"Call audio context"<<int(type);
    if(phone->m_stopping||!audioContext||phone->context!=audioContext.data())return;
    if(type!=ac::AudioContext::Type::VoIP)phone->stopAudio();
    else if(phone->active())phone->startMedia(phone->call);
   },Qt::QueuedConnection);
  }
 } else if(!m_transportError.isEmpty())m_status=m_transportError;
 else if(error!=PJ_SUCCESS){char text[PJ_ERR_MSG_SIZE];pj_strerror(error,text,sizeof(text));m_status="Ошибка регистрации: "+QString::fromUtf8(text);}
 else m_status=QString("Регистрация: %1 (%2)").arg(status).arg(reason);
 if(!m_registered&&status>=200){
  if(status==401||status==403||status==407)reconnect.block();else reconnect.schedule();
 }
 qInfo().noquote()<<"SIP registration:"<<status<<"local error:"<<error<<m_status;
 emit changed();
}
void Phone::dial(QString number){
 if(!m_registered||active()||!guiSipRegistered||m_stopping)return;
 if(!QRegularExpression("\\A[+0-9*#]{1,64}\\z").match(number).hasMatch()){m_status="Недопустимый номер";emit changed();return;}
 m_peer=number;m_incoming=false;answered=false;talkSeconds=0;started=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
 dialPending=true;cancelPendingDial=false;pendingCallEvents.clear();m_status="Вызов…";emit changed();
 const auto generation=sessionGeneration.load();const int accountId=account;const QString domain=domainName;
 sipThread->post([this,number,domain,accountId,generation]{
  QByteArray uri=("sip:"+QString(number).replace("#","%23")+"@"+domain+";transport=tls").toUtf8();pj_str_t dest=str(uri);
  pjsua_call_setting options;pjsua_call_setting_default(&options);options.aud_cnt=1;options.vid_cnt=0;options.txt_cnt=0;
  pjsua_call_id id=PJSUA_INVALID_ID;const auto status=pjsua_call_make_call(accountId,&dest,&options,nullptr,nullptr,&id);
  QMetaObject::invokeMethod(this,"applyDialResult",Qt::QueuedConnection,Q_ARG(int,id),Q_ARG(int,status),Q_ARG(qulonglong,generation));
 });
}
void Phone::applyDialResult(int id,int status,qulonglong generation){
 if(m_stopping||generation!=sessionGeneration.load())return;
 dialPending=false;
 if(status!=PJ_SUCCESS){pendingCallEvents.clear();call=PJSUA_INVALID_ID;m_status="Не удалось начать вызов";saveHistory();emit changed();return;}
 call=id;publishCall();
 auto events=std::move(pendingCallEvents);pendingCallEvents.clear();for(auto &event:events)event();
 if(cancelPendingDial&&call!=PJSUA_INVALID_ID)hangup();
 if(call!=PJSUA_INVALID_ID)startMedia(call);
 emit changed();
}
void Phone::incomingCall(pjsua_acc_id,pjsua_call_id id,pjsip_rx_data*){
 qInfo()<<"Incoming SIP call received";
 if(!self||self->m_stopping||!self->initialized)return;
 pjsua_call_info info;if(pjsua_call_get_info(id,&info)!=PJ_SUCCESS)return;
 const QString peer=QString::fromUtf8(info.remote_info.ptr,info.remote_info.slen);
 pjsua_call_answer(id,180,nullptr,nullptr);
 QMetaObject::invokeMethod(self,"applyIncomingCall",Qt::QueuedConnection,Q_ARG(int,id),Q_ARG(QString,peer),Q_ARG(qulonglong,self->nativeGeneration.load()));
}
void Phone::applyIncomingCall(int id,const QString &peer,qulonglong generation){
 if(m_stopping||!initialized||generation!=sessionGeneration.load())return;
 if(active())return;
 call=id;m_incoming=true;answered=false;talkSeconds=0;started=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
 m_peer=peer;m_status="Входящий звонок";publishCall();qInfo()<<"Incoming call published"<<bool(systemCall);emit changed();
}
void Phone::answer(){if(active()&&m_incoming&&sipThread){const int id=call;sipThread->post([id]{pjsua_call_answer(id,200,nullptr,nullptr);});}}
void Phone::hangup(){
 if(dialPending){cancelPendingDial=true;return;}
 if(!active()||!initialized||m_stopping||!sipThread)return;
 const int id=call;sipThread->post([id]{
  pjsua_call_info info;
  if(pjsua_call_get_info(id,&info)==PJ_SUCCESS&&info.state!=PJSIP_INV_STATE_DISCONNECTED)pjsua_call_hangup(id,0,nullptr,nullptr);
 });
}
void Phone::dtmf(QString digit){
 if(dialPending)return;
 if(!active()||!sipThread||!QRegularExpression("\\A[0-9*#]\\z").match(digit).hasMatch())return;
 const int id=call;sipThread->post([id,digit]{QByteArray b=digit.toLatin1();pj_str_t s=str(b);pjsua_call_dial_dtmf(id,&s);});
}
void Phone::toggleMute(){
 m_muted=!m_muted;
 if(slot!=PJSUA_INVALID_ID&&sipThread){const auto port=slot;const bool mute=m_muted;sipThread->post([port,mute]{pjsua_conf_adjust_rx_level(port,mute?0:1);});}
 emit changed();
}
void Phone::callState(pjsua_call_id id,pjsip_event *event){
 if(!self||self->m_stopping||!self->initialized)return;
 pjsua_call_info info;if(pjsua_call_get_info(id,&info))return;
 if(event&&event->type==PJSIP_EVENT_TSX_STATE&&event->body.tsx_state.type==PJSIP_EVENT_RX_MSG){
  const auto data=event->body.tsx_state.src.rdata;
  if(data&&data->msg_info.msg){
   pj_str_t name=pj_str(const_cast<char*>("Reason"));
   auto header=static_cast<pjsip_hdr*>(pjsip_msg_find_hdr_by_name(data->msg_info.msg,&name,nullptr));
   if(header){
    char buffer[512];int length=pjsip_hdr_print_on(header,buffer,sizeof(buffer));
    if(length>0){
     const auto cause=QRegularExpression("cause=([0-9]+)").match(QString::fromUtf8(buffer,length));
     if(cause.hasMatch())qInfo()<<"SIP response cause"<<cause.captured(1).toInt();
    }
   }
  }
 }
 // Snapshot native data; Qt objects and audio must be touched on the GUI thread.
 QMetaObject::invokeMethod(self,"applyCallState",Qt::QueuedConnection,
  Q_ARG(int,id),Q_ARG(int,int(info.state)),Q_ARG(int,int(info.last_status)),
  Q_ARG(qulonglong,self->nativeGeneration.load()),Q_ARG(qlonglong,qlonglong(info.connect_duration.sec)));
}
void Phone::applyCallState(int id,int state,int status,qulonglong generation,qlonglong durationSeconds){
 if(m_stopping||!initialized||generation!=sessionGeneration)return;
 if(dialPending){pendingCallEvents.push_back([this,id,state,status,generation,durationSeconds]{applyCallState(id,state,status,generation,durationSeconds);});return;}
 if(id!=call)return;
 talkSeconds=qMax<qlonglong>(0,durationSeconds);
 qInfo()<<"SIP call state"<<state<<"status"<<status;
 if(state==PJSIP_INV_STATE_CONFIRMED){
  if(systemCall)systemCall->setStatus(ac::Call::Active);
  if(!answered)conversationClock.start();
  answered=true;m_incoming=false;m_status="Разговор";
 }
 if(state==PJSIP_INV_STATE_DISCONNECTED){
  // Invalidate before notifying CallService: setStatus can emit disconnected
  // synchronously and can cause the manager to delete its Call object.
  call=PJSUA_INVALID_ID;
  saveHistory();stopAudio();releaseSystemCall();
  m_incoming=false;m_muted=false;
  if(slot!=PJSUA_INVALID_ID&&sipThread){const auto port=slot;sipThread->post([port]{pjsua_conf_adjust_rx_level(port,1);});}
  m_status=QString("Вызов завершён (%1)").arg(status);
 }
 emit changed();
}
void Phone::releaseSystemCall(){
 QPointer<ac::Call> finished=systemCall;
 systemCall.clear();
 if(!finished)return;
 const QString id=finished->callId();
 QPointer<ac::CallManager> owner=manager;
 QObject::disconnect(finished.data(),nullptr,this,nullptr);
 finished->setStatus(ac::Call::Disconnected);
 if(owner&&finished&&owner->getCall(id)==finished.data())owner->removeCall(id);
}
void Phone::saveHistory(){QVariantMap row;row["peer"]=m_peer;row["time"]=started;row["result"]=answered?"Разговор":"Без ответа";row["durationSeconds"]=answered?talkSeconds:qlonglong(0);m_history.prepend(row);while(m_history.size()>100)m_history.removeLast();AppSettings settings;settings.setValue("history",m_history);settings.sync();m_storageError=settings.status()==QSettings::NoError?QString():QStringLiteral("Не удалось сохранить историю вызовов");}
void Phone::stopAudio(){
 playbackTimer.stop();
 // Detach first: stopping PulseAudio can synchronously dispatch more signals.
 auto oldInput=input;auto oldOutput=output;
 auto oldMic=mic;auto oldSpeaker=speaker;
 input=nullptr;output=nullptr;mic=nullptr;speaker=nullptr;
 mediaSlot=PJSUA_INVALID_ID;
 if(oldMic)QObject::disconnect(oldMic,nullptr,this,nullptr);
 if(oldSpeaker)QObject::disconnect(oldSpeaker,nullptr,this,nullptr);
 if(oldInput){QObject::disconnect(oldInput,nullptr,this,nullptr);oldInput->stop();oldInput->deleteLater();}
 if(oldOutput){QObject::disconnect(oldOutput,nullptr,this,nullptr);oldOutput->stop();oldOutput->deleteLater();}
 QMutexLocker lock(&mutex);captured.clear();playback.clear();
}

void Phone::mediaState(pjsua_call_id id){if(self&&!self->m_stopping)QMetaObject::invokeMethod(self,"applyMediaState",Qt::QueuedConnection,Q_ARG(int,id),Q_ARG(qulonglong,self->nativeGeneration.load()));}
void Phone::applyMediaState(int id,qulonglong generation){if(generation==sessionGeneration.load())startMedia(id);}
static int pcmPeak(const char *data,int size){
 int peak=0;for(int n=0;n+1<size;n+=2){int value=qFromLittleEndian<qint16>(reinterpret_cast<const uchar*>(data+n));peak=qMax(peak,qAbs(value));}return peak;
}
void Phone::sdpCreated(pjsua_call_id,pjmedia_sdp_session *sdp,pj_pool_t *,const pjmedia_sdp_session *){
 // Log only negotiation metadata. Never print SDP, crypto keys or SIP credentials.
 for(unsigned i=0;sdp&&i<sdp->media_count;++i){
  const auto media=sdp->media[i];QStringList codecs;bool crypto=false;
  for(unsigned a=0;a<media->attr_count;++a){
   auto attr=media->attr[a];
   if(!pj_stricmp2(&attr->name,"rtpmap"))codecs.append(QString::fromUtf8(attr->value.ptr,attr->value.slen));
   if(!pj_stricmp2(&attr->name,"crypto"))crypto=true;
  }
  qInfo().noquote()<<"SDP offer media"<<QString::fromUtf8(media->desc.media.ptr,media->desc.media.slen)<<"protocol"<<QString::fromUtf8(media->desc.transport.ptr,media->desc.transport.slen)<<"codecs"<<codecs.join(",")<<"crypto offered"<<crypto;
 }
}
void Phone::startMedia(int id){
 if(m_stopping||!initialized||id==PJSUA_INVALID_ID||id!=call||!sipThread)return;
 const auto generation=sessionGeneration.load();const auto bridgeSlot=slot;
 sipThread->post([this,id,generation,bridgeSlot]{
  pjsua_call_info ci;if(pjsua_call_get_info(id,&ci)||ci.media_status!=PJSUA_CALL_MEDIA_ACTIVE)return;
  QVariantMap snapshot;snapshot["slot"]=ci.conf_slot;
  snapshot["receive"]=pjsua_conf_connect(ci.conf_slot,bridgeSlot);snapshot["transmit"]=pjsua_conf_connect(bridgeSlot,ci.conf_slot);
  pjsua_stream_info stream;
  if(pjsua_call_get_stream_info(id,0,&stream)==PJ_SUCCESS&&stream.type==PJMEDIA_TYPE_AUDIO)
   snapshot["codec"]=QString::fromUtf8(stream.info.aud.fmt.encoding_name.ptr,stream.info.aud.fmt.encoding_name.slen)+"/"+QString::number(stream.info.aud.fmt.clock_rate);
  pjmedia_transport_info transportInfo;pjmedia_transport_info_init(&transportInfo);
  if(pjsua_call_get_med_transport_info(id,0,&transportInfo)==PJ_SUCCESS){
   auto srtp=static_cast<pjmedia_srtp_info*>(pjmedia_transport_info_get_spc_info(&transportInfo,PJMEDIA_TRANSPORT_TYPE_SRTP));snapshot["srtp"]=srtp&&srtp->active;
  }
  pjsua_stream_stat stats;if(pjsua_call_get_stream_stat(id,0,&stats)==PJ_SUCCESS){snapshot["rtpTx"]=stats.rtcp.tx.pkt;snapshot["rtpRx"]=stats.rtcp.rx.pkt;}
  QMetaObject::invokeMethod(this,"applyMediaSnapshot",Qt::QueuedConnection,Q_ARG(int,id),Q_ARG(QVariantMap,snapshot),Q_ARG(qulonglong,generation));
 });
}
void Phone::applyMediaSnapshot(int id,const QVariantMap &snapshot,qulonglong generation){
 if(m_stopping||generation!=sessionGeneration.load()||id!=call||id==PJSUA_INVALID_ID)return;
 mediaCodec=snapshot.value("codec").toString();mediaSrtp=snapshot.value("srtp").toBool();
 rtpTx=snapshot.value("rtpTx").toUInt();rtpRx=snapshot.value("rtpRx").toUInt();
 if(!context||context->context()!=ac::AudioContext::Type::VoIP){m_status="Ожидание доступа к звуку…";emit changed();return;}
 if(input&&output){mediaSlot=snapshot.value("slot").toInt();updatePlaybackGain();return;}
 stopAudio();
 {QMutexLocker lock(&mutex);micBytes=speakerBytes=txFrames=rxFrames=0;micPeak=rxPeak=0;}
 QAudioFormat format;format.setSampleRate(16000);format.setChannelCount(1);format.setSampleSize(16);format.setCodec("audio/pcm");format.setByteOrder(QAudioFormat::LittleEndian);format.setSampleType(QAudioFormat::SignedInt);
 if(!QAudioDeviceInfo::defaultInputDevice().isFormatSupported(format)||!QAudioDeviceInfo::defaultOutputDevice().isFormatSupported(format)){
  m_status="Устройство не поддерживает PCM 16 кГц";emit changed();return;
 }
 input=new QAudioInput(QAudioDeviceInfo::defaultInputDevice(),format,this);output=new QAudioOutput(QAudioDeviceInfo::defaultOutputDevice(),format,this);
 output->setCategory("phone");input->setBufferSize(3200);output->setBufferSize(6400);
 mic=input->start();speaker=output->start();
 if(!mic||!speaker){stopAudio();m_status="Не удалось открыть микрофон или динамик";emit changed();return;}
 const auto source=mic;
 connect(source,&QIODevice::readyRead,this,[this,source]{
  if(mic!=source)return;
  const auto bytes=source->readAll();
  QMutexLocker lock(&mutex);micBytes+=bytes.size();micPeak=qMax(micPeak,pcmPeak(bytes.constData(),bytes.size()));
  captured+=bytes;if(captured.size()>6400)captured.remove(0,captured.size()-6400);
 });
 // notify() follows processed audio; an initially idle sink may never notify.
 // An independent clock primes it when the first decoded RTP frame arrives.
 playbackTimer.start();
 mediaSlot=snapshot.value("slot").toInt();updatePlaybackGain();
 if(answered)m_status="Разговор";
 qInfo()<<"Audio media started codec"<<mediaCodec<<"SRTP"<<mediaSrtp<<"bridge status"<<snapshot.value("receive")<<snapshot.value("transmit");
 emit changed();
}
void Phone::updatePlaybackGain(){
 if(!initialized||m_stopping||slot==PJSUA_INVALID_ID)return;
 float gain=1.0f;
 for(const auto &device:devices)
  if(device.id==outputRoute&&device.type==ac::AudioDevice::Earpiece)gain=2.0f;
 if(sipThread){const auto port=slot;const auto generation=sessionGeneration.load();sipThread->post([this,port,gain,generation]{
  if(pjsua_conf_adjust_tx_level(port,gain)==PJ_SUCCESS)QMetaObject::invokeMethod(this,"applyPlaybackGain",Qt::QueuedConnection,Q_ARG(float,gain),Q_ARG(qulonglong,generation));
 });}
}
void Phone::applyPlaybackGain(float gain,qulonglong generation){if(!m_stopping&&generation==sessionGeneration.load())playbackGain=gain;}
void Phone::pumpPlayback(){
 if(!output||!speaker)return;
 QMutexLocker lock(&mutex);
 const qint64 available=qMin<qint64>(playback.size(),output->bytesFree());
 if(available<=0)return;
 const auto written=speaker->write(playback.constData(),available);
 if(written>0){speakerBytes+=written;playback.remove(0,written);}
}
QVariantMap Phone::mediaDiagnostics(){
 if(initialized&&call!=PJSUA_INVALID_ID)startMedia(call);
 QVariantMap result;result["active"]=active();result["codec"]=mediaCodec;result["srtp"]=mediaSrtp;
 result["rtpTx"]=rtpTx;result["rtpRx"]=rtpRx;
 result["audioContext"]=context?int(context->context()):-1;
 result["playbackGain"]=double(playbackGain);
 result["systemCallActive"]=systemCall&&systemCall->active();
 int inputType=0,outputType=0;
 for(const auto &device:devices){if(device.id==inputRoute)inputType=int(device.type);if(device.id==outputRoute)outputType=int(device.type);}
 result["routeInputType"]=inputType;result["routeOutputType"]=outputType;
 result["inputVolume"]=input?input->volume():0.0;result["outputVolume"]=output?output->volume():0.0;
 result["inputState"]=input?int(input->state()):-1;result["outputState"]=output?int(output->state()):-1;
 result["inputError"]=input?int(input->error()):-1;result["outputError"]=output?int(output->error()):-1;
 QMutexLocker lock(&mutex);
 result["micBytes"]=QVariant::fromValue(micBytes);result["speakerBytes"]=QVariant::fromValue(speakerBytes);
 result["txFrames"]=QVariant::fromValue(txFrames);result["rxFrames"]=QVariant::fromValue(rxFrames);
 result["micPeak"]=micPeak;result["rxPeak"]=rxPeak;return result;
}
pj_status_t Phone::getFrame(pjmedia_port*,pjmedia_frame *frame){
 if(!self||!frame||!frame->buf)return PJ_EINVAL;
 QMutexLocker lock(&self->mutex);memset(frame->buf,0,frame->size);
 int n=qMin<int>(frame->size,self->captured.size());memcpy(frame->buf,self->captured.constData(),n);self->captured.remove(0,n);
 ++self->txFrames;frame->type=PJMEDIA_FRAME_TYPE_AUDIO;return PJ_SUCCESS;
}
pj_status_t Phone::putFrame(pjmedia_port*,pjmedia_frame *frame){
 if(!self||!frame)return PJ_EINVAL;
 if(frame->type!=PJMEDIA_FRAME_TYPE_AUDIO||!frame->buf)return PJ_SUCCESS;
 QMutexLocker lock(&self->mutex);++self->rxFrames;
 self->rxPeak=qMax(self->rxPeak,pcmPeak(static_cast<const char*>(frame->buf),frame->size));
 self->playback.append(static_cast<char*>(frame->buf),frame->size);
 if(self->playback.size()>6400)self->playback.remove(0,self->playback.size()-6400);
 return PJ_SUCCESS;
}

void Phone::publishCall(){
 if(!manager||!manager->valid())return;
 QVariantMap params;params["incoming"]=m_incoming;params["remoteName"]=m_peer;params["remoteHandle"]=m_peer;
 systemCall=manager->newCall("Call"+QString::number(QDateTime::currentMSecsSinceEpoch()),params);
 if(!systemCall)return;
 systemCall->addInterfaces({this});
 connect(systemCall,&ac::Call::accepted,this,&Phone::answer);
 connect(systemCall,&ac::Call::rejected,this,&Phone::hangup);
 connect(systemCall,&ac::Call::disconnected,this,&Phone::hangup);
 systemCall->setStatus(m_incoming?ac::Call::Ringing:ac::Call::Dialing);
}
void Phone::UpdateAudioRouteState(const QList<ac::AudioDevice> &list,bool mute,const QString &in,const QString &out){devices=list;UpdateAudioRoute(mute,in,out);}
void Phone::UpdateAudioRoute(bool mute,const QString &in,const QString &out){inputRoute=in;outputRoute=out;m_muted=mute;updatePlaybackGain();if(slot!=PJSUA_INVALID_ID&&sipThread){const auto port=slot;sipThread->post([port,mute]{pjsua_conf_adjust_rx_level(port,mute?0:1);});}emit changed();}
void Phone::toggleSpeaker(){
 ac::AudioDevice::Type target=ac::AudioDevice::Speaker;
 for(const auto &d:devices)if(d.id==outputRoute&&d.type==ac::AudioDevice::Speaker)target=ac::AudioDevice::Earpiece;
 for(const auto &d:devices)if(d.type==target&&d.hasOutput){emit RequestAudioRoute(inputRoute,d.id);return;}
 m_status="Аудиомаршрут недоступен";emit changed();
}

void Phone::acceptPassword(const QString &secret,const QVariantMap &loadedProfile){
 const auto profile=loadedProfile.isEmpty()?loadedProfile:AccountSettings::withLocalAudioPreference(loadedProfile);
 savedPassword=QCA::SecureArray(secret.toUtf8());savedAccount=profile;
 if(!profile.isEmpty()){
  const auto error=AccountSettings::save(profile);m_storageError=error;
 }
 if(!profile.isEmpty())emit accountLoaded(profile);
 emit passwordLoaded(secret);
 if(pendingLogin){
  pendingLogin=false;const auto account=profile.isEmpty()?AccountSettings::current():profile;
  const auto error=AccountSettings::validate(account);
  if(!error.isEmpty()){m_status=error;emit changed();}
  else login(account.value("host").toString(),account.value("domain").toString(),account.value("port").toInt(),account.value("user").toString(),secret,account.value("srtp").toBool());
 }
}
bool Phone::saveAccount(QString host,QString domain,int port,QString user,bool secureAudio){
 const auto profile=AccountSettings::profile(host,domain,port,user,secureAudio);const auto error=AccountSettings::validate(profile);
 if(!error.isEmpty()){stagedAccount.clear();m_credentialStatus=error;emit changed();return false;}
 stagedAccount=profile;return true;
}
void Phone::savePassword(QString password){
 if(credentials.isRunning())return;
 savedPassword=QCA::SecureArray();savedAccount.clear();writingAccount=stagedAccount.isEmpty()?AccountSettings::current():stagedAccount;stagedAccount.clear();
 m_credentialStatus="Сохранение аккаунта…";credentials.save(password,writingAccount);emit changed();
}
void Phone::loadPassword(){
 if(savedPassword.size()>0){QTimer::singleShot(0,this,[this]{acceptPassword(QString::fromUtf8(savedPassword.toByteArray()),savedAccount);m_credentialStatus="Пароль загружен";emit changed();});return;}
 if(credentials.isRunning())return;
 m_credentialStatus="Загрузка пароля…";credentials.load();emit changed();}
bool Phone::hasSavedPassword() const {return QFile::exists(AppSettings::dataDirectory()+"/omp-credentials.v1.json");}
void Phone::loginSaved(){if(credentials.isRunning()||active())return;pendingLogin=true;loadPassword();}

void Phone::attemptReconnect(){
 if(!reconnect.wanted()||reconnect.blocked()||m_stopping||sipThread||initialized)return;
 if(sessionPassword.isEmpty()){reconnect.block();m_status="Для подключения нужен пароль аккаунта";emit changed();return;}
 automaticRetry=true;
 login(sessionAccount.value("host").toString(),sessionAccount.value("domain").toString(),sessionAccount.value("port").toInt(),sessionAccount.value("user").toString(),QString::fromUtf8(sessionPassword.toByteArray()),sessionAccount.value("srtp").toBool());
 automaticRetry=false;
}
QVariantMap Phone::reconnectState() const {
 QVariantMap state;state["wanted"]=reconnect.wanted();state["blocked"]=reconnect.blocked();state["attempts"]=reconnect.attempts();state["retryInMs"]=reconnect.remaining();return state;
}
