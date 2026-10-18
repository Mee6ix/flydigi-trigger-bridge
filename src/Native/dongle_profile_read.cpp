// Read-only queries via the already installed XboxFilter. No vendor DLL init.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fltuser.h>
#include <cfgmgr32.h>
#include <array>
#include <vector>
#include <fstream>
#include <iostream>
#include <string>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <atomic>
#include <cwctype>
#include <conio.h>
using Packet=std::array<unsigned char,15>;
using Frame=std::array<unsigned char,32>;
static std::atomic<bool> quit(false),changed(false);
static bool cleanup=false;
static bool diagnosticLogging=false;
static HANDLE guiStopEvent=nullptr,guiParent=nullptr;
static void pollGuiStop(){if((guiStopEvent&&WaitForSingleObject(guiStopEvent,0)==WAIT_OBJECT_0)||(guiParent&&WaitForSingleObject(guiParent,0)==WAIT_OBJECT_0))quit=true;}
static BOOL WINAPI control(DWORD x){if(x==CTRL_C_EVENT||x==CTRL_BREAK_EVENT||x==CTRL_CLOSE_EVENT){quit=true;return TRUE;}return FALSE;}
static ULONG CALLBACK change(HCMNOTIFICATION,PVOID,CM_NOTIFY_ACTION,PCM_NOTIFY_EVENT_DATA,DWORD){changed=true;quit=true;return ERROR_SUCCESS;}
template<class T> static std::string hex(const T& b){const char* d="0123456789abcdef";std::string s;for(auto c:b){s+=d[c>>4];s+=d[c&15];}return s;}
static void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
struct LinkUnavailable:std::runtime_error{using std::runtime_error::runtime_error;};
static bool linkError(DWORD error){return error==ERROR_NO_SUCH_DEVICE||error==ERROR_DEVICE_NOT_CONNECTED||error==ERROR_DEV_NOT_EXIST;}
static void requireTransport(DWORD error,const char* operation){
    if(linkError(error))throw LinkUnavailable(std::string(operation)+": physical controller absent; Win32="+std::to_string(error));
    require(error==0,std::string(operation)+" failed; Win32="+std::to_string(error));
}
static void device(){
    ULONG n=0;require(CM_Get_Device_ID_List_SizeW(&n,nullptr,CM_GETIDLIST_FILTER_PRESENT)==CR_SUCCESS,"PnP size failed");
    std::vector<wchar_t> b(n+2);require(CM_Get_Device_ID_ListW(nullptr,b.data(),(ULONG)b.size(),CM_GETIDLIST_FILTER_PRESENT)==CR_SUCCESS,"PnP list failed");
    int count=0;bool found=false;
    for(auto p=b.data();*p;p+=wcslen(p)+1){std::wstring id=p;std::transform(id.begin(),id.end(),id.begin(),[](wchar_t c){return (wchar_t)towupper(c);});
        if(id.rfind(L"USB\\VID_045E&PID_028E\\",0)!=0)continue;++count;
        if(id==L"USB\\VID_045E&PID_028E\\FLYDIGI_VADER4"){
            DEVINST dn;ULONG status=0,problem=0;found=CM_Locate_DevNodeW(&dn,p,CM_LOCATE_DEVNODE_NORMAL)==CR_SUCCESS&&CM_Get_DevNode_Status(&status,&problem,dn,0)==CR_SUCCESS&&(status&DN_STARTED)&&!problem;
        }
    }require(count<=1,"Ambiguous physical USB 045E:028E devices");
    if(count!=1||!found)throw LinkUnavailable("Require exactly one active USB 045E:028E FLYDIGI_VADER4");
}
struct Pending{OVERLAPPED ov{};alignas(8) std::array<unsigned char,64> raw{};};
class Receiver{
    HANDLE port=INVALID_HANDLE_VALUE;std::unique_ptr<Pending> p;bool pending=false;
    void post(){p->raw.fill(0);p->ov={};p->ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);require(p->ov.hEvent!=nullptr,"Receive event failed");
        HRESULT hr=FilterGetMessage(port,(PFILTER_MESSAGE_HEADER)p->raw.data(),64,&p->ov);
        if(FAILED(hr)&&HRESULT_FACILITY(hr)==FACILITY_WIN32&&linkError(HRESULT_CODE(hr)))throw LinkUnavailable("FilterGetMessage: physical controller absent; HRESULT="+std::to_string((unsigned long)hr));
        require(hr==S_OK||hr==HRESULT_FROM_WIN32(ERROR_IO_PENDING),"FilterGetMessage HRESULT="+std::to_string((unsigned long)hr));pending=true;
    }
public:
    Receiver(){HRESULT hr=E_FAIL;ULONGLONG until=GetTickCount64()+6000;
        do{hr=FilterConnectCommunicationPort(L"\\XboxFilterPort",0,nullptr,0,nullptr,&port);
            if(hr!=HRESULT_FROM_WIN32(ERROR_CONNECTION_COUNT_LIMIT))break;Sleep(200);
        }while(GetTickCount64()<until&&!quit);
        require(SUCCEEDED(hr),"Exclusive receive port unavailable; HRESULT="+std::to_string((unsigned long)hr));p=std::make_unique<Pending>();post();}
    ~Receiver(){
        if(port!=INVALID_HANDLE_VALUE){if(pending){CancelIoEx(port,&p->ov);if(WaitForSingleObject(p->ov.hEvent,1000)!=WAIT_OBJECT_0){CloseHandle(port);p.release();return;}}CloseHandle(port);}
        if(p&&p->ov.hEvent)CloseHandle(p->ov.hEvent);
    }
    bool next(Frame& frame,std::ofstream& log,DWORD wait=25){
        DWORD result=WaitForSingleObject(p->ov.hEvent,wait);if(result==WAIT_TIMEOUT)return false;require(result==WAIT_OBJECT_0,"Receive wait failed");
        DWORD n=0;if(!GetOverlappedResult(port,&p->ov,&n,FALSE)){DWORD error=GetLastError();log<<"{\"event\":\"receive-error\",\"win32\":"<<error<<"}\n";log.flush();requireTransport(error,"Receive completion");}pending=false;
        DWORD length=0;memcpy(&length,p->raw.data()+16,4);if(length!=40){log<<"{\"event\":\"receive-length\",\"length\":"<<length<<",\"raw\":\""<<hex(p->raw)<<"\"}\n";log.flush();}require(length==40,"Unexpected driver message length="+std::to_string(length));
        // Custom 48-byte driver payload: u32 size + padding + opaque
        // 8-byte device cookie + 32-byte USB data. SDK PtrToStructure
        // skips the cookie; command byte 15 is relative to USB data.
        std::copy_n(p->raw.begin()+32,32,frame.begin());if(diagnosticLogging)log<<"{\"event\":\"frame\",\"ms\":"<<GetTickCount64()<<",\"hex\":\""<<hex(frame)<<"\"}\n";
        CloseHandle(p->ov.hEvent);p->ov.hEvent=nullptr;post();return true;
    }
};
class Queries{
    HANDLE h=INVALID_HANDLE_VALUE;bool stuck=false;
public:
    Queries(){h=CreateFileW(L"\\\\.\\XboxFilter",GENERIC_ALL,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);require(h!=INVALID_HANDLE_VALUE,"Cannot open existing XboxFilter; Win32="+std::to_string(GetLastError()));}
    ~Queries(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
    void send(const Packet& b,std::ofstream& log){
        require((!quit||cleanup)&&!changed&&!stuck,"Cancelled or device changed");device();
        require(b[0]==0xa5&&(b[1]==0x10||b[1]==0x20||b[1]==0x21),"Query whitelist rejected header");
        require(b[1]!=0x20||b[2]==0,"Query whitelist rejected current-id argument");for(int i=3;i<15;++i)require(b[i]==0,"Query whitelist rejected tail");
        struct Write{OVERLAPPED ov{};Packet data{};};auto p=std::make_unique<Write>();p->data=b;p->ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);require(p->ov.hEvent!=nullptr,"Write event failed");DWORD n=0,error=0;
        if(!DeviceIoControl(h,0xc07fe004,p->data.data(),15,nullptr,0,&n,&p->ov)){
            error=GetLastError();if(error==ERROR_IO_PENDING){DWORD waited=WaitForSingleObject(p->ov.hEvent,500);
                if(waited!=WAIT_OBJECT_0){CancelIoEx(h,&p->ov);if(WaitForSingleObject(p->ov.hEvent,1000)!=WAIT_OBJECT_0){stuck=true;p.release();throw std::runtime_error("Query cancellation incomplete");}error=ERROR_TIMEOUT;}
                else error=GetOverlappedResult(h,&p->ov,&n,FALSE)?0:GetLastError();
            }
        }CloseHandle(p->ov.hEvent);log<<"{\"event\":\"query\",\"hex\":\""<<hex(b)<<"\",\"win32\":"<<error<<"}\n";log.flush();requireTransport(error,"Query");
    }
};
static void check(){if(!cleanup){pollGuiStop();if(_kbhit()){int k=_getch();if(k=='q'||k=='Q'||k==27)quit=true;}}require((!quit||cleanup)&&!changed,"Cancelled or device changed");}
static void firmware(Queries& q,Receiver& r,std::ofstream& log,Frame* acknowledgement=nullptr){
    Packet p{};p[0]=0xa5;p[1]=0x10;p[2]=(unsigned char)(GetTickCount64()%255);q.send(p,log);
    Frame f{};ULONGLONG until=GetTickCount64()+4000;
    while(GetTickCount64()<until){check();if(r.next(f,log)&&f[14]==0xa5&&f[15]==0x10){
        log<<"{\"event\":\"firmware\",\"model\":"<<(unsigned)f[16]<<",\"version\":\""<<(f[22]>>4)<<'.'<<(f[22]&15)<<'.'<<(f[21]>>4)<<'.'<<(f[21]&15)<<"\"}\n";
        require(f[16]==105&&f[22]==0x69&&f[21]==0x59,"Require main model 105 firmware 6.9.5.9");if(acknowledgement)*acknowledgement=f;return;
    }}throw std::runtime_error("No firmware heartbeat acknowledgement");
}
static unsigned char current(Queries& q,Receiver& r,std::ofstream& log){
    Packet p{};p[0]=0xa5;p[1]=0x20;q.send(p,log);ULONGLONG until=GetTickCount64()+4000;Frame f{};
    while(GetTickCount64()<until){check();if(r.next(f,log)&&f[14]==0xa5&&f[15]==0x20){log<<"{\"event\":\"current-id\",\"id\":"<<(unsigned)f[16]<<"}\n";return f[16];}}
    throw std::runtime_error("No A5/20 acknowledgement");
}
struct Profile{std::array<unsigned char,840> bytes{};std::array<bool,84> seen{};};
static Profile read(Queries& q,Receiver& r,std::ofstream& log,unsigned char id){
    Packet p{};p[0]=0xa5;p[1]=0x21;p[2]=id;q.send(p,log);Profile result;Frame f{};ULONGLONG until=GetTickCount64()+7000;int count=0;
    while(GetTickCount64()<until){check();if(!r.next(f,log)||f[14]!=0xa5||f[15]!=0x22)continue;unsigned index=f[16];require(index<84,"Unsupported profile chunk index");
        auto start=result.bytes.begin()+index*10;
        if(result.seen[index])require(std::equal(start,start+10,f.begin()+17),"Conflicting profile chunk");else{std::copy_n(f.begin()+17,10,start);result.seen[index]=true;++count;}
        if(count==84)break;
    }
    log<<"{\"event\":\"profile\",\"receivedChunks\":"<<count<<",\"seen\":[";for(int i=0;i<84;++i){if(i)log<<',';log<<(result.seen[i]?"true":"false");}log<<"],\"hex\":\""<<hex(result.bytes)<<"\"}\n";log.flush();
    std::cout<<"Profile chunks received: "<<count<<"/84.\n";
    return result;
}
int wmain(int argc,wchar_t** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    std::wstring errorLog;
    try{
        bool queries=false;std::wstring path;int listenMs=4000;
        for(int i=1;i<argc;++i){std::wstring a=argv[i];if(a==L"--read")queries=true;else if(a==L"--log"&&i+1<argc)path=argv[++i];else if(a==L"--listen-ms"&&i+1<argc){listenMs=std::stoi(argv[++i]);require(listenMs>=100&&listenMs<=10000,"Listen duration outside 100..10000 ms");}else throw std::runtime_error("Unknown argument");}
        require(!path.empty(),"--log is required");errorLog=path;device();SetConsoleCtrlHandler(control,TRUE);
        HCMNOTIFICATION notification=nullptr;CM_NOTIFY_FILTER filter{};filter.cbSize=sizeof(filter);filter.FilterType=CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;filter.Flags=CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES;
        require(CM_Register_Notification(&filter,nullptr,change,&notification)==CR_SUCCESS,"Device monitor failed");
        struct Registration{HCMNOTIFICATION h;~Registration(){CM_Unregister_Notification(h);}} registration{notification};
        std::ofstream log(path.c_str(),std::ios::binary);require(!!log,"Cannot create log");log<<"{\"event\":\"start\",\"revision\":\"profile-read-1\",\"queries\":"<<(queries?"true":"false")<<"}\n";
        Receiver r;std::cout<<"Exclusive receive port connected. No profile writes or vibration commands.\n";
        if(!queries){Frame f{};unsigned count=0;ULONGLONG until=GetTickCount64()+listenMs;while(GetTickCount64()<until){check();if(r.next(f,log))++count;}std::cout<<"Passive frames: "<<count<<"\n";}
        else{
            Queries q;unsigned char id=current(q,r,log);std::cout<<"Current profile id: "<<(unsigned)id<<"\n";
            auto a=read(q,r,log,id);require(current(q,r,log)==id,"Active profile changed");auto b=read(q,r,log,id);require(current(q,r,log)==id,"Active profile changed");
            require(a.seen==b.seen&&a.bytes==b.bytes,"Two profile reads differ");require(a.seen[0]&&a.bytes[0]==1&&a.bytes[1]==3,"Require profile format 03.01");
            for(int i=15;i<=18;++i)require(a.seen[i],"Trigger profile chunk missing");
            log<<"{\"event\":\"stable-trigger-backup\",\"id\":"<<(unsigned)id<<",\"all840Verified\":"<<(std::all_of(a.seen.begin(),a.seen.end(),[](bool x){return x;})?"true":"false")<<",\"trigger40Bytes\":\""<<hex(std::vector<unsigned char>(a.bytes.begin()+150,a.bytes.begin()+190))<<"\"}\n";
            std::cout<<"Two reads stable; original trigger chunks captured. Missing chunks are unknown, not filled with FF.\n";
        }
        log<<"{\"event\":\"finish\",\"ok\":true,\"profileWrites\":false,\"motorWrites\":false}\n";return 0;
    }catch(const std::exception& e){std::cerr<<"Error: "<<e.what()<<"\n";if(!errorLog.empty()){std::ofstream out(errorLog.c_str(),std::ios::app|std::ios::binary);out<<"{\"event\":\"error\",\"message\":\""<<e.what()<<"\"}\n";}return 1;}
}
