#pragma once
#include <array>
#include <stdexcept>
#include <algorithm>

// Hardware-free policy: a retained RAM patch must never replace the original
// backup, and reconnecting a different pad/profile must never authorize writes.
struct ReconnectProfile {
    using Bytes=std::array<unsigned char,840>;
    using Address=std::array<unsigned char,4>;
    static constexpr std::array<int,8> fields={155,159,162,166,169,173,176,180};
    Bytes original{},modified{};Address address{};unsigned char id=0;
    bool captured=false,writeAttempted=false,ready=false;unsigned generations=0;
    bool knownWorking(const Bytes& bytes)const{
        for(size_t i=0;i<bytes.size();++i){
            bool field=std::find(fields.begin(),fields.end(),int(i))!=fields.end();
            if(bytes[i]!=original[i]&&!(field&&bytes[i]==0))return false;
        }
        return true; // Also accepts a partially applied patch for safe rollback.
    }
    void observe(unsigned char profileId,const Address& mac,const Bytes& bytes){
        if(!std::any_of(mac.begin(),mac.end(),[](unsigned char x){return x!=0;}))throw std::runtime_error("Controller address is empty; reconnect identity cannot be verified");
        if(bytes[0]!=1||bytes[1]!=3)throw std::runtime_error("Unsupported active profile format. The validated format is 03.01");
        if(bytes[154]!=0)throw std::runtime_error("Enable Trigger Vibration Switch for the active profile in Space Station, then close Space Station and retry");
        if(bytes[230]!=0)throw std::runtime_error("The active profile contains macros. Select a profile without macros and retry");
        if(!captured){original=bytes;modified=bytes;for(int p:fields)modified[p]=0;address=mac;id=profileId;captured=true;}
        else{
            if(mac!=address)throw std::runtime_error("Different physical controller on reconnect");
            if(profileId!=id)throw std::runtime_error("Active profile changed on reconnect");
            if(!knownWorking(bytes))throw std::runtime_error("Unexpected profile on reconnect; original backup retained");
        }
        ready=false;
    }
    void beforePatch(){if(!captured)throw std::runtime_error("No original backup");writeAttempted=true;ready=false;}
    void verified(){if(!writeAttempted)throw std::runtime_error("No patch attempted");ready=true;++generations;}
    void lost(){ready=false;}
    void restored(){ready=false;writeAttempted=false;}
};
