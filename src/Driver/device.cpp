// Only the exact root node owned by this application may be created/removed.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <newdev.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>
#include <string>
#include <iostream>
#include <stdexcept>
static const GUID systemClass={0x4d36e97d,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};
static constexpr wchar_t instance[]=L"ROOT\\WINUHID\\FLYDIGI_BRIDGE";
static constexpr wchar_t hardware[]=L"Root\\WinUHid\0";
static constexpr wchar_t label[]=L"Flydigi Bridge WinUHid Enumerator";
static void must(bool ok,const char* why){if(!ok)throw std::runtime_error(std::string(why)+"; Win32="+std::to_string(GetLastError()));}
struct Info{HDEVINFO h=INVALID_HANDLE_VALUE;Info(){h=SetupDiCreateDeviceInfoList(&systemClass,nullptr);must(h!=INVALID_HANDLE_VALUE,"Setup list");}~Info(){SetupDiDestroyDeviceInfoList(h);}};
static std::wstring property(HDEVINFO h,SP_DEVINFO_DATA& d,const DEVPROPKEY& key){
    DEVPROPTYPE type=0;wchar_t out[2048]{};DWORD required=0;
    if(!SetupDiGetDevicePropertyW(h,&d,&key,&type,reinterpret_cast<PBYTE>(out),sizeof(out),&required,0))return L"";
    must(type==DEVPROP_TYPE_STRING,"Unexpected property type");return out;
}
static std::string ascii(const std::wstring& s){std::string out;for(wchar_t c:s){must(c>=32&&c<127&&c!='"'&&c!='\\',"Unsafe output field");out+=char(c);}return out;}
int wmain(int argc,wchar_t** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    try{
        if(argc==1){std::cout<<"Preview only. Explicit --create INF or --remove affects ROOT\\WINUHID\\FLYDIGI_BRIDGE only.\n";return 0;}
        std::wstring mode=argv[1];must((mode==L"--create"&&argc==3)||((mode==L"--remove"||mode==L"--verify-owner"||mode==L"--check-absent")&&argc==2),"Arguments");
        Info list;SP_DEVINFO_DATA d{};d.cbSize=sizeof(d);bool exists=SetupDiOpenDeviceInfoW(list.h,instance,nullptr,0,&d)!=FALSE;
        if(mode==L"--remove"||mode==L"--verify-owner"){
            if(!exists){std::cout<<"{\"removed\":false,\"absent\":true}\n";return 0;}
            wchar_t friendly[256]{};DWORD type=0;
            bool marked=SetupDiGetDeviceRegistryPropertyW(list.h,&d,SPDRP_FRIENDLYNAME,&type,reinterpret_cast<PBYTE>(friendly),sizeof(friendly),nullptr)&&type==REG_SZ&&std::wstring(friendly)==label;
            auto provider=property(list.h,d,DEVPKEY_Device_DriverProvider);must(provider.empty()||provider==L"Flydigi Bridge Driver","Foreign driver provider");
            // PnP binding removes the pre-install friendly-name property.
            // Exact owned instance plus our package provider identifies the
            // installed node; the marker is retained for unbound partial nodes.
            must(marked||provider==L"Flydigi Bridge Driver","Foreign node owner identity");
            if(mode==L"--verify-owner"){std::cout<<"{\"owned\":true,\"publishedInf\":\""<<ascii(property(list.h,d,DEVPKEY_Device_DriverInfPath))<<"\",\"deviceChanges\":0}\n";return 0;}
            must(SetupDiCallClassInstaller(DIF_REMOVE,list.h,&d)!=FALSE,"Remove owned node");std::cout<<"{\"removed\":true}\n";return 0;
        }
        must(!exists,"Own root node already exists; do not overwrite");
        HDEVINFO all=SetupDiGetClassDevsW(nullptr,nullptr,nullptr,DIGCF_ALLCLASSES);must(all!=INVALID_HANDLE_VALUE,"Enumerate existing root devices");
        bool foreign=false;SP_DEVINFO_DATA other{};other.cbSize=sizeof(other);
        for(DWORD i=0;SetupDiEnumDeviceInfo(all,i,&other);++i){wchar_t ids[2048]{};DWORD type=0;
            if(!SetupDiGetDeviceRegistryPropertyW(all,&other,SPDRP_HARDWAREID,&type,reinterpret_cast<PBYTE>(ids),sizeof(ids),nullptr)||type!=REG_MULTI_SZ)continue;
            for(const wchar_t* p=ids;*p;p+=wcslen(p)+1)if(_wcsicmp(p,L"Root\\WinUHid")==0)foreign=true;
        }
        SetupDiDestroyDeviceInfoList(all);must(!foreign,"Another WinUHid root node exists; do not bind or overwrite it");
        if(mode==L"--check-absent"){std::cout<<"{\"winuhidRootsAbsent\":true,\"deviceChanges\":0}\n";return 0;}
        wchar_t inf[MAX_PATH]{};must(GetFullPathNameW(argv[2],MAX_PATH,inf,nullptr)>0,"INF path");
        must(GetFileAttributesW(inf)!=INVALID_FILE_ATTRIBUTES,"INF missing");
        must(SetupDiCreateDeviceInfoW(list.h,instance,&systemClass,label,nullptr,0,&d)!=FALSE,"Create exact root node");
        must(SetupDiSetDeviceRegistryPropertyW(list.h,&d,SPDRP_HARDWAREID,reinterpret_cast<const BYTE*>(hardware),sizeof(hardware))!=FALSE,"Set hardware ID");
        must(SetupDiSetDeviceRegistryPropertyW(list.h,&d,SPDRP_FRIENDLYNAME,reinterpret_cast<const BYTE*>(label),sizeof(label))!=FALSE,"Set owner marker");
        must(SetupDiCallClassInstaller(DIF_REGISTERDEVICE,list.h,&d)!=FALSE,"Register owned root node");
        BOOL reboot=FALSE;
        if(!UpdateDriverForPlugAndPlayDevicesW(nullptr,L"Root\\WinUHid",inf,0,&reboot)){
            DWORD error=GetLastError();SetupDiCallClassInstaller(DIF_REMOVE,list.h,&d);SetLastError(error);must(false,"Bind local driver (new node removed)");
        }
        auto provider=property(list.h,d,DEVPKEY_Device_DriverProvider);auto published=property(list.h,d,DEVPKEY_Device_DriverInfPath);
        must(provider==L"Flydigi Bridge Driver","Installed provider mismatch");
        must(!published.empty(),"Installed INF identity unavailable");
        std::cout<<"{\"created\":true,\"publishedInf\":\""<<ascii(published)<<"\",\"rebootRequired\":"<<(reboot?"true":"false")<<"}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"Driver device error: "<<e.what()<<"\n";return 1;}
}
