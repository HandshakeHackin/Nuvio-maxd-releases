#!/usr/bin/env python3
"""Compile the shipped module's parsers; no substitute decoder or PS5 claim."""
from pathlib import Path
import subprocess,os
r=Path(__file__).resolve().parents[1]
s=(r/'third_party/vlc/modules/ps5vdec.c').read_text()
vp9=s[s.index('static unsigned vp9_frames('):s.index('static bool vp9_rebuild(')]
h264=s[s.index('typedef struct {\n    uint8_t b[256]'):s.index('/* One decoded picture into a VLC picture. */')]
p=r/'test-output/vlc-decoder-headers.c';p.parent.mkdir(exist_ok=True)
p.write_text('''#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define VLOG(...) ((void)0)
typedef struct { bool sps_ok,frame_mbs_only,separate_colour_plane; unsigned log2_max_frame_num; } decoder_sys_t;
'''+vp9+h264+'''
static uint8_t bytes[256]; static unsigned bit;
static void put(unsigned v,unsigned n){for(unsigned i=n;i>0;i--){if((v>>(i-1))&1)bytes[bit/8]|=1u<<(7-bit%8);bit++;}}
static void ue(unsigned v){unsigned x=v+1,n=0;for(unsigned k=x;k;k>>=1)n++;put(0,n-1);put(x,n);}
static void sps(decoder_sys_t*s,bool progressive){
 memset(bytes,0,sizeof(bytes));bit=0;put(66,8);put(0,8);put(30,8);ue(0);ue(0);ue(2);ue(1);put(0,1);ue(119);ue(33);put(progressive,1);put(1,1);
 h264_read_sps(s,bytes,(bit+7)/8);
}
int main(void){
 size_t off[8],len[8];bool key,shows;
 assert(!vp9_frames(bytes,0,off,len));
 uint8_t frames[]={0x82,1,2,0x86,3,0xc1,3,2,0xc1};
 assert(vp9_frames(frames,sizeof(frames),off,len)==2 && off[1]==3 && len[1]==2);
 vp9_header(frames,3,&key,&shows);assert(key && shows && vp9_profile(frames,3)==0);
 vp9_header(frames+3,2,&key,&shows);assert(!key && shows);
 frames[6]=8;assert(!vp9_frames(frames,sizeof(frames),off,len));
 uint8_t invalid=0;vp9_header(&invalid,1,&key,&shows);assert(!key && !shows && vp9_profile(&invalid,1)==4);
 decoder_sys_t sys={0};sps(&sys,false);assert(sys.sps_ok && !sys.frame_mbs_only && sys.log2_max_frame_num==4);
 for(int bottom=0;bottom<2;bottom++){
  memset(bytes,0,sizeof(bytes));bit=32;bytes[2]=1;bytes[3]=0x65;ue(0);ue(2);ue(0);put(3,4);put(1,1);put(bottom,1);put(1,1);
  bool field,b;unsigned number;
  assert(h264_slice_field(&sys,bytes,(bit+7)/8,&field,&b,&number) && field && b==!!bottom && number==3);
 }
 h264_read_sps(&sys,&invalid,1);assert(sys.sps_ok && !sys.frame_mbs_only);
 sps(&sys,true);assert(sys.frame_mbs_only);
 const uint8_t source[]={11,21,12,22,0,0,13,23,14,24,0,0};uint8_t u[4],v[4];
 split_plane8(u,2,v,2,source,6,2,2);assert(u[0]==11 && u[3]==14 && v[0]==21 && v[3]==24);
 // Exercise Annex-B SPS discovery using the same generated progressive header.
 sps(&sys,false);uint8_t annex[260]={0,0,1,0x67};memcpy(annex+4,bytes,(bit+7)/8);decoder_sys_t scan={0};
 h264_scan_sps(&scan,annex,4+(bit+7)/8);assert(scan.sps_ok && !scan.frame_mbs_only);
 puts("VLC decoder headers passed: VP9 superframes, malformed bounds, keyframes, H264 interlaced/progressive SPS, top/bottom fields and planar chroma");
}
''')
exe=p.with_suffix('')
subprocess.run([os.environ.get('HOST_CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined,address',str(p),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
