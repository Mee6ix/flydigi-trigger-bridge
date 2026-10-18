// System virtual Xbox One prototype. No driver installation or profile save.
#define FLYDIGI_RAM_HELPERS_ONLY
#include "dongle_ram_trigger_test.cpp"
#undef min
#undef max
#include "GameInput.h"
#include "WinUHid.h"
#include "WinUHidXOne.h"
#include <cmath>
#include <mutex>
#include <chrono>
#include <climits>
#include "bridge_reconnect_state.h"
using namespace GameInput::v3;

// Fixed identity separates the virtual device from the physical dongle.
static constexpr GUID bridgeContainer={0x3173e76a,0x8b20,0x4cdf,{0xa9,0x1c,0x7a,0x24,0x38,0x61,0xe5,0xb2}};
static constexpr wchar_t bridgeInstance[]=L"FlydigiQuadBridgePrototype";
static constexpr wchar_t bridgeHardware[]=L"HID\\VID_045E&PID_02FF&IG_00\0";
using Motors=std::array<unsigned char,4>;
struct Feedback {
    std::mutex lock;Motors value{};uint64_t sequence=0;bool invalid=false,accepting=true;
    void put(Motors x){std::lock_guard<std::mutex> hold(lock);for(auto v:x)if(v>100)invalid=true;if(accepting){value=x;++sequence;}}
    void enable(bool on){std::lock_guard<std::mutex> hold(lock);accepting=on;value={};++sequence;}
    Motors get(uint64_t* seq=nullptr){std::lock_guard<std::mutex> hold(lock);require(!invalid,"Invalid virtual motor percentage");if(seq)*seq=sequence;return value;}
};
static void callback(PVOID context,UCHAR gl,UCHAR gr,UCHAR lt,UCHAR rt){
    static_cast<Feedback*>(context)->put({gl,gr,lt,rt}); // No device I/O on WinUHid's thread.
}
static uint16_t axis(float value,bool invert=false){
    require(std::isfinite(value),"Non-finite input axis");value=std::clamp(value,-1.0f,1.0f);if(invert)value=-value;
    return static_cast<uint16_t>(std::lround((double(value)+1.0)*32767.5));
}
static uint16_t trigger(float value){require(std::isfinite(value),"Non-finite trigger");return static_cast<uint16_t>(std::lround(std::clamp(value,0.0f,1.0f)*1023.0f));}
static WINUHID_XONE_INPUT_REPORT inputReport(const GameInputGamepadState& s,bool home=false){
    WINUHID_XONE_INPUT_REPORT r{};WinUHidXOneInitializeInputReport(&r);
    r.LeftStickX=axis(s.leftThumbstickX);r.LeftStickY=axis(s.leftThumbstickY,true);
    r.RightStickX=axis(s.rightThumbstickX);r.RightStickY=axis(s.rightThumbstickY,true);
    r.LeftTrigger=trigger(s.leftTrigger);r.RightTrigger=trigger(s.rightTrigger);
    auto pressed=[&](GameInputGamepadButtons b){return (s.buttons&b)!=0;};
    r.ButtonA=pressed(GameInputGamepadA);r.ButtonB=pressed(GameInputGamepadB);
    r.ButtonX=pressed(GameInputGamepadX);r.ButtonY=pressed(GameInputGamepadY);
    r.ButtonLB=pressed(GameInputGamepadLeftShoulder);r.ButtonRB=pressed(GameInputGamepadRightShoulder);
    r.ButtonBack=pressed(GameInputGamepadView);r.ButtonMenu=pressed(GameInputGamepadMenu);
    r.ButtonLS=pressed(GameInputGamepadLeftThumbstick);r.ButtonRS=pressed(GameInputGamepadRightThumbstick);r.ButtonHome=home;
    int x=int(pressed(GameInputGamepadDPadRight))-int(pressed(GameInputGamepadDPadLeft));
    int y=int(pressed(GameInputGamepadDPadDown))-int(pressed(GameInputGamepadDPadUp));WinUHidXOneSetHatState(&r,x,y);
    // Battery is a placeholder from upstream, not a physical battery reading.
    return r;
}
static Packet feedbackPacket(const Motors& motors,unsigned phase){
    auto packet=motor(motors[0],motors[1],motors[2],motors[3],100);
    // Firmware ignores byte14 in A5/50/01, but includes it in duplicate comparison.
    // Refresh the payload without changing any game's motor amplitude; zero stays canonical.
    if(std::any_of(motors.begin(),motors.end(),[](auto value){return value!=0;}))packet[14]=static_cast<unsigned char>(phase&1);
    return packet;
}
static int linkProbe(const std::wstring& path,unsigned seconds){
    require(!path.empty(),"Link probe log required");std::ofstream log(path.c_str(),std::ios::binary);require(bool(log),"Cannot open link log");
    SetConsoleCtrlHandler(control,TRUE);device();serviceStopped();Receiver receiver;Queries queries;
    log<<"{\"event\":\"link-probe-start\",\"profileWrites\":0,\"motorWrites\":0}\n";log.flush();
    unsigned inputs=0,acks=0,nonce=0;ULONGLONG end=GetTickCount64()+seconds*1000ULL,nextQuery=0,nextSummary=0,lastInput=0,lastAck=0;
    std::cout<<"Read-only link probe active. Keep dongle in USB. Profile and motors are untouched.\n";
    try{while(GetTickCount64()<end){check();auto now=GetTickCount64();
        if(now>=nextQuery){Packet p{};p[0]=0xa5;p[1]=0x10;p[2]=static_cast<unsigned char>((++nonce)%255);queries.send(p,log);nextQuery=now+1000;}
        Frame f{};if(receiver.next(f,log,5)){
            if(f[0]==0&&f[1]==0x14){++inputs;lastInput=GetTickCount64();}
            if(f[14]==0xa5&&f[15]==0x10){++acks;lastAck=GetTickCount64();log<<"{\"event\":\"link-heartbeat\",\"ms\":"<<lastAck<<",\"model\":"<<unsigned(f[16])<<",\"hex\":\""<<hex(f)<<"\"}\n";}
        }
        if(now>=nextSummary){log<<"{\"event\":\"link-summary\",\"ms\":"<<now<<",\"inputFrames\":"<<inputs<<",\"heartbeatAcks\":"<<acks<<",\"lastInputMs\":"<<lastInput<<",\"lastAckMs\":"<<lastAck<<"}\n";log.flush();inputs=acks=0;nextSummary=now+1000;}
    }}catch(const std::exception& e){if(!quit)throw;}
    log<<"{\"event\":\"link-probe-finish\",\"profileWrites\":0,\"motorWrites\":0}\n";log.flush();return 0;
}
static void offlineTests(){
    selfTest();require(sizeof(WINUHID_XONE_INPUT_REPORT)==17,"Unexpected Xbox input ABI");
    require(axis(-1)==0&&axis(0)==32768&&axis(1)==65535&&axis(1,true)==0,"Stick conversion failed");
    require(trigger(0)==0&&trigger(0.5f)==512&&trigger(1)==1023,"Trigger conversion failed");
    GameInputGamepadState s{};s.leftTrigger=1;s.rightTrigger=.5f;s.leftThumbstickY=1;s.rightThumbstickX=-1;
    s.buttons=GameInputGamepadA|GameInputGamepadRightShoulder|GameInputGamepadDPadUp|GameInputGamepadDPadRight;
    auto r=inputReport(s,true);require(r.ButtonA&&!r.ButtonB&&r.ButtonRB&&!r.ButtonLB&&r.Hat==2&&r.ButtonHome,"Button mapping failed");
    require(r.LeftTrigger==1023&&r.RightTrigger==512&&r.LeftStickY==0&&r.RightStickX==0,"Axis report failed");
    for(int channel=0;channel<4;++channel)for(int level=0;level<=100;++level){
        Motors m{};m[channel]=static_cast<unsigned char>(level);auto a=feedbackPacket(m,0),b=feedbackPacket(m,1);
        for(int i=0;i<4;++i)require(a[4+i]==m[i]&&b[4+i]==m[i],"Game amplitude changed or motor channel leak");
        require(a[14]==0&&b[14]==(level>0?1:0),"Incorrect refresh marker");
        require(permitted(a,{},true,100,true)&&permitted(b,{},true,100,true),"Exact-amplitude packet rejected");
        require(level==0?a==b:a!=b,"Duplicate refresh not avoided");
    }
    for(Motors mixed: {Motors{1,37,61,100},Motors{100,100,100,100},Motors{0,0,0,0}}){
        for(unsigned phase=0;phase<4;++phase){auto packet=feedbackPacket(mixed,phase);
            require(std::equal(mixed.begin(),mixed.end(),packet.begin()+4),"Mixed game amplitude changed");}}
    auto invalidPacket=feedbackPacket({1,37,61,100},1);invalidPacket[14]=2;require(!permitted(invalidPacket,{},true,100,true),"Unapproved refresh marker accepted");
    invalidPacket=feedbackPacket({1,37,61,100},1);invalidPacket[8]=1;require(!permitted(invalidPacket,{},true,100,true),"Motor reserved bytes accepted");
    invalidPacket=feedbackPacket({1,37,61,100},1);invalidPacket[7]=101;require(!permitted(invalidPacket,{},true,100,true),"Excess game amplitude accepted");
    bool rejected=false;try{Motors m={101,0,0,0};feedbackPacket(m,0);}catch(...){rejected=true;}require(rejected,"Bad motor amplitude accepted");
    rejected=false;try{axis(NAN);}catch(...){rejected=true;}require(rejected,"Non-finite input accepted");
    ReconnectProfile memory;ReconnectProfile::Bytes profile{};profile[0]=1;profile[1]=3;profile[155]=1;profile[159]=1;profile[181]=77;
    ReconnectProfile::Address mac={0x11,0x22,0x33,0x44};memory.observe(0,mac,profile);memory.beforePatch();memory.verified();memory.lost();
    memory.observe(0,mac,memory.modified);require(memory.original==profile&&!memory.ready,"Retained patch replaced original or enabled output");
    memory.beforePatch();memory.verified();require(memory.generations==2,"Reconnect did not rearm");memory.lost();memory.observe(0,mac,profile);
    auto partial=profile;partial[155]=0;memory.observe(0,mac,partial);require(memory.original==profile,"Partial patch replaced original backup");
    auto foreign=profile;foreign[181]^=1;
    for(int test=0;test<3;++test){rejected=false;try{auto copy=memory;copy.observe(test==0?1:0,test==1?ReconnectProfile::Address{1,2,3,4}:mac,test==2?foreign:profile);}catch(...){rejected=true;}require(rejected,"Foreign reconnect profile/device accepted");}
    require(memory.original==profile,"Rejected reconnect damaged backup");memory.restored();require(!memory.ready&&!memory.writeAttempted,"Restore did not clear pending patch");
    Feedback gate;gate.put({0,0,100,0});gate.enable(false);gate.put({100,0,0,0});require(gate.get()==Motors{},"Offline output was buffered");gate.enable(true);require(gate.get()==Motors{},"Stale output replayed on reconnect");gate.put({0,0,0,40});require(gate.get()==Motors{0,0,0,40},"Fresh output rejected");
    require(linkError(433)&&linkError(1167)&&!linkError(ERROR_ACCESS_DENIED)&&!linkError(ERROR_TIMEOUT),"Link errors mixed with permission/timeouts");
    std::cout<<"Bridge mapping checks passed: input axes/buttons, 404 exact-amplitude motor cases, mixed values, marker refresh, stop, bounds. No device APIs called.\n";
    std::cout<<"Reconnect checks passed: reset/retained RAM, immutable backup, foreign device/profile rejection, stale feedback discarded. No device APIs called.\n";
}
struct DeviceList{std::mutex lock;std::vector<IGameInputDevice*> entries;~DeviceList(){for(auto d:entries)d->Release();}};
static void CALLBACK enumerate(GameInputCallbackToken,void* context,IGameInputDevice* d,uint64_t,GameInputDeviceStatus status,GameInputDeviceStatus){
    if(!(status&GameInputDeviceConnected))return;auto& list=*static_cast<DeviceList*>(context);std::lock_guard<std::mutex> hold(list.lock);
    if(std::find(list.entries.begin(),list.entries.end(),d)==list.entries.end()){d->AddRef();list.entries.push_back(d);}
}
class InputApi {
    HMODULE module=nullptr;
public:
    IGameInput* api=nullptr;
    InputApi(){module=LoadLibraryExW(L"GameInputRedist.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);require(module!=nullptr,"System GameInputRedist.dll unavailable");
        auto init=reinterpret_cast<HRESULT(WINAPI*)(REFIID,void**)>(GetProcAddress(module,"GameInputInitialize"));require(init!=nullptr,"GameInputInitialize unavailable");
        require(SUCCEEDED(init(IID_IGameInput,reinterpret_cast<void**>(&api)))&&api,"GameInput v3 unavailable");api->SetFocusPolicy(GameInputEnableBackgroundInput|GameInputEnableBackgroundGuideButton);}
    ~InputApi(){if(api)api->Release();if(module)FreeLibrary(module);}
    IGameInputDevice* find(bool physical,std::ofstream& log){
        auto list=std::make_unique<DeviceList>();GameInputCallbackToken token=0;
        require(SUCCEEDED(api->RegisterDeviceCallback(nullptr,GameInputKindGamepad,GameInputDeviceConnected,GameInputBlockingEnumeration,list.get(),enumerate,&token)),"Enumeration failed");
        api->StopCallback(token);bool unregistered=false;unsigned retries=0;auto deadline=GetTickCount64()+1000;
        do{if(api->UnregisterCallback(token)){unregistered=true;break;}++retries;Sleep(5);}while(GetTickCount64()<deadline);
        if(retries)log<<"{\"event\":\"enumeration-unregister-retry\",\"attempts\":"<<retries<<",\"success\":"<<(unregistered?"true":"false")<<"}\n";
        // A failed unregister cannot authorize freeing callback context.
        if(!unregistered){list.release();throw std::runtime_error("Enumeration unregister failed after bounded retry");}
        std::lock_guard<std::mutex> hold(list->lock);IGameInputDevice* match=nullptr;unsigned matches=0;
        for(auto d:list->entries){const GameInputDeviceInfo* info=nullptr;if(FAILED(d->GetDeviceInfo(&info))||!info)continue;
            bool selected=false;if(physical){std::string path=info->pnpPath?info->pnpPath:"";std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return char(toupper(c));});
                selected=info->vendorId==0x045e&&info->productId==0x028e&&path.find("FLYDIGI_VADER4")!=std::string::npos;}
            else selected=IsEqualGUID(info->containerId,bridgeContainer)&&info->vendorId==0x045e&&info->productId==0x02ff;
            if(!selected)continue;++matches;match=d;
            log<<"{\"event\":\"selected-device\",\"physical\":"<<(physical?"true":"false")<<",\"vid\":"<<info->vendorId<<",\"pid\":"<<info->productId<<",\"rumbleMask\":"<<unsigned(info->supportedRumbleMotors)<<"}\n";
            if(!physical)require(unsigned(info->supportedRumbleMotors)==15,"Virtual controller does not advertise four motors; physical forwarding refused");
        }
        require(matches<=1,"Ambiguous controller selection");if(match)match->AddRef();return match;
    }
};
class VirtualPad {
public:
    PWINUHID_XONE_GAMEPAD pad=nullptr;
    VirtualPad(Feedback& feedback){WINUHID_PRESET_DEVICE_INFO info{};info.VendorID=0x045e;info.ProductID=0x02ff;info.ContainerId=bridgeContainer;
        info.InstanceID=bridgeInstance;info.HardwareIDs=bridgeHardware;
        pad=WinUHidXOneCreate(&info,callback,&feedback);require(pad!=nullptr,"WinUHid Xbox One create failed, Win32="+std::to_string(GetLastError()));}
    ~VirtualPad(){if(pad)WinUHidXOneDestroy(pad);}
    void submit(const WINUHID_XONE_INPUT_REPORT& report){require(WinUHidXOneReportInput(pad,&report)!=FALSE,"Virtual input submission failed");}
};
struct DeviceRef{
    IGameInputDevice* d=nullptr;DeviceRef()=default;DeviceRef(const DeviceRef&)=delete;DeviceRef& operator=(const DeviceRef&)=delete;
    DeviceRef(DeviceRef&& other)noexcept:d(other.d){other.d=nullptr;}
    ~DeviceRef(){if(d)d->Release();}
};
class GuideButton {
    IGameInput* api;GameInputCallbackToken token=0;
    std::unique_ptr<std::atomic<bool>> pressed=std::make_unique<std::atomic<bool>>(false);
    static void CALLBACK update(GameInputCallbackToken,void* context,IGameInputDevice*,uint64_t,GameInputSystemButtons current,GameInputSystemButtons){
        *static_cast<std::atomic<bool>*>(context)=(current&GameInputSystemButtonGuide)!=0;
    }
public:
    GuideButton(IGameInput* input,IGameInputDevice* source):api(input){
        require(SUCCEEDED(api->RegisterSystemButtonCallback(source,GameInputSystemButtonGuide,pressed.get(),update,&token)),"Cannot register Guide input");
    }
    ~GuideButton(){if(token){api->StopCallback(token);
        // Never abort before profile restoration. If a callback cannot be
        // unregistered, retain its tiny context until process exit.
        if(!api->UnregisterCallback(token))pressed.release();}}
    bool get()const{return pressed->load();}
};
static LRESULT CALLBACK windowProc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_CLOSE){quit=true;return 0;}return DefWindowProcW(h,m,w,l);}
class FocusWindow {
    HWND h=nullptr;
public:
    FocusWindow(bool physical=false){WNDCLASSW c{};c.lpfnWndProc=windowProc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"FlydigiQuadBridgeCheck";
        require(RegisterClassW(&c)!=0||GetLastError()==ERROR_CLASS_ALREADY_EXISTS,"Cannot register test window");
        h=CreateWindowW(c.lpszClassName,physical?L"Flydigi Trigger Bridge: Grip L, Grip R, LT, RT":L"Flydigi Trigger Bridge: four-channel API check",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,560,160,nullptr,nullptr,c.hInstance,nullptr);
        require(h!=nullptr,"Cannot create API focus window");
        // The engine inherits a hidden STARTUPINFO from the background worker.
        // Windows applies that flag to the first ShowWindow call, so explicitly
        // show this intentional interactive validation window on the second call.
        ShowWindow(h,SW_SHOW);ShowWindow(h,SW_SHOWNORMAL);SetForegroundWindow(h);
        CreateWindowW(L"STATIC",physical?L"Keep this window focused. Keep triggers released.\nGrip L, Grip R, LT, RT: each 3 seconds, with pauses.":L"Keep this window focused during the four-channel API check.\nNo commands are sent to the Flydigi controller.",WS_CHILD|WS_VISIBLE,15,20,510,60,h,nullptr,c.hInstance,nullptr);}
    ~FocusWindow(){if(h)DestroyWindow(h);}
    static void poll(){MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}}
    bool focused(){return GetForegroundWindow()==h;}
    void waitForFocus(){ULONGLONG until=GetTickCount64()+30000,nextAttempt=0;while(!focused()&&GetTickCount64()<until){check();poll();if(GetTickCount64()>=nextAttempt){SetForegroundWindow(h);nextAttempt=GetTickCount64()+1000;}Sleep(50);}require(focused(),"Activate the validation window and retry");}
};
static DeviceRef virtualDevice(InputApi& api,VirtualPad& pad,std::ofstream& log){
    DeviceRef result;ULONGLONG until=GetTickCount64()+10000;GameInputGamepadState neutral{};
    while(GetTickCount64()<until){check();pad.submit(inputReport(neutral));result.d=api.find(false,log);if(result.d)return result;Sleep(100);}
    throw std::runtime_error("Virtual device not enumerated by GameInput; no physical forwarding");
}
static void apiTest(IGameInputDevice* target,Feedback& feedback,std::ofstream& log){
    FocusWindow window;window.waitForFocus();GameInputRumbleParams zero{};
    struct Stop{IGameInputDevice* d;~Stop(){GameInputRumbleParams p{};d->SetRumbleState(&p);}} stop{target};
    const char* labels[]={"Grip L","Grip R","LT","RT"};
    for(int channel=0;channel<4;++channel){check();FocusWindow::poll();require(window.focused(),"Focus lost during API test; refocus and retry");
        GameInputRumbleParams p{};float* v[]={&p.lowFrequency,&p.highFrequency,&p.leftTrigger,&p.rightTrigger};*v[channel]=0.4f;
        bool confirmed=false;ULONGLONG until=0;
        for(int attempt=1;attempt<=3&&!confirmed;++attempt){
            // Windows may still be opening the output path after enumeration.
            // Retry a complete zero/request transition, never accept a missing
            // callback. This validation never opens the physical transport.
            target->SetRumbleState(&zero);until=GetTickCount64()+150;
            while(GetTickCount64()<until){check();FocusWindow::poll();require(window.focused(),"Focus lost during API test; refocus and retry");Sleep(5);}
            uint64_t before=0;feedback.get(&before);target->SetRumbleState(&p);until=GetTickCount64()+1500;
            while(GetTickCount64()<until){check();FocusWindow::poll();require(window.focused(),"Focus lost during API test; refocus and retry");uint64_t now=0;auto values=feedback.get(&now);Motors expected{};expected[channel]=40;
                if(now>before&&values==expected){confirmed=true;break;}Sleep(5);}
            log<<"{\"event\":\"api-channel-attempt\",\"channel\":"<<channel<<",\"attempt\":"<<attempt<<",\"confirmed\":"<<(confirmed?"true":"false")<<",\"physicalWrites\":0}\n";log.flush();
        }
        require(confirmed,std::string("No independent API callback: ")+labels[channel]);
        log<<"{\"event\":\"api-channel-confirmed\",\"channel\":"<<channel<<",\"percent\":40,\"physicalWrites\":0}\n";log.flush();std::cout<<labels[channel]<<": independent callback confirmed.\n";
        target->SetRumbleState(&zero);confirmed=false;until=GetTickCount64()+1500;
        while(GetTickCount64()<until){check();FocusWindow::poll();if(feedback.get()==Motors{}){confirmed=true;break;}Sleep(5);}require(confirmed,"API zero stop callback missing");
    }
    log<<"{\"event\":\"virtual-api-pass\",\"channels\":4,\"physicalWrites\":0}\n";
}
static ULONG CALLBACK physicalChange(HCMNOTIFICATION,PVOID,CM_NOTIFY_ACTION action,PCM_NOTIFY_EVENT_DATA,DWORD){
    // Registration is for the exact physical USB instance, so creating the
    // virtual device does not look like removal of the physical dongle.
    if(action==CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED||action==CM_NOTIFY_ACTION_DEVICEQUERYREMOVE||action==CM_NOTIFY_ACTION_DEVICEREMOVEPENDING||action==CM_NOTIFY_ACTION_DEVICEREMOVECOMPLETE){changed=true;}return ERROR_SUCCESS;
}
static bool exactSourcePresent(){try{device();return true;}catch(...){return false;}}
static void pollQuit(){pollGuiStop();if(_kbhit()){int k=_getch();if(k=='q'||k=='Q'||k==27)quit=true;}}
static ReconnectProfile::Address addressOf(const Frame& f){return {f[17],f[18],f[19],f[20]};}
static Profile profileOf(const ReconnectProfile::Bytes& b){Profile p;p.bytes=b;p.seen.fill(true);return p;}
static void forward(InputApi& api,IGameInputDevice* physical,IGameInputDevice* target,VirtualPad& pad,Feedback& feedback,Receiver& receiver,RamWrites& writer,std::ofstream& log,unsigned seconds,bool bridgeCheck){
    GuideButton guide(api.api,physical);
    std::unique_ptr<FocusWindow> window;if(bridgeCheck){window=std::make_unique<FocusWindow>(true);window->waitForFocus();}
    struct ApiStop{IGameInputDevice* d;~ApiStop(){if(d){GameInputRumbleParams p{};d->SetRumbleState(&p);}}} stop{bridgeCheck?target:nullptr};
    ULONGLONG started=GetTickCount64(),nextApiRefresh=0;int previousStage=-1;unsigned inputChanges=0;std::array<bool,4> testCallbacks{};
    ULONGLONG until=seconds?GetTickCount64()+seconds*1000ULL:ULLONG_MAX,next=0,nextTelemetryFlush=0,lastInputFrame=GetTickCount64();unsigned phase=0;uint64_t lastOutputSequence=~uint64_t(0);
    WINUHID_XONE_INPUT_REPORT previous{};bool havePrevious=false;auto zero=motor(0,0,0,0);writer.send(zero,log);
    std::cout<<"Bridge active for "<<seconds<<" seconds. Game amplitudes forwarded unchanged (0..100); payload marker refresh. q/Esc/Ctrl+C stops and restores.\n";
    while(GetTickCount64()<until){check();if(!(physical->GetDeviceStatus()&GameInputDeviceConnected))throw LinkUnavailable("Physical controller disconnected");
        if(bridgeCheck){FocusWindow::poll();require(window->focused(),"Focus lost during physical API check; stopping and restoring");
            auto elapsed=GetTickCount64()-started;int stage=int(elapsed/6000);int position=int(elapsed%6000);bool on=stage<4&&position>=2000&&position<5000;
            int marker=stage*2+(on?1:0);if(marker!=previousStage){GameInputRumbleParams p{};
                if(on){float* v[]={&p.lowFrequency,&p.highFrequency,&p.leftTrigger,&p.rightTrigger};*v[stage]=1.0f;}
                target->SetRumbleState(&p);previousStage=marker;nextApiRefresh=GetTickCount64()+100;
                log<<"{\"event\":\"bridge-api-test-phase\",\"channel\":"<<stage<<",\"active\":"<<(on?"true":"false")<<",\"virtualPercent\":"<<(on?100:0)<<"}\n";log.flush();
                if(on){const char* labels[]={"Grip L","Grip R","LT","RT"};std::cout<<labels[stage]<<": 3 seconds via virtual GameInput.\n";}
                if(stage==4)std::cout<<"Motors stopped. Move a stick or press A now to check forwarded input (6 seconds).\n";
            }
            if(on){Motors expected{};expected[stage]=100;if(feedback.get()==expected)testCallbacks[stage]=true;}
            // Refresh the virtual request too, rather than relying on the HID
            // duration chosen by GameInput. Keep amplitude constant to test marker refresh.
            if(on&&GetTickCount64()>=nextApiRefresh){GameInputRumbleParams p{};float* v[]={&p.lowFrequency,&p.highFrequency,&p.leftTrigger,&p.rightTrigger};
                *v[stage]=1.0f;target->SetRumbleState(&p);nextApiRefresh=GetTickCount64()+100;}
        }
        IGameInputReading* reading=nullptr;if(FAILED(api.api->GetCurrentReading(GameInputKindGamepad,physical,&reading))||!reading)throw LinkUnavailable("Physical input unavailable");
        GameInputGamepadState state{};bool valid=reading->GetGamepadState(&state);reading->Release();require(valid,"Physical gamepad state unavailable");
        auto report=inputReport(state,guide.get());
        if(!havePrevious||memcmp(&previous,&report,sizeof(report))){pad.submit(report);previous=report;havePrevious=true;++inputChanges;
            log<<"{\"event\":\"forwarded-input\",\"leftTrigger\":"<<state.leftTrigger<<",\"rightTrigger\":"<<state.rightTrigger<<",\"leftX\":"<<state.leftThumbstickX<<",\"leftY\":"<<state.leftThumbstickY<<",\"buttons\":"<<unsigned(state.buttons)<<"}\n";}
        if(GetTickCount64()>=next){uint64_t seq=0;auto values=feedback.get(&seq);writer.send(feedbackPacket(values,phase++),log);next=GetTickCount64()+10;
            if(seq!=lastOutputSequence){log<<"{\"event\":\"game-feedback\",\"sequence\":"<<seq<<",\"percent\":["<<unsigned(values[0])<<','<<unsigned(values[1])<<','<<unsigned(values[2])<<','<<unsigned(values[3])<<"]}\n";lastOutputSequence=seq;}}
        Frame frame{};if(receiver.next(frame,log,1)&&frame[0]==0&&frame[1]==0x14)lastInputFrame=GetTickCount64();
        if(GetTickCount64()-lastInputFrame>=1500)throw LinkUnavailable("Physical input stream stopped");
        if(GetTickCount64()>=nextTelemetryFlush){log.flush();nextTelemetryFlush=GetTickCount64()+100;}
    }
    log<<"{\"event\":\"forward-summary\",\"inputChanges\":"<<inputChanges<<",\"testCallbackChannels\":[";
    for(int i=0;i<4;++i){if(i)log<<',';log<<(testCallbacks[i]?"true":"false");}log<<"]}\n";log.flush();
    if(bridgeCheck)require(std::all_of(testCallbacks.begin(),testCallbacks.end(),[](bool b){return b;}),"Not all four test callbacks arrived through virtual Xbox");
}
static ReconnectProfile loadReconnectBackup(const std::wstring& path){
    std::ifstream file(path.c_str(),std::ios::binary);require(bool(file),"Cannot read pending restore backup");
    std::string line,originalHex,addressHex;unsigned id=256;unsigned backups=0;
    auto field=[](const std::string& text,const char* key){std::string pattern="\""+std::string(key)+"\":\"";auto start=text.find(pattern);if(start==std::string::npos)return std::string{};start+=pattern.size();auto end=text.find('"',start);require(end!=std::string::npos,"Invalid backup string");return text.substr(start,end-start);};
    while(std::getline(file,line)){
        if(line.find("\"event\":\"backup\"")!=std::string::npos){++backups;originalHex=field(line,"hex");addressHex=field(line,"address");auto p=line.find("\"id\":");require(p!=std::string::npos,"Backup profile id missing");id=std::stoul(line.substr(p+5));}
        if(addressHex.empty()&&line.find("\"event\":\"connection-ready\"")!=std::string::npos)addressHex=field(line,"address");
    }
    require(backups==1&&originalHex.size()==1680&&addressHex.size()==8&&id<256,"Require one complete owned backup and controller address");
    auto byte=[](char a,char b){auto nibble=[](char c){require((c>='0'&&c<='9')||(c>='a'&&c<='f'),"Invalid backup hex");return c<='9'?c-'0':c-'a'+10;};return static_cast<unsigned char>((nibble(a)<<4)|nibble(b));};
    ReconnectProfile::Bytes original{};ReconnectProfile::Address address{};
    for(size_t i=0;i<original.size();++i)original[i]=byte(originalHex[i*2],originalHex[i*2+1]);
    for(size_t i=0;i<address.size();++i)address[i]=byte(addressHex[i*2],addressHex[i*2+1]);
    ReconnectProfile memory;memory.observe(static_cast<unsigned char>(id),address,original);memory.beforePatch();return memory;
}
static std::wstring recoveryFile;
static void durableRecovery(const std::string& line){
    if(recoveryFile.empty())return;
    HANDLE f=CreateFileW(recoveryFile.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(f!=INVALID_HANDLE_VALUE,"Cannot create durable original-profile backup");DWORD n=0;
    bool ok=WriteFile(f,line.data(),DWORD(line.size()),&n,nullptr)&&n==line.size()&&FlushFileBuffers(f);CloseHandle(f);
    require(ok,"Cannot flush original-profile backup; no RAM patch authorized");
}
static void runReconnectingBridge(InputApi& api,VirtualPad& pad,IGameInputDevice* target,Feedback& feedback,std::ofstream& log,unsigned seconds,bool bridgeCheck,const std::wstring& resumeBackup){
    ReconnectProfile memory;if(!resumeBackup.empty())memory=loadReconnectBackup(resumeBackup);
    ULONGLONG deadline=seconds?GetTickCount64()+seconds*1000ULL:ULLONG_MAX;
    std::string failure;bool restoration=false;unsigned connections=0;
    feedback.enable(false);pad.submit(inputReport(GameInputGamepadState{}));
    try{
        while(GetTickCount64()<deadline){pollQuit();if(quit)break;
            if(!exactSourcePresent()){Sleep(100);continue;}
            changed=false;DeviceRef source;source.d=api.find(true,log);if(!source.d){Sleep(100);continue;}
            try{
                Receiver receiver;Queries queries;Frame ack{};firmware(queries,receiver,log,&ack);
                auto mac=addressOf(ack);if(memory.captured)require(mac==memory.address,"Different physical controller on reconnect");
                auto id=current(queries,receiver,log);auto observed=read(queries,receiver,log,id);
                verify(queries,receiver,log,id,observed,"connection-baseline-repeat");
                memory.observe(id,mac,observed.bytes);
                if(connections==0){std::string backup="{\"event\":\"backup\",\"id\":"+std::to_string(unsigned(id))+",\"address\":\""+hex(mac)+"\",\"hex\":\""+hex(memory.original)+"\"}\n";durableRecovery(backup);log<<backup;log.flush();}
                RamWrites writer(profileOf(memory.original),profileOf(memory.modified),true,100,true);
                memory.beforePatch();patch(writer,receiver,log,profileOf(memory.modified));
                verify(queries,receiver,log,id,profileOf(memory.modified),"patch-readback");pump(receiver,log,4000);
                verify(queries,receiver,log,id,profileOf(memory.modified),"patch-after-timeout");
                memory.verified();++connections;feedback.enable(true);
                log<<"{\"event\":\"connection-ready\",\"generation\":"<<connections<<",\"address\":\""<<hex(mac)<<"\",\"retainedPatch\":"<<(observed.bytes==memory.modified?"true":"false")<<"}\n";log.flush();
                unsigned left=seconds?unsigned((deadline>GetTickCount64()?deadline-GetTickCount64():0)/1000):0;
                if(left||!seconds)forward(api,source.d,target,pad,feedback,receiver,writer,log,left,bridgeCheck);
                break;
            }catch(const std::exception& e){
                feedback.enable(false);pad.submit(inputReport(GameInputGamepadState{}));memory.lost();
                if(quit)break;
                bool lost=dynamic_cast<const LinkUnavailable*>(&e)!=nullptr||changed||!exactSourcePresent()||!(source.d->GetDeviceStatus()&GameInputDeviceConnected);
                log<<"{\"event\":\"connection-failure\",\"classifiedOffline\":"<<(lost?"true":"false")<<",\"message\":\"";
                for(unsigned char c:std::string(e.what())){if(c=='"'||c=='\\')log<<'\\'<<char(c);else if(c>=32)log<<char(c);else log<<' ';}log<<"\"}\n";log.flush();
                if(!lost)throw;
                log<<"{\"event\":\"connection-lost\",\"generation\":"<<connections<<",\"neutralInput\":true,\"staleFeedbackDiscarded\":true}\n";log.flush();
                std::cout<<"Controller offline. Virtual Xbox stays connected; waiting for the same controller.\n";
            }
        }
    }catch(const std::exception& e){failure=e.what();}
    bool patchAttempted=memory.writeAttempted;
    feedback.enable(false);pad.submit(inputReport(GameInputGamepadState{}));cleanup=true;
    try{
        if(memory.writeAttempted){
            require(exactSourcePresent(),"Controller offline at bridge stop; exact RAM restoration cannot be verified");changed=false;
            Receiver receiver;Queries queries;Frame ack{};firmware(queries,receiver,log,&ack);
            require(addressOf(ack)==memory.address,"Refuse restoration to a different controller");
            auto id=current(queries,receiver,log);require(id==memory.id,"Refuse restoration to a different active profile");
            auto observed=read(queries,receiver,log,id);memory.observe(id,addressOf(ack),observed.bytes);
            RamWrites writer(profileOf(memory.original),profileOf(memory.modified),true,100,true);writer.send(motor(0,0,0,0),log);
            patch(writer,receiver,log,profileOf(memory.original));verify(queries,receiver,log,id,profileOf(memory.original),"restored-readback");
            pump(receiver,log,4000);verify(queries,receiver,log,id,profileOf(memory.original),"restore-after-timeout");memory.restored();restoration=true;
        }
    }catch(const std::exception& e){failure="Restoration not verified: "+std::string(e.what());}
    log<<"{\"event\":\"finish\",\"restored840\":"<<(restoration?"true":"false")<<",\"profilePatchAttempted\":"<<(patchAttempted?"true":"false")<<",\"connections\":"<<connections<<",\"gameBridgePhysicalResult\":null}\n";log.flush();
    require(failure.empty(),failure);std::cout<<(restoration?"Original profile restored.\n":"No RAM patch attempted.\n");
}
static int restoreProfile(const std::wstring& path,const std::wstring& backup){
    require(!path.empty()&&!backup.empty(),"Restoration requires an owned backup and log");
    std::ofstream log(path.c_str(),std::ios::binary);require(bool(log),"Cannot create restoration log");
    auto memory=loadReconnectBackup(backup);device();serviceStopped();cleanup=true;changed=false;
    Receiver receiver;Queries queries;Frame ack{};firmware(queries,receiver,log,&ack);
    require(addressOf(ack)==memory.address,"Refuse restoration to a different controller");
    auto id=current(queries,receiver,log);require(id==memory.id,"Refuse restoration to another profile");
    auto observed=read(queries,receiver,log,id);memory.observe(id,addressOf(ack),observed.bytes);
    RamWrites writer(profileOf(memory.original),profileOf(memory.modified),true,100,true);writer.send(motor(0,0,0,0),log);
    patch(writer,receiver,log,profileOf(memory.original));verify(queries,receiver,log,id,profileOf(memory.original),"restored-readback");
    pump(receiver,log,4000);verify(queries,receiver,log,id,profileOf(memory.original),"restore-after-timeout");
    log<<"{\"event\":\"finish\",\"restored840\":true,\"profilePatchAttempted\":true,\"restorationOnly\":true}\n";log.flush();return 0;
}
int wmain(int argc,wchar_t** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    std::wstring errorLog;
    try{
        bool self=false,preflight=false,virtualCheck=false,bridge=false,bridgeCheck=false,sourceProbe=false,probeLink=false,restoreOnly=false;unsigned seconds=120;std::wstring path,resumeBackup;
        for(int i=1;i<argc;++i){std::wstring a=argv[i];if(a==L"--self-test")self=true;else if(a==L"--preflight")preflight=true;else if(a==L"--virtual-check")virtualCheck=true;
            else if(a==L"--bridge")bridge=true;else if(a==L"--bridge-check")bridgeCheck=true;else if(a==L"--source-probe")sourceProbe=true;else if(a==L"--link-probe")probeLink=true;else if(a==L"--log"&&i+1<argc)path=argv[++i];
            else if(a==L"--duration-seconds"&&i+1<argc){std::wstring value=argv[++i];require(value.find_first_not_of(L"0123456789")==std::wstring::npos&&!value.empty(),"Invalid duration");seconds=std::stoul(value);}
            else if(a==L"--resume-backup"&&i+1<argc)resumeBackup=argv[++i];
            else if(a==L"--diagnostic-logging")diagnosticLogging=true;
            else if(a==L"--restore-profile")restoreOnly=true;
            else if(a==L"--recovery-file"&&i+1<argc)recoveryFile=argv[++i];
            else if(a==L"--stop-event"&&i+1<argc){guiStopEvent=OpenEventW(SYNCHRONIZE,FALSE,argv[++i]);require(guiStopEvent!=nullptr,"Cannot open application stop event");}
            else if(a==L"--parent-pid"&&i+1<argc){guiParent=OpenProcess(SYNCHRONIZE,FALSE,std::stoul(argv[++i]));require(guiParent!=nullptr,"Cannot monitor application lifetime");}
            else throw std::runtime_error("Unknown bridge argument");}
        require(int(self)+int(preflight)+int(virtualCheck)+int(bridge)+int(bridgeCheck)+int(sourceProbe)+int(probeLink)+int(restoreOnly)<=1,"Choose one bridge mode");if(bridgeCheck){bridge=true;seconds=30;}require(seconds==0||(seconds>=10&&seconds<=86400),"Duration must be zero (until stopped) or10..86400 seconds");
        struct GuiHandles{~GuiHandles(){if(guiStopEvent)CloseHandle(guiStopEvent);if(guiParent)CloseHandle(guiParent);}} guiHandles;
        require(resumeBackup.empty()||bridge||restoreOnly,"Pending backup applies only to bridge/restoration mode");
        if(self){offlineTests();return 0;}
        if(preflight){DWORD v=WinUHidGetDriverInterfaceVersion();std::cout<<"{\"event\":\"winuhid-preflight\",\"interfaceVersion\":"<<v<<",\"win32\":"<<(v?0:GetLastError())<<",\"virtualDevicesCreated\":0,\"physicalWrites\":0}\n";return v?0:2;}
        if(probeLink){errorLog=path;return linkProbe(path,seconds);}
        if(restoreOnly){errorLog=path;return restoreProfile(path,resumeBackup);}
        if(sourceProbe){require(!path.empty(),"Source probe log required");errorLog=path;std::ofstream log(path.c_str(),std::ios::binary);require(bool(log),"Cannot open source log");
            InputApi api;DeviceRef source;source.d=api.find(true,log);IGameInputReading* reading=nullptr;
            HRESULT hr=source.d?api.api->GetCurrentReading(GameInputKindGamepad,source.d,&reading):E_FAIL;
            GameInputGamepadState state{};bool readable=reading&&reading->GetGamepadState(&state);if(reading)reading->Release();
            log<<"{\"event\":\"source-probe\",\"sourceVisible\":"<<(source.d?"true":"false")<<",\"sourceReadable\":"<<(readable?"true":"false")<<",\"hresult\":"<<long(hr)<<",\"physicalWrites\":0,\"virtualDevicesCreated\":0}\n";
            std::cout<<"Source visible="<<bool(source.d)<<", readable="<<readable<<". No physical writes or virtual devices.\n";return readable?0:3;}
        if(!virtualCheck&&!bridge){std::cout<<"Preview. --virtual-check tests four GameInput channels on a virtual Xbox One only; --bridge forwards physical input and game rumble with temporary RAM restoration. No I/O in preview.\n";return 0;}
        require(!path.empty(),"Log required");errorLog=path;std::ofstream log(path.c_str(),std::ios::binary);require(bool(log),"Cannot open bridge log");SetConsoleCtrlHandler(control,TRUE);
        log<<"{\"event\":\"start\",\"revision\":\"flydigi-trigger-bridge-app-0.1.2\",\"physicalForwarding\":"<<(bridge?"true":"false")<<",\"amplitudeMapping\":\"identity-0-100\",\"refreshMarkerByte\":14}\n";log.flush();
        require(WinUHidGetDriverInterfaceVersion()>0,"WinUHid driver missing or administrator rights required; no automatic installation");
        Feedback feedback;VirtualPad pad(feedback);InputApi api;auto target=virtualDevice(api,pad,log);
        if(virtualCheck){apiTest(target.d,feedback,log);log<<"{\"event\":\"finish\",\"virtualApiPassed\":true,\"physicalWrites\":0}\n";return 0;}
        device();serviceStopped();DeviceRef source;source.d=api.find(true,log);require(source.d!=nullptr,"Exact physical Flydigi GameInput device unavailable");
        HCMNOTIFICATION notification=nullptr;CM_NOTIFY_FILTER filter{};filter.cbSize=sizeof(filter);filter.FilterType=CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE;
        wcscpy_s(filter.u.DeviceInstance.InstanceId,L"USB\\VID_045E&PID_028E\\FLYDIGI_VADER4");
        require(CM_Register_Notification(&filter,nullptr,physicalChange,&notification)==CR_SUCCESS,"Cannot monitor physical dongle");
        struct Registration{HCMNOTIFICATION h;~Registration(){CM_Unregister_Notification(h);}} registration{notification};
        runReconnectingBridge(api,pad,target.d,feedback,log,seconds,bridgeCheck,resumeBackup);return 0;
    }catch(const std::exception& e){std::cerr<<"Bridge error: "<<e.what()<<"\n";
        if(!errorLog.empty()){std::ofstream out(errorLog.c_str(),std::ios::app|std::ios::binary);out<<"{\"event\":\"native-error\",\"message\":\"";
            for(const unsigned char c:std::string(e.what())){if(c=='"'||c=='\\')out<<'\\'<<char(c);else if(c>=32)out<<char(c);else out<<' ';}out<<"\"}\n";}
        return 1;}
}
