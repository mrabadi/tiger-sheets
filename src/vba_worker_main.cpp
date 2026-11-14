#include "tigersheets/vba.hpp"
#include <seccomp.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <stdexcept>
namespace {
constexpr std::size_t maximum=16U*1024U*1024U;
void restrictWorker(){
 const rlimit memory{768U*1024U*1024U,768U*1024U*1024U},cpu{5,5},files{0,0},descriptors{32,32};
 if(setrlimit(RLIMIT_AS,&memory)||setrlimit(RLIMIT_CPU,&cpu)||setrlimit(RLIMIT_FSIZE,&files)||setrlimit(RLIMIT_NOFILE,&descriptors)||prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0))throw std::runtime_error("Cannot establish macro worker resource limits");
 for(int fd=3;fd<1024;++fd)close(fd);
 auto filter=seccomp_init(SCMP_ACT_ALLOW);if(!filter)throw std::runtime_error("Cannot create macro worker sandbox");
 const int denied[]={SCMP_SYS(open),SCMP_SYS(openat),SCMP_SYS(openat2),SCMP_SYS(creat),SCMP_SYS(open_by_handle_at),SCMP_SYS(socket),SCMP_SYS(socketpair),SCMP_SYS(connect),SCMP_SYS(bind),SCMP_SYS(listen),SCMP_SYS(accept),SCMP_SYS(accept4),SCMP_SYS(sendto),SCMP_SYS(recvfrom),SCMP_SYS(sendmsg),SCMP_SYS(recvmsg),SCMP_SYS(execve),SCMP_SYS(execveat),SCMP_SYS(clone),SCMP_SYS(clone3),SCMP_SYS(fork),SCMP_SYS(vfork),SCMP_SYS(ptrace),SCMP_SYS(process_vm_readv),SCMP_SYS(process_vm_writev),SCMP_SYS(mount),SCMP_SYS(umount2),SCMP_SYS(unlink),SCMP_SYS(unlinkat),SCMP_SYS(rename),SCMP_SYS(renameat),SCMP_SYS(renameat2),SCMP_SYS(mkdir),SCMP_SYS(mkdirat),SCMP_SYS(rmdir),SCMP_SYS(link),SCMP_SYS(linkat),SCMP_SYS(symlink),SCMP_SYS(symlinkat),SCMP_SYS(truncate),SCMP_SYS(ftruncate),SCMP_SYS(chmod),SCMP_SYS(fchmod),SCMP_SYS(fchmodat),SCMP_SYS(chown),SCMP_SYS(fchown),SCMP_SYS(fchownat),SCMP_SYS(lchown),SCMP_SYS(mknod),SCMP_SYS(mknodat),SCMP_SYS(io_uring_setup),SCMP_SYS(bpf),SCMP_SYS(perf_event_open)};
 for(const auto call:denied)if(seccomp_rule_add(filter,SCMP_ACT_ERRNO(EPERM),call,0)<0){seccomp_release(filter);throw std::runtime_error("Cannot configure macro worker sandbox");}
 if(seccomp_load(filter)<0){seccomp_release(filter);throw std::runtime_error("Cannot activate macro worker sandbox");}seccomp_release(filter);
}
}
int main(int argc,char** argv){
 using namespace tigersheets;Json original=nullptr,result;
 try{
  if(argc!=2||std::string(argv[1])!="--stdio")throw std::runtime_error("Use --stdio for the bounded macro protocol");
  restrictWorker();std::string input;char c=0;while(std::cin.get(c)&&c!='\n'){if(input.size()>=maximum)throw std::runtime_error("Macro request exceeds 16 MiB");input+=c;}const auto request=Json::parse(input);if(request.value("version",0)!=1)throw std::runtime_error("Unsupported macro worker protocol");original=request.at("snapshot");WorkbookSession validated(original);
  result=runMacro(validated.snapshot(),request.at("source").get<std::string>(),request.at("procedure").get<std::string>(),request.at("enabled").get<bool>());
 }catch(const std::exception& ex){result={{"status","error"},{"workbook",original},{"diagnostics",Json::array({ex.what()})},{"variables",Json::object()}};}
 result["version"]=1;const auto output=result.dump();if(output.size()>maximum)std::cout<<Json{{"version",1},{"status","error"},{"workbook",nullptr},{"diagnostics",Json::array({"Macro response exceeds 16 MiB"})}}.dump()<<'\n';else std::cout<<output<<'\n';return 0;
}
