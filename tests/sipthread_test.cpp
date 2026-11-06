#include "sipthread.h"
#include "reconnectpolicy.h"
#include <QtTest>
#include <QTimer>
#include <atomic>

// Native boundary substitutes deliberately stall. The production SipThread
// must keep these operations off the GUI thread and serialize teardown.
static std::atomic<int> ipCalls{0},destroyFlags{-1};
static QThread *nativeThread=nullptr;
extern "C" {
pj_status_t pj_thread_register(const char *,pj_thread_desc,pj_thread_t **thread){*thread=nullptr;nativeThread=QThread::currentThread();return PJ_SUCCESS;}
int pjsua_handle_events(unsigned){QThread::msleep(20);return 0;}
void pjsua_ip_change_param_default(pjsua_ip_change_param *p){memset(p,0,sizeof(*p));}
pj_status_t pjsua_handle_ip_change(const pjsua_ip_change_param *){++ipCalls;QThread::msleep(240);return PJ_SUCCESS;}
pjmedia_port *pjsua_set_no_snd_dev(){return nullptr;}
pj_status_t pjmedia_port_destroy(pjmedia_port *){return PJ_SUCCESS;}
pj_status_t pjsua_destroy2(unsigned flags){destroyFlags=int(flags);QThread::msleep(200);return PJ_SUCCESS;}
}
class SipThreadTest : public QObject {
    Q_OBJECT
private slots:
    void slowNetworkAndShutdownDoNotBlockGui(){
        int heartbeats=0;QTimer heartbeat;heartbeat.setInterval(10);connect(&heartbeat,&QTimer::timeout,this,[&]{++heartbeats;});heartbeat.start();
        SipThread worker(nullptr);QSignalSpy changed(&worker,&SipThread::ipChangeFinished),stopped(&worker,&QThread::finished);
        worker.requestIpChange();worker.start();
        QTRY_COMPARE(changed.count(),1);QVERIFY(heartbeats>8);QVERIFY(nativeThread!=QThread::currentThread());
        int before=heartbeats;worker.stop();QTRY_COMPARE(stopped.count(),1);worker.wait();
        QVERIFY(heartbeats-before>6);QCOMPARE(destroyFlags.load(),int(PJSUA_DESTROY_NO_NETWORK));QCOMPARE(ipCalls.load(),1);
    }
    void commandsRunOnWorker(){
        std::atomic_bool ran{false};QThread *commandThread=nullptr;
        SipThread worker(nullptr);QSignalSpy stopped(&worker,&QThread::finished);
        worker.post([&]{commandThread=QThread::currentThread();ran=true;});worker.start();QTRY_VERIFY(ran.load());QVERIFY(commandThread!=QThread::currentThread());
        worker.stop(true);QTRY_COMPARE(stopped.count(),1);worker.wait();QCOMPARE(destroyFlags.load(),int(PJSUA_DESTROY_NO_RX_MSG));
    }
    void slowStartupAndStopStayOffGui(){
        int beats=0;QTimer heartbeat;heartbeat.setInterval(10);connect(&heartbeat,&QTimer::timeout,this,[&]{++beats;});heartbeat.start();
        std::atomic_bool entered{false},ready{false},cleaned{false};
        SipThread worker([&]{entered=true;QThread::msleep(250);ready=true;return true;},[&](unsigned){QThread::msleep(200);cleaned=true;});
        QSignalSpy stopped(&worker,&QThread::finished);worker.start();QTRY_VERIFY(entered.load());worker.stop();QTRY_COMPARE(stopped.count(),1);worker.wait();
        QVERIFY(ready.load());QVERIFY(cleaned.load());QVERIFY(beats>10);
    }
    void failedStartupCleansUp(){
        std::atomic_bool cleaned{false};SipThread worker([]{return false;},[&](unsigned flags){QCOMPARE(flags,unsigned(PJSUA_DESTROY_NO_NETWORK));cleaned=true;});
        QSignalSpy stopped(&worker,&QThread::finished);worker.start();QTRY_COMPARE(stopped.count(),1);worker.wait();QVERIFY(cleaned.load());
    }
    void retriesUseBackoffAndReset(){
        ReconnectPolicy policy(nullptr,20,80);QSignalSpy retries(&policy,&ReconnectPolicy::retryRequested);
        policy.begin();
        for(int i=0;i<5;++i){
            policy.schedule();QVERIFY(policy.remaining()>0);QVERIFY(policy.remaining()<=qMin(80,20*(1<<qMin(i,2))));
            QTRY_COMPARE(retries.count(),i+1);
        }
        QCOMPARE(policy.attempts(),5);policy.connected();QCOMPARE(policy.attempts(),0);QCOMPARE(policy.remaining(),-1);QVERIFY(policy.wanted());
        policy.schedule();QVERIFY(policy.remaining()>0&&policy.remaining()<=20);policy.cancel();
    }
    void manualDisconnectPreventsReconnect(){
        ReconnectPolicy policy(nullptr,10,40);QSignalSpy retries(&policy,&ReconnectPolicy::retryRequested);
        policy.begin();policy.schedule();policy.cancel();policy.retryNow();policy.schedule();QTest::qWait(60);
        QCOMPARE(retries.count(),0);QVERIFY(!policy.wanted());QCOMPARE(policy.remaining(),-1);
    }
    void authenticationFailurePausesRetries(){
        ReconnectPolicy policy(nullptr,10,40);QSignalSpy retries(&policy,&ReconnectPolicy::retryRequested);
        policy.begin();policy.schedule();policy.block();policy.retryNow();policy.schedule();QTest::qWait(50);
        QCOMPARE(retries.count(),0);QVERIFY(policy.blocked());
        policy.begin();policy.schedule();QTRY_COMPARE(retries.count(),1);policy.block();policy.connected();QVERIFY(!policy.blocked());
    }
    void networkRestorationAcceleratesRetry(){
        ReconnectPolicy policy(nullptr,10,80);QSignalSpy retries(&policy,&ReconnectPolicy::retryRequested);
        policy.begin();policy.schedule();QTRY_COMPARE(retries.count(),1);policy.schedule();QVERIFY(policy.remaining()<=20);
        policy.retryNow();QVERIFY(policy.remaining()>0&&policy.remaining()<=10);QCOMPARE(policy.attempts(),0);policy.cancel();
    }
    void shutdownWithoutPolling(){
        SipThread worker(nullptr);worker.stop();worker.start();QSignalSpy stopped(&worker,&QThread::finished);QTRY_COMPARE(stopped.count(),1);worker.wait();QCOMPARE(destroyFlags.load(),int(PJSUA_DESTROY_NO_NETWORK));
    }
};
QTEST_GUILESS_MAIN(SipThreadTest)
#include "sipthread_test.moc"
