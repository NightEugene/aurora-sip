#pragma once
#include <QObject>
#include "credentialstore.h"
#include <QtCrypto>
#include "sipthread.h"
#include "reconnectpolicy.h"
#include <atomic>
#include <functional>
#include <QElapsedTimer>
#include <QVariantList>
#include <QTimer>
#include <QAudioInput>
#include <QAudioOutput>
#include <QMutex>
#include <QPointer>
#include <pjsua-lib/pjsua.h>
#include <callmanager.h>
#include <call.h>
#include <audiocontext.h>
#include <audiocontrolinterface.h>
namespace ac = ru::auroraos::call;
class Phone : public QObject, public ac::AudioControlInterface {
 Q_OBJECT
 Q_INTERFACES(ru::auroraos::call::AudioControlInterface)
 Q_PROPERTY(QString accountName READ accountName NOTIFY changed)
 Q_PROPERTY(QString storageError READ storageError NOTIFY changed)
 Q_PROPERTY(QString status READ status NOTIFY changed)
 Q_PROPERTY(QString credentialStatus READ credentialStatus NOTIFY changed)
 Q_PROPERTY(bool credentialsBusy READ credentialsBusy NOTIFY changed)
 Q_PROPERTY(QString peer READ peer NOTIFY changed)
 Q_PROPERTY(bool connecting READ connecting NOTIFY changed)
 Q_PROPERTY(bool disconnecting READ disconnecting NOTIFY changed)
 Q_PROPERTY(bool connectionEnabled READ connectionEnabled NOTIFY changed)
 Q_PROPERTY(bool registered READ registered NOTIFY changed)
 Q_PROPERTY(bool active READ active NOTIFY changed)
 Q_PROPERTY(bool incoming READ incoming NOTIFY changed)
 Q_PROPERTY(bool muted READ muted NOTIFY changed)
 Q_PROPERTY(QVariantList history READ history NOTIFY changed)
public:
 explicit Phone(QObject *parent=nullptr); ~Phone();
 QString accountName() const;
 Q_INVOKABLE void setAccountName(const QString &name);
 QString credentialStatus() const {return m_credentialStatus;}
 bool credentialsBusy() const {return credentials.isRunning();}
 QString storageError() const {return m_storageError;}
 QString status() const {return m_status;} QString peer() const {return m_peer;}
 bool connectionEnabled() const {return m_registered;}
 bool connecting() const {return m_connecting;}
 bool disconnecting() const {return m_stopping;}
 bool registered() const {return m_registered;} bool active() const {return call!=PJSUA_INVALID_ID||dialPending;}
 bool incoming() const {return m_incoming;} bool muted() const {return m_muted;}
 QVariantList history() const {return m_history;}
 Q_INVOKABLE void setRequireSecureAudio(bool required);
 Q_INVOKABLE QVariant setting(const QString &key, const QVariant &fallback) const;
 Q_INVOKABLE void login(QString host, QString domain, int port, QString user, QString password, bool secureAudio);
 Q_INVOKABLE void dial(QString number);
 Q_INVOKABLE void answer(); Q_INVOKABLE void hangup(); Q_INVOKABLE void toggleMute();
 Q_INVOKABLE void toggleSpeaker();
 Q_INVOKABLE bool saveAccount(QString host, QString domain, int port, QString user, bool secureAudio);
 Q_INVOKABLE void savePassword(QString password);
 Q_INVOKABLE void loadPassword();
 Q_INVOKABLE bool hasSavedPassword() const;
 Q_INVOKABLE void loginSaved();
 QVariantMap mediaDiagnostics();
 QVariantMap reconnectState() const;
 Q_INVOKABLE void dtmf(QString digit); Q_INVOKABLE void logout();
signals:
 void changed();
 void accountLoaded(const QVariantMap &profile);
 void passwordLoaded(const QString &secret);
 void RequestMute(bool) override;
 void RequestAudioRoute(const QString &, const QString &) override;
public slots:
 void UpdateAudioRouteState(const QList<ac::AudioDevice> &, bool, const QString &, const QString &) override;
 void UpdateAudioRoute(bool, const QString &, const QString &) override;
private slots:
 void applyCallState(int id, int state, int status, qulonglong generation, qlonglong durationSeconds);
 void startMedia(int id);
 void applyDialResult(int id,int status,qulonglong generation);
 void applyMediaSnapshot(int id,const QVariantMap &snapshot,qulonglong generation);
 void pumpPlayback();
 void transportFailure(const QString &message, qulonglong generation);
 void applyRegistration(int id,int status,int expires,int error,const QString &reason,qulonglong generation);
 void applyIncomingCall(int id,const QString &peer,qulonglong generation);
 void applyMediaState(int id,qulonglong generation);
 void finishLogout();
 void applyPlaybackGain(float gain,qulonglong generation);
 void nativeReady(qulonglong generation);
 void nativeFailed(const QString &message,qulonglong generation);
private:
 void stopSession();
 void attemptReconnect();
 ReconnectPolicy reconnect;
 bool automaticRetry=false;
 bool dialPending=false,cancelPendingDial=false;
 std::vector<std::function<void()>> pendingCallEvents;
 QCA::SecureArray sessionPassword;
 void startSipThread(std::function<bool()> initialize);
 bool initializeNative(QString host,QString domain,int tlsPort,QString user,QString password,bool secureAudio);
 bool guiSipRegistered=false;
 SipThread *sipThread=nullptr;
 std::function<void()> afterStop,pendingRegistration;
 QElapsedTimer conversationClock;
 ac::CallManager *manager=nullptr; QPointer<ac::Call> systemCall; ac::AudioContext *context=nullptr;
 QList<ac::AudioDevice> devices; QString inputRoute, outputRoute;
 CredentialStore credentials;
 void publishCall();
 void releaseSystemCall();
 void updatePlaybackGain();
 void acceptPassword(const QString &secret,const QVariantMap &profile);
 QCA::SecureArray savedPassword;
 QVariantMap stagedAccount,writingAccount,savedAccount,sessionAccount;
 static Phone *self;
 static void callState(pjsua_call_id, pjsip_event*);
 static void mediaState(pjsua_call_id);
 static void sdpCreated(pjsua_call_id, pjmedia_sdp_session *, pj_pool_t *, const pjmedia_sdp_session *);
 static void incomingCall(pjsua_acc_id,pjsua_call_id,pjsip_rx_data*);
 static void registration(pjsua_acc_id, pjsua_reg_info*);
 static void transportState(pjsip_transport*,pjsip_transport_state,const pjsip_transport_state_info*);
 static pj_status_t destroyAudioPort(pjmedia_port*);
 static pj_status_t getFrame(pjmedia_port*,pjmedia_frame*);
 static pj_status_t putFrame(pjmedia_port*,pjmedia_frame*);
 void fail(pj_status_t, const QString&); void stopAudio(); void saveHistory();
 QString m_storageError;
 QString m_credentialStatus,m_transportError; bool pendingLogin=false;
 QString m_status="Не подключён",m_peer,domainName; bool m_registered=false,m_incoming=false,m_muted=false;
 bool m_connecting=false; std::atomic_bool m_stopping{false}; std::atomic<qulonglong> sessionGeneration{0},nativeGeneration{0};
 std::atomic_bool initialized{false}; pjsua_acc_id account=PJSUA_INVALID_ID; pjsua_call_id call=PJSUA_INVALID_ID;
 pjsua_transport_id transport=PJSUA_INVALID_ID;
 pj_pool_t *pool=nullptr; pjmedia_port *port=nullptr; pjsua_conf_port_id slot=PJSUA_INVALID_ID;
 QAudioInput *input=nullptr; QAudioOutput *output=nullptr; QIODevice *mic=nullptr,*speaker=nullptr;
 QMutex mutex; QByteArray captured, playback; QVariantList m_history; QString started; bool answered=false; qlonglong talkSeconds=0;
 QTimer playbackTimer,registrationTimeout;
 quint64 micBytes=0, speakerBytes=0, txFrames=0, rxFrames=0;
 int micPeak=0, rxPeak=0;
 unsigned rtpTx=0,rtpRx=0;
 QString mediaCodec;
 bool mediaSrtp=false;
 pjsua_conf_port_id mediaSlot=PJSUA_INVALID_ID;
 float playbackGain=1.0f;
};
