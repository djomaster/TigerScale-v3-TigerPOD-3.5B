#!/usr/bin/env python3
"""Host tests of the custom OTA gates and memory admission policy; no ESP32 build."""
import os,re,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'TigerTagSplashESP32/TigerTagSplashESP32.ino').read_text()
def function(name):
    m=re.search(r'^(?:static )?(?:bool|void|uint8_t) '+name+r'\([^;{}]*\)\s*\{',s,re.M)
    assert m,name
    # These small gate functions contain no nested braces.
    end=s.index('\n}',m.end())+2
    return s[m.start():end]
code=r'''
#include <string>
#include <functional>
#include <cassert>
#include <cstdio>
using String=std::string;
struct {void println(const char*){}} Serial;
String gOtaLatestVer="3.11",gOtaLatestSha="old",gOtaFirmwareUrl="upstream",gOtaLittlefsUrl="upstream",gOtaLittlefsSha="old";
bool gOtaPromptPending=true;
#include <cstdint>
const uint8_t R86_MEM_OK=0,R86_MEM_TOTAL=1,R86_MEM_BLOCK=2,R86_MEM_PSRAM=3,R86_MEM_PSRAM_BLOCK=4;
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_SPIRAM 2
size_t internal=65536,largest=28672,psram=1024*1024,psramLargest=512*1024;
size_t heap_caps_get_free_size(int c){return c==1?internal:psram;}
size_t heap_caps_get_largest_free_block(int c){return c==1?largest:psramLargest;}
'''
code+='\n'.join(function(n) for n in ['otaFetchLatest','otaStartJob','otaApply','r86MemoryCheck'])
code+=r'''
int main(){
 assert(!otaFetchLatest());assert(gOtaLatestVer.empty()&&gOtaLatestSha.empty()&&gOtaFirmwareUrl.empty()&&gOtaLittlefsUrl.empty()&&gOtaLittlefsSha.empty()&&!gOtaPromptPending);
 for(auto url:{"https://upstream/firmware.bin","https://custom/firmware.bin",""}){
 assert(!otaStartJob(url,"hash",url,"hash"));bool called=false;
 assert(!otaApply(url,"hash",0,[&](int,const String&){called=true;}));assert(!called);
 }
 assert(r86MemoryCheck(true,0,0)==R86_MEM_OK&&r86MemoryCheck(false,0,0)==R86_MEM_OK);
 internal--;assert(r86MemoryCheck(true,0,0)==R86_MEM_TOTAL);internal++;
 largest--;assert(r86MemoryCheck(true,0,0)==R86_MEM_BLOCK);largest++;psram--;assert(r86MemoryCheck(true,0,0)==R86_MEM_PSRAM);psram++;
 psramLargest--;assert(r86MemoryCheck(true,0,0)==R86_MEM_PSRAM_BLOCK);psramLargest++;
 internal=49151;assert(r86MemoryCheck(false,0,0)==R86_MEM_TOTAL);internal=65536;
 puts("custom OTA/memory policy checks passed.");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++11',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
# Wiring checks complement the callable policy tests above.
assert 'twinWorkerTask' not in s
assert 'const char* TIGERSCALE_VERSION_URL = "";' in s
assert 'lv_label_set_text(lvEraseCaption, "TigerPOD")' in s
assert 'rows.push_back({"TigerPOD' not in s
assert not re.search(r'^\s*[^/\n].*\bUpdate\.begin\(',s,re.M)
pod=(root/'TigerTagSplashESP32/pod_inventory.h').read_text()
assert 'lv_scr_load(lvScreen?lvScreen:previous)' in pod
assert 'while(!r7WorkerPaused || !gLiveSuspended)' in pod
print('7 custom integration checks passed.')

# The 3.5B environment inherits the loop-task stack budget through common flags.
import configparser
cfg=configparser.ConfigParser(interpolation=None);cfg.read(root/'platformio.ini')
assert '-DARDUINO_LOOP_STACK_SIZE=32768' in cfg['common']['base_flags']
assert '${common.base_flags}' in cfg['env:esp32s3_hsu_b']['build_flags']
assert 'getArduinoLoopTaskStackSize()' in s
print('3 loop-task stack configuration/diagnostic checks passed.')
