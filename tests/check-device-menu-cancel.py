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
assert 'boolean false' in call('CallActive'),'Active call: stop checks'
def check(i):
    s=state('UiLayout')
    assert s.get('pagerIndex')==i and s.get('currentTab')==i,('index',i,s)
    assert abs(s.get('pageOffset',0)-s.get('pagerWidth',1080)*i)<2,('position',i,s)
    return s
for i in [1,2,3,0]:
    call('TapBottomTab','int32:'+str(i));time.sleep(.45);check(i)
print(json.dumps({'baselineActualClicks':True}),flush=True)
for round in range(2):
    for i in [0,1,2,3]:
        call('TapBottomTab','int32:'+str(i));time.sleep(.45);check(i)
        call('SetMenuOpen','boolean:true');time.sleep(.4)
        assert state('MenuState').get('opened'),('not opened',i)
        call('TapOutsideMenu');time.sleep(.65)
        assert not state('MenuState').get('opened'),('not cancelled',i)
        check(i)
        target=(i+1)%4
        call('TapBottomTab','int32:'+str(target));time.sleep(.45);check(target)
        print(json.dumps({'round':round,'cancelledOnTab':i,'clickedTab':target,'passed':True}),flush=True)
call('OpenContactsSettings');time.sleep(.5)
assert not state('MenuState').get('activeBarFound'),'Catalog settings must not have a menu'
call('CloseAccount');time.sleep(.5)
for i in [2,1,3,0]:
    call('TapBottomTab','int32:'+str(i));time.sleep(.45);check(i)
print(json.dumps({'returnFromSettingsThenBottomClicks':True}),flush=True)
