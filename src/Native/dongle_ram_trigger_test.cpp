// RAM-only profile experiment. Includes audited read/receive utilities.
#define wmain read_probe_entry
#include "dongle_profile_read.cpp"
#undef wmain
#include <winsvc.h>
static const std::array<int,8> offsets={155,159,162,166,169,173,176,180};
static Packet startRam(){Packet b{};b[0]=0xa5;b[1]=0x23;b[3]=84;return b;}
static Packet chunk(const Profile& profile,int index){require(index>=15&&index<=18,"RAM chunk outside trigger block");Packet b{};b[0]=0xa5;b[1]=0x24;std::copy_n(profile.bytes.begin()+index*10,10,b.begin()+2);b[12]=0xa0;b[13]=(unsigned char)index;return b;}
static Packet motor(int gl,int gr,int lt,int rt,int amplitudeMax=41){Packet b{};b[0]=0xa5;b[1]=0x50;b[2]=1;require(amplitudeMax==41||amplitudeMax==100,"Invalid motor range");int v[]={gl,gr,lt,rt};for(int i=0;i<4;++i){require(v[i]>=0&&v[i]<=amplitudeMax,"Motor amplitude exceeds selected range");b[4+i]=(unsigned char)v[i];}return b;}
static bool permitted(const Packet& b,const std::vector<Packet>& allowed,bool permitMotors,int amplitudeMax=41,bool refreshMarker=false){
    bool motorOk=permitMotors&&b[0]==0xa5&&b[1]==0x50&&b[2]==1&&b[3]==0;
    motorOk=motorOk&&(amplitudeMax==41||amplitudeMax==100)&&(!refreshMarker||amplitudeMax==100);
    for(int i=4;i<8;++i)motorOk=motorOk&&b[i]<=amplitudeMax;for(int i=8;i<14;++i)motorOk=motorOk&&b[i]==0;
    motorOk=motorOk&&(refreshMarker?b[14]<=1:b[14]==0);
    return motorOk||std::find(allowed.begin(),allowed.end(),b)!=allowed.end();
}
static void selfTest(){
    Profile a;for(int i=0;i<840;++i)a.bytes[i]=(unsigned char)(i*71+13);auto b=a;for(int i:offsets)b.bytes[i]=0;
    std::vector<Packet> allowed={startRam()};for(int i=15;i<=18;++i){allowed.push_back(chunk(a,i));allowed.push_back(chunk(b,i));}
    require(permitted(startRam(),allowed,false)&&permitted(chunk(b,17),allowed,false),"Allowed RAM packets rejected");
    for(auto command:{0x11,0x25,0x28,0x30,0x41,0x51}){auto p=startRam();p[1]=(unsigned char)command;require(!permitted(p,allowed,true),"Foreign command accepted");}
    auto p=startRam();p[4]=0xa0;require(!permitted(p,allowed,false),"Profile selector accepted");p=startRam();p[3]=4;require(!permitted(p,allowed,false),"Different session count accepted");
    p=chunk(b,15);p[2]^=1;require(!permitted(p,allowed,false),"Neighbor profile byte modification accepted");p=chunk(b,15);p[13]=14;require(!permitted(p,allowed,false),"Foreign chunk index accepted");
    require(!permitted(motor(0,0,40,0),allowed,false),"Roundtrip permits motors");require(permitted(motor(0,0,41,0),allowed,true),"Bounded motor rejected");p=motor(0,0,40,0);p[6]=42;require(!permitted(p,allowed,true),"Excess motor level accepted");p=motor(0,0,40,0);p[14]=1;require(!permitted(p,allowed,true),"Motor tail mutation accepted");
    std::cout<<"RAM-test whitelist checks passed; no device APIs called.\n";
}
static void serviceStopped(){SC_HANDLE scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);require(scm!=nullptr,"Cannot query service manager");SC_HANDLE s=OpenServiceW(scm,L"Flydigi Space Station Service",SERVICE_QUERY_STATUS);if(!s){CloseServiceHandle(scm);throw std::runtime_error("Cannot query Space Station service");}SERVICE_STATUS status{};BOOL ok=QueryServiceStatus(s,&status);CloseServiceHandle(s);CloseServiceHandle(scm);require(ok&&status.dwCurrentState==SERVICE_STOPPED,"Space Station service must remain stopped for RAM experiment");}
class RamWrites{
    HANDLE h=INVALID_HANDLE_VALUE;std::vector<Packet> allowed;bool permitMotors=false,stuck=false;int amplitudeMax=41;bool refreshMarker=false;
    Packet previousMotor{};bool haveMotor=false;
public:
    RamWrites(const Profile& original,const Profile& modified,bool motors,int maximum=41,bool marker=false):permitMotors(motors),amplitudeMax(maximum),refreshMarker(marker){
        require((maximum==41||maximum==100)&&(!marker||maximum==100),"Invalid writer motor range/marker");
        allowed.push_back(startRam());for(int i=15;i<=18;++i){allowed.push_back(chunk(original,i));allowed.push_back(chunk(modified,i));}
        h=CreateFileW(L"\\\\.\\XboxFilter",GENERIC_ALL,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);require(h!=INVALID_HANDLE_VALUE,"Cannot open XboxFilter for RAM test");
    }
    ~RamWrites(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
    void send(const Packet& b,std::ofstream& log){
        check();device();serviceStopped();require(!stuck,"Prior write cancellation incomplete");
        require(permitted(b,allowed,permitMotors,amplitudeMax,refreshMarker),"Rejected RAM-test packet");
        struct Write{OVERLAPPED ov{};Packet data{};};auto p=std::make_unique<Write>();p->data=b;p->ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);require(p->ov.hEvent!=nullptr,"Write event failed");DWORD n=0,error=0;
        if(!DeviceIoControl(h,0xc07fe004,p->data.data(),15,nullptr,0,&n,&p->ov)){
            error=GetLastError();if(error==ERROR_IO_PENDING){if(WaitForSingleObject(p->ov.hEvent,500)!=WAIT_OBJECT_0){CancelIoEx(h,&p->ov);if(WaitForSingleObject(p->ov.hEvent,1000)!=WAIT_OBJECT_0){stuck=true;p.release();throw std::runtime_error("RAM write cancellation incomplete");}error=ERROR_TIMEOUT;}else error=GetOverlappedResult(h,&p->ov,&n,FALSE)?0:GetLastError();}
        }CloseHandle(p->ov.hEvent);
        bool logWrite=diagnosticLogging||error||b[1]!=0x50||!haveMotor||!std::equal(b.begin()+4,b.begin()+8,previousMotor.begin()+4);
        if(logWrite){log<<"{\"event\":\"ram-test-write\",\"hex\":\""<<hex(b)<<"\",\"win32\":"<<error<<"}\n";log.flush();}
        if(b[1]==0x50){previousMotor=b;haveMotor=true;}requireTransport(error,"RAM test write");
    }
};
static void pump(Receiver& r,std::ofstream& log,DWORD duration){Frame f{};ULONGLONG until=GetTickCount64()+duration;while(GetTickCount64()<until){check();r.next(f,log,10);}}
static void patch(RamWrites& w,Receiver& r,std::ofstream& log,const Profile& profile){
    w.send(startRam(),log);pump(r,log,100);
    for(int index=15;index<=18;++index){Packet b=chunk(profile,index);w.send(b,log);Frame f{};ULONGLONG until=GetTickCount64()+2000;bool acknowledged=false;
        while(GetTickCount64()<until){check();if(r.next(f,log)&&f[14]==0xa5&&f[15]==0x24&&f[16]==index+1&&std::equal(b.begin()+2,b.begin()+12,f.begin()+17)){acknowledged=true;break;}}
        require(acknowledged,"No matching RAM-chunk acknowledgement");
    }
}
static void verify(Queries& q,Receiver& r,std::ofstream& log,unsigned char id,const Profile& expected,const char* label){
    require(current(q,r,log)==id,"Active profile changed");auto actual=read(q,r,log,id);require(std::all_of(actual.seen.begin(),actual.seen.end(),[](bool v){return v;}),"Complete profile readback required");require(actual.bytes==expected.bytes,std::string(label)+": 840-byte readback mismatch");
    log<<"{\"event\":\"verified\",\"label\":\""<<label<<"\",\"bytes\":840}\n";log.flush();
}
#ifndef FLYDIGI_RAM_HELPERS_ONLY
int wmain(int argc,wchar_t** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);std::wstring errorLog;
    try{
        bool send=false,roundtrip=false,self=false;std::wstring path;
        for(int i=1;i<argc;++i){std::wstring a=argv[i];if(a==L"--send")send=true;else if(a==L"--roundtrip")roundtrip=true;else if(a==L"--self-test")self=true;else if(a==L"--log"&&i+1<argc)path=argv[++i];else throw std::runtime_error("Unknown RAM-test argument");}
        if(self){require(!send&&!roundtrip&&path.empty(),"Conflicting self-test arguments");selfTest();return 0;}
        require(!(send&&roundtrip),"Choose send or roundtrip");if(!send&&!roundtrip){std::cout<<"Preview only; no device I/O. RAM Type and MinStart offsets: 155,159,162,166,169,173,176,180.\n";return 0;}
        require(!path.empty(),"Log is required");errorLog=path;device();serviceStopped();SetConsoleCtrlHandler(control,TRUE);
        HCMNOTIFICATION notification=nullptr;CM_NOTIFY_FILTER filter{};filter.cbSize=sizeof(filter);filter.FilterType=CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;filter.Flags=CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES;require(CM_Register_Notification(&filter,nullptr,change,&notification)==CR_SUCCESS,"Cannot monitor devices");struct Registration{HCMNOTIFICATION h;~Registration(){CM_Unregister_Notification(h);}} registration{notification};
        std::ofstream log(path.c_str(),std::ios::binary);require(!!log,"Cannot create log");log<<"{\"event\":\"start\",\"revision\":\"ram-trigger-1\",\"motors\":"<<(send?"true":"false")<<"}\n";Receiver r;Queries q;firmware(q,r,log);
        unsigned char id=current(q,r,log);auto original=read(q,r,log,id);verify(q,r,log,id,original,"baseline-repeat");
        require(original.bytes[0]==1&&original.bytes[1]==3&&original.bytes[154]==0&&original.bytes[230]==0,"Require 03.01 profile with trigger vibration enabled and no macros");
        auto modified=original;for(int offset:offsets)modified.bytes[offset]=0;
        log<<"{\"event\":\"backup\",\"id\":"<<(unsigned)id<<",\"hex\":\""<<hex(original.bytes)<<"\"}\n";log.flush();
        RamWrites w(original,modified,send);bool attempted=false,restored=false;std::string failure;
        try{
            attempted=true;patch(w,r,log,modified);verify(q,r,log,id,modified,"patch-readback");pump(r,log,4000);verify(q,r,log,id,modified,"patch-after-timeout");
            if(send){
                struct Phase{const char* name;Packet p;};std::vector<Phase> phases={{"Grip L",motor(40,0,0,0)},{"Grip R",motor(0,40,0,0)},{"LT",motor(0,0,40,0)},{"RT",motor(0,0,0,40)},{"LT+RT",motor(0,0,40,40)},{"All four",motor(40,40,40,40)}};
                w.send(motor(0,0,0,0),log);std::cout<<"Keep triggers released. q/Esc/Ctrl+C stops motors and restores profile.\n";
                for(const auto& phase:phases){std::cout<<"Prepare "<<phase.name<<" (2 seconds).\n";pump(r,log,2000);std::cout<<phase.name<<": 3 seconds.\n";ULONGLONG until=GetTickCount64()+3000,next=0;unsigned n=0;while(GetTickCount64()<until){check();if(GetTickCount64()>=next){auto b=phase.p;if(n++&1)for(int i=4;i<8;++i)if(b[i])++b[i];w.send(b,log);next=GetTickCount64()+10;}Frame f{};r.next(f,log,1);}w.send(motor(0,0,0,0),log);pump(r,log,1000);}
            }
        }catch(const std::exception& e){failure=e.what();}
        cleanup=true;
        try{if(send)w.send(motor(0,0,0,0),log);if(attempted){patch(w,r,log,original);verify(q,r,log,id,original,"restored-readback");pump(r,log,4000);verify(q,r,log,id,original,"restore-after-timeout");restored=true;}}
        catch(const std::exception& e){log<<"{\"event\":\"restore-failed\",\"message\":\""<<e.what()<<"\"}\n";log.flush();throw;}
        log<<"{\"event\":\"finish\",\"restored840\":"<<(restored?"true":"false")<<",\"physicalMotorResult\":null}\n";require(failure.empty(),failure);std::cout<<"Original 840-byte profile restored and read back.\n";return 0;
    }catch(const std::exception& e){std::cerr<<"Error: "<<e.what()<<"\n";if(!errorLog.empty()){std::ofstream out(errorLog.c_str(),std::ios::app|std::ios::binary);out<<"{\"event\":\"error\",\"message\":\""<<e.what()<<"\"}\n";}return 1;}
}
#endif
