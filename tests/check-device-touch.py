import subprocess,time,re,json,os
ssh=['ssh','-o','BatchMode=yes','-o','StrictHostKeyChecking=no','-o','ConnectTimeout=10',os.environ.get('AURORA_DEVICE','defaultuser@192.168.2.15')]
def call(method,arg=None):
    cmd=['dbus-send','--session','--print-reply','--dest=ru.nighteugene.aurorasip.Account','/Account','ru.nighteugene.aurorasip.Account.'+method]
    if arg is not None:cmd.append(arg)
    r=subprocess.run(ssh+cmd,text=True,capture_output=True,timeout=12)
    if r.returncode:raise RuntimeError(method+': '+r.stdout+r.stderr[-250:])
    return r.stdout
def state(method):
    out=call(method);d={}
    for key,kind,val in re.findall(r'string "([^"]+)"\s+variant\s+(boolean|int32|double|string)\s+("[^"]*"|[^\n]+)',out):
        val=val.strip();d[key]=val=='true' if kind=='boolean' else int(val) if kind=='int32' else float(val) if kind=='double' else val.strip('"')
    return d
assert 'boolean false'")])
assert 'boolean false' in call('CallActive')
def check(i):
    state_now=state('UiLayout');assert state_now.get('pagerIndex')==i and state_now.get('currentTab')==i,('index',i,state_now)
    assert abs(state_now.get('pageOffset',0)-state_now.get('pagerWidth',1080)*i)<2,('position',i,state_now)
for i in [1,2,3,0]:
    call('TouchBottomTab','int32:'+str(i));time.sleep(.5);check(i)
print(json.dumps({'touchBaseline':True}),flush=True)
for i in [0,1,2,3]:
    call('TouchBottomTab','int32:'+str(i));time.sleep(.5);check(i)
    call('SetMenuOpen','boolean:true');time.sleep(.45);assert state('MenuState').get('opened')
    call('TouchOutsideMenu');time.sleep(.7);assert not state('MenuState').get('opened'),('touch cancel',i)
    check(i)
    next_tab=(i+1)%4;call('TouchBottomTab','int32:'+str(next_tab));time.sleep(.5);check(next_tab)
    print(json.dumps({'touchCancelTab':i,'touchSelectedTab':next_tab,'passed':True}),flush=True)
for i in [2,1,3]:
    call('TouchBottomTab','int32:'+str(i));time.sleep(.5);check(i)
    call('OpenAccount');time.sleep(.6)
    assert not state('UiLayout').get('mainPageVisible')
    call('TouchBack');time.sleep(1)
    result=state('UiLayout');assert result.get('mainPageVisible'),('touch back',result)
    check(i)
    next_tab=(i+1)%4;call('TouchBottomTab','int32:'+str(next_tab));time.sleep(.5);check(next_tab)
    print(json.dumps({'touchBackFromAccountOnTab':i,'touchSelectedTab':next_tab,'passed':True}),flush=True)
call('TouchBottomTab','int32:0');time.sleep(.5);check(0)
