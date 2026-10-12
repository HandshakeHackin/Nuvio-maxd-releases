#include "diagnostics.h"
#include "buffered_log.h"
#include "util.h"
#include <csignal>
#include <cmath>
#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <unistd.h>
namespace nuvio_diagnostics {
namespace {
std::unique_ptr<BufferedLog> writer;
nlohmann::json current;
std::string root;
volatile std::sig_atomic_t crash_fd=-1, handling=0;
void crash(int signum) {
    if(handling) _exit(128+signum);
    handling=1;
    const char* marker="fatal signal: unknown\n";
    size_t length=22;
    switch(signum) {
        case SIGABRT: marker="fatal signal: SIGABRT\n"; length=22; break;
        case SIGSEGV: marker="fatal signal: SIGSEGV\n"; length=22; break;
        case SIGBUS: marker="fatal signal: SIGBUS\n"; length=21; break;
        case SIGILL: marker="fatal signal: SIGILL\n"; length=21; break;
        case SIGFPE: marker="fatal signal: SIGFPE\n"; length=21; break;
    }
    if(crash_fd>=0) { const auto ignored=::write(crash_fd,marker,length); (void)ignored; }
    std::signal(signum,SIG_DFL); std::raise(signum);
}
void rotate(const char* name, const char* previous) {
    std::rename((root+"/"+name).c_str(),(root+"/"+previous).c_str());
}
}
nlohmann::json filter_context(const nlohmann::json& input) {
    nlohmann::json output=nlohmann::json::object();
    if(!input.is_object()) return output;
    for(const char* key : {"positionSeconds","durationSeconds","bufferedSeconds","sourceFps","shownFps","shownFrames","decodedFrames","droppedFrames","renderWidth","renderHeight","acceptedRefreshHz","uiFps","worstFrameMs","frameHitches","playing","paused","buffering","nativeHdr","evo","cleanExit","sourceWidth","sourceHeight","sourceTenBit","outputFailed","playerFailed","hdrOutputActive","hdrResult"}) {
        const auto it=input.find(key);
        if(it!=input.end() && (it->is_boolean() || (it->is_number() && std::isfinite(it->get<double>())))) output[key]=*it;
    }
    const auto phase=input.find("phase");
    if(phase!=input.end() && phase->is_string()) for(const char* value : {"boot","ready","playback","fatal","output-rejected","exit"}) if(*phase==value) output["phase"]=value;
    return output;
}
void init(const std::string& directory, const std::string& version) {
    shutdown(); root=directory; make_dirs(root);
    nlohmann::json previous; load_json(root+"/runtime.json",previous);
    rotate("runtime.json","runtime-previous.json"); rotate("boot-current.txt","boot-previous.txt"); rotate("crash-current.txt","crash-previous.txt");
    const bool clean=(previous.is_object() && previous.contains("cleanExit") && previous["cleanExit"].is_boolean() && previous["cleanExit"].get<bool>());
    write_file(root+"/boot-current.txt","Nuvio Max'd "+version+"\nPrevious run recorded a clean exit: "+(clean ? "yes\n" : "no or unknown\n")+"Diagnostics omit stream URLs, account data and title names.\n");
    crash_fd=::open((root+"/crash-current.txt").c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600); handling=0;
    for(int signum : {SIGABRT,SIGSEGV,SIGBUS,SIGILL,SIGFPE}) std::signal(signum,crash);
    current={{"version",version},{"cleanExit",false},{"phase","boot"}};
    writer=std::make_unique<BufferedLog>([](const auto& batch) {
        if(!batch.empty()) write_file(root+"/runtime.json",batch.back());
    },8);
    record({});
}
void record(const nlohmann::json& context) {
    if(!writer) return;
    current.update(filter_context(context)); current["updatedEpochMs"]=now_epoch_ms();
    writer->push(current.dump(2));
}
void shutdown() {
    if(writer) { writer->close(); writer.reset(); current["cleanExit"]=true; current["phase"]="exit"; current["updatedEpochMs"]=now_epoch_ms(); write_file(root+"/runtime.json",current.dump(2)); }
    if(crash_fd>=0) { const int fd=crash_fd; crash_fd=-1; ::close(fd); }
    for(int signum : {SIGABRT,SIGSEGV,SIGBUS,SIGILL,SIGFPE}) std::signal(signum,SIG_DFL);
}
}
