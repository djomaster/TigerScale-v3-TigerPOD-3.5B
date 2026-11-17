#!/usr/bin/env python3
"""Validate actual fast HSU ACK framing, split replies, timeout and corruption."""
import os,subprocess,tempfile,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1]
code=r'''
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cassert>
uint32_t tick=0;uint32_t millis(){return tick;}void delay(int n){tick+=n;}
#include "pod_uart.h"
struct Port {
 std::vector<uint8_t> sent,reply={0,0,255,0,255,0};size_t at=0;bool shortWrite=false;uint32_t start=0,spacing=1;
 size_t write(const uint8_t* p,size_t n){sent.assign(p,p+n);start=tick;return shortWrite?n-1:n;}
 int available(){return at<reply.size()&&tick-start>=(at+1)*spacing;}
 int read(){return available()?reply[at++]:-1;}
};
int main(){int checks=0;
#define CHECK(x) do{assert(x);checks++;}while(0)
 uint8_t read[]={0x40,1,0x30,4};Port p;CHECK(PodUart::ack(p,read,4));CHECK(tick==6);
 const uint8_t expected[]={0,0,255,5,251,0xD4,0x40,1,0x30,4,0xB7,0};
 CHECK(p.sent.size()==sizeof(expected));CHECK(!memcmp(p.sent.data(),expected,sizeof(expected)));
 for(int i=0;i<6;i++){Port bad;bad.reply[i]^=1;CHECK(!PodUart::ack(bad,read,4));}
 for(int n=0;n<6;n++){Port shortAck;shortAck.reply.resize(n);uint32_t start=tick;CHECK(!PodUart::ack(shortAck,read,4));CHECK(tick-start==100);}
 Port slow;slow.spacing=16;CHECK(PodUart::ack(slow,read,4));Port late;late.spacing=18;CHECK(!PodUart::ack(late,read,4));
 Port failed;failed.shortWrite=true;CHECK(!PodUart::ack(failed,read,4));Port zero;CHECK(!PodUart::ack(zero,read,0));
 uint8_t large[33]={};CHECK(!PodUart::ack(zero,large,33));
 tick=0xfffffffcu;Port wrap;CHECK(PodUart::ack(wrap,read,4));CHECK(tick==2);
 printf("%d fast HSU framing/timeout checks passed. RF speed not measured.\n",checks);
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run([os.environ.get('CXX') or shutil.which('c++'),'-std=c++11','-I'+str(root/'TigerTagSplashESP32'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
