#!/usr/bin/env python3
"""Host tests of the OTA install gates and memory admission policy; no ESP32 build."""
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
struct String:std::string{String(const char*s=""):std::string(s){}String(const std::string&s):std::string(s){}
 size_t length()const{return size();}bool startsWith(const char*p)const{return rfind(p,0)==0;}};
#define TIGERSCALE_OTA_ASSET_BASE "https://github.com/djomaster/TigerScale-v3-TigerPOD-3.5B/releases/download/"
#include <cstdint>
const uint8_t R86_MEM_OK=0,R86_MEM_TOTAL=1,R86_MEM_BLOCK=2,R86_MEM_PSRAM=3,R86_MEM_PSRAM_BLOCK=4;
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_SPIRAM 2
size_t internal=65536,largest=28672,psram=1024*1024,psramLargest=512*1024;
size_t heap_caps_get_free_size(int c){return c==1?internal:psram;}
size_t heap_caps_get_largest_free_block(int c){return c==1?largest:psramLargest;}
'''
code+='\n'.join(function(n) for n in ['otaUrlAllowed','r86MemoryCheck'])
code+=r'''
int main(){
 const std::string base=TIGERSCALE_OTA_ASSET_BASE;
 assert(otaUrlAllowed(""));
 assert(otaUrlAllowed(base+"v3.13.0-pod.1.1.0/firmware-esp32s3_hsu_b.bin"));
 for(auto url:{"https://tigertag-project.github.io/Tiger-Scale-V3/firmware.bin",
   "https://github.com/TigerTag-Project/Tiger-Scale-V3/releases/download/v3.13.0/firmware.bin",
   "https://github.com/djomaster/TigerScale-v3-TigerPOD-3.5B.evil/releases/download/v1/x.bin",
   "http://github.com/djomaster/TigerScale-v3-TigerPOD-3.5B/releases/download/v1/x.bin",
   "https://example.com/firmware.bin"}) assert(!otaUrlAllowed(url));
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
assert 'const char* TIGERSCALE_VERSION_URL = "https://djomaster.github.io/TigerScale-v3-TigerPOD-3.5B/version.json";' in s
assert '#define TIGERSCALE_OTA_AUTOCHECK 0' in s
assert 'OTA from the cloud is disabled' in s
assert 'OTA CUSTOM' not in s
assert 'lv_label_set_text(lvEraseCaption, "TigerPOD")' in s
assert 'rows.push_back({"TigerPOD' not in s
assert len(re.findall(r'^\s*[^/\n].*\bUpdate\.begin\(',s,re.M))==1
i=s.rindex('static bool otaApply(const String& url,')
body=s[i:s.index('Update.begin(',i)]
assert 'otaUrlAllowed(url)' in body and 'expectedSha.length() != 64' in body
j=s.rindex('static bool otaStartJob(const String& fwUrl')
j=s.index('{',s.index('const String& fsSha)',j))
assert 'otaUrlAllowed(fwUrl)' in s[j:j+900] and 'SHA-256 is required' in s[j:j+900]
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
