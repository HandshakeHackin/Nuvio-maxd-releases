"""Failure-inject the production console decoder CPU workspace functions."""
from pathlib import Path
import subprocess,os
r=Path(__file__).resolve().parents[1];s=(r/'third_party/vlc/modules/ps5vdec.c').read_text()
code=s[s.index('typedef struct {\n    void *addr;'):s.index('/* One compute queue for the app')]
p=r/'test-output/vlc-workspace.c';p.parent.mkdir(exist_ok=True)
p.write_text('''#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#define vlc_mutex_t pthread_mutex_t
#define VLC_STATIC_MUTEX PTHREAD_MUTEX_INITIALIZER
#define vlc_mutex_lock pthread_mutex_lock
#define vlc_mutex_unlock pthread_mutex_unlock
#define PAGE_16K ((size_t)0x4000)
#define PROT_CPU_RW 3
#define DIRECT_TYPE 12
#define VLOG(...) ((void)0)
static bool flex_fail=true,direct_fail;
static int maps,releases,unmaps;static size_t mapped;
static int64_t sceKernelGetDirectMemorySize(void){return (int64_t)8<<30;}
static int sceKernelAllocateDirectMemory(int64_t a,int64_t b,size_t n,size_t align,int type,int64_t*out){(void)a;(void)b;(void)n;assert(align>=PAGE_16K && type==12);*out=0;return direct_fail?-1:0;}
static int sceKernelMapDirectMemory(void**p,size_t n,int prot,int flags,int64_t start,size_t align){(void)n;(void)flags;(void)start;(void)align;assert(prot==3);*p=(void*)0x20000000;return 0;}
static int sceKernelMapFlexibleMemory(void**p,size_t n,int prot,int flags){(void)prot;(void)flags;++maps;mapped=n;if(flex_fail)return -1;*p=(void*)0x10000000;return 0;}
static int sceKernelReleaseDirectMemory(int64_t a,size_t n){(void)a;(void)n;++releases;return 0;}
static int sceKernelMunmap(void*p,size_t n){(void)p;(void)n;++unmaps;return 0;}
'''+code+'''
int main(void){
 Mem a={0},b={0};
 assert(mem_cpu(&a,72u<<20) && a.start>=0 && !cpu_block_busy);
 mem_free(&a);assert(!a.addr && releases==1 && unmaps==1);
 direct_fail=true;assert(!mem_cpu(&a,72u<<20) && !a.addr);direct_fail=false;
 flex_fail=false;assert(mem_cpu(&a,32u<<20) && a.start==-2 && cpu_block_busy && mapped==(96u<<20));
 int before=maps;assert(mem_cpu(&b,32u<<20) && b.start>=0);mem_free(&b);assert(cpu_block_busy && maps==before);
 mem_free(&a);assert(!cpu_block_busy);assert(mem_cpu(&a,72u<<20) && a.start==-2 && maps==before);mem_free(&a);
 flex_fail=true;assert(mem_cpu(&a,128u<<20) && a.start>=0);mem_free(&a);
 puts("VLC workspace: exhausted flexible memory uses direct storage; shared-block ownership and failure cleanup passed");
}
''')
exe=r/'test-output/vlc-workspace'
subprocess.run([os.environ.get('HOST_CC','gcc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-pthread',str(p),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
