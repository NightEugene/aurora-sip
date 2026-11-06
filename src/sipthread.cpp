#include "sipthread.h"
#include <QDebug>
void SipThread::run(){
    pj_thread_desc descriptor={};pj_thread_t *thread=nullptr;
    if(initializer){
        if(stopping.load())return;
        auto initialize=std::move(initializer);
        const bool ready=initialize();initialize={};
        if(!ready){
            if(cleanup)cleanup(PJSUA_DESTROY_NO_NETWORK);
            return;
        }
    } else {
        const pj_status_t registered=pj_thread_register("sip-network",descriptor,&thread);
        if(registered!=PJ_SUCCESS){qWarning()<<"Cannot register SIP network thread"<<registered;return;}
    }
    qInfo()<<"SIP network worker started";
    while(!stopping.load()){
        if(ipChange.exchange(false)){
            ipBusy.store(true);
            pjsua_ip_change_param p;pjsua_ip_change_param_default(&p);
            const auto status=pjsua_handle_ip_change(&p);
            ipBusy.store(false);emit ipChangeFinished(status);
        }
        std::vector<std::function<void()>> work;
        {std::lock_guard<std::mutex> lock(commandMutex);work.swap(commands);}
        for(auto &task:work){if(stopping.load())break;task();}
        if(!stopping.load())pjsua_handle_events(20);
    }
    // Never wait for an unreachable server's UNREGISTER during teardown.
    if(cleanup)cleanup(destroyFlags.load());
    else {
        if(audioPort){pjsua_set_no_snd_dev();pjmedia_port_destroy(audioPort);audioPort=nullptr;}
        pjsua_destroy2(destroyFlags.load());
    }
    qInfo()<<"SIP native shutdown completed";
}
