#!/usr/bin/env python3
"""Fault-inject the actual custom PMIC, PN532 and screen-sleep functions.

Reuses the parent fork's function extractor and a native C++ compiler.
No device, account or network is accessed.
"""
from pathlib import Path
import importlib.util
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("inventory", ROOT / "scripts/test-inventory.py")
inventory = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inventory)

SHIM = r'''
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>
using std::fabs;
uint32_t clockMs=0; void delay(int n){clockMs+=n;if(clockMs>10000)throw std::runtime_error("sleep loop did not wake");}
uint32_t millis(){return clockMs;}
std::string logText;
struct {void println(const char* s){logText+=s;}
 template<typename... A>void printf(const char* f,A... a){char b[512];snprintf(b,sizeof(b),f,a...);logText+=b;}
}Serial;
uint8_t regs[256]={}; bool writeOk=true; std::vector<int> writes;
uint8_t axpRead(uint8_t r){return regs[r];}
bool axpWrite(uint8_t r,uint8_t v){writes.push_back(r);if(writeOk)regs[r]=v;return writeOk;}
const int PN532_COMMAND_POWERDOWN=0x16;
struct Reader {
 bool _pendingUid=true,_fieldOn=true,ack=true,frame=true;
 uint8_t opcode=0x17,status=0,responseLen=3;int commands=0,responses=0;
 struct MockPn {
  Reader* reader;
  bool sendCommandCheckAck(uint8_t* cmd,uint8_t len,int timeout){
   if(len!=2||cmd[0]!=0x16||cmd[1]!=0x10||timeout!=500)throw std::runtime_error("wrong sleep command");
   reader->commands++;return reader->ack;
  }
 }_pn532{this};
 bool readResponse(uint8_t* body,uint8_t,uint8_t& len){responses++;len=responseLen;body[0]=0xD5;body[1]=opcode;body[2]=status;return frame;}
__POWERDOWN__
}rfid1,rfid2;
bool gDisplayReady=true,rfidTestActive=false,gBlankFlowActive=false,gInventoryAssignmentPending=false,gBambuPending=false;
bool gRfid1Present=true,gRfid2Present=true,rfidAntennasOn=true,gScreenOff=false,gRfidCountIsAuto=true;
uint8_t hwRfidCount=2,hwRfidSide=1,gBrightness=80;
bool gHeartbeatForce=false,gHeartbeatPending=false;uint32_t gLastHeartbeatScheduleMs=0;
struct {bool vbusChanged=false;}gBattery;
struct {bool isConnected(){return true;}}WiFi;
int pollBattery=0;void pollBatteryState(){pollBattery++;}
uint32_t heartbeatIntervalMs(){return 1000;}
const int PN532_1_RST=17,PN532_2_RST=9,LOW=0,LCD_BL=6;
int resets=0;void digitalWrite(int,int){resets++;}
struct Canvas {void fillScreen(int){} void flush(){}}canvas;
Canvas* gfx=&canvas;bool gUseLvglMainScreen=true;int screen=1;int* lvScreen=&screen;
int repaints=0,brightness=0,backlight=0;
void lv_obj_invalidate(int*){repaints++;}void lv_timer_handler(){}
void applyBrightness(uint8_t n){brightness=n;}void ledcWrite(int,int n){backlight=n;}
bool weightWake=false;int touches=0,weightReads=0;
bool tsRead(int16_t&,int16_t&){touches++;return !weightWake&&touches==2;}
float readWeight(){return weightWake&&weightReads++>0?11:0;}
int setups=0;bool failFirstWake=false,failEveryWake=false;
void setupRFID(bool persist){
 if(persist)throw std::runtime_error("wake requested NVS persistence");
 if(repaints!=1||brightness!=80)throw std::runtime_error("panel not repainted before RFID setup");
 if(hwRfidCount!=2)throw std::runtime_error("second reader skipped after failed first wake");
 setups++;gRfid1Present=true;gRfid2Present=!(failEveryWake||(failFirstWake&&setups==1));
 hwRfidCount=gRfid2Present?2:1;
}
__SLEEPALL__
__SCREENSAVER__
'''

TESTS = r'''
int checks=0;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
void reset(){
 clockMs=0;logText.clear();writes.clear();memset(regs,0,sizeof(regs));writeOk=true;
 rfid1=Reader();rfid1._pn532.reader=&rfid1;rfid2=Reader();rfid2._pn532.reader=&rfid2;
 gDisplayReady=true;gfx=&canvas;rfidTestActive=gBlankFlowActive=gInventoryAssignmentPending=gBambuPending=false;
 gRfid1Present=gRfid2Present=rfidAntennasOn=gRfidCountIsAuto=true;gScreenOff=false;
 hwRfidCount=2;hwRfidSide=1;setups=resets=repaints=brightness=touches=weightReads=pollBattery=0;
 failFirstWake=failEveryWake=weightWake=false;gHeartbeatPending=gHeartbeatForce=false;
}
int main(){
 reset();regs[0x16]=0xA9;regs[0x62]=0xE5;configureBatteryCharging();
 CHECK(regs[0x16]==0xAC);CHECK(regs[0x62]==0xF0);
 CHECK(writes.size()==2&&writes[0]==0x16&&writes[1]==0x62);
 CHECK(logText.find("verified=yes")!=std::string::npos);
 for(int reg:{0x16,0x62}){reset();regs[reg]=0xFF;configureBatteryCharging();CHECK(writes.empty());}
 reset();writeOk=false;configureBatteryCharging();CHECK(logText.find("verified=NO")!=std::string::npos);
 reset();CHECK(rfid1.enterPowerDown());CHECK(!rfid1._fieldOn&&!rfid1._pendingUid);
 CHECK(rfid1.commands==1&&rfid1.responses==1&&clockMs>=2);
 reset();rfid1.ack=false;CHECK(!rfid1.enterPowerDown());CHECK(rfid1.responses==0);
 reset();rfid1.frame=false;CHECK(!rfid1.enterPowerDown());
 reset();rfid1.opcode=0x33;CHECK(!rfid1.enterPowerDown());
 reset();rfid1.status=0x27;CHECK(!rfid1.enterPowerDown());
 reset();rfid1.responseLen=2;CHECK(!rfid1.enterPowerDown());
 reset();runScreensaver();CHECK(setups==1);CHECK(rfid1.commands==1&&rfid2.commands==1);
 CHECK(!gScreenOff&&rfidAntennasOn&&gRfid1Present&&gRfid2Present);
 CHECK(hwRfidCount==2&&gHeartbeatPending&&repaints==1);
 reset();weightWake=true;runScreensaver();CHECK(setups==1&&weightReads>=2);
 reset();failFirstWake=true;runScreensaver();CHECK(setups==2&&gRfid2Present&&hwRfidCount==2);
 reset();failEveryWake=true;runScreensaver();CHECK(setups==2&&!gRfid2Present&&hwRfidCount==2);
 for(int guard=0;guard<4;guard++){
  reset();if(guard==0)rfidTestActive=true;if(guard==1)gBlankFlowActive=true;
  if(guard==2)gInventoryAssignmentPending=true;if(guard==3)gBambuPending=true;
  runScreensaver();CHECK(setups==0&&rfid1.commands==0&&rfid2.commands==0);
 }
 reset();gRfid1Present=gRfid2Present=false;runScreensaver();CHECK(setups==1&&rfid1.commands==1&&rfid2.commands==1);
 reset();gRfid1Present=gRfid2Present=false;gRfidCountIsAuto=false;hwRfidCount=0;
 runScreensaver();CHECK(setups==0&&rfid1.commands==0&&rfid2.commands==0);
 printf("Custom power: %d checks passed\n",checks);
}
'''


def main():
    cxx = os.environ.get("CXX") or shutil.which("g++")
    if not cxx:
        raise SystemExit("g++ not found; set CXX to a native C++ compiler")
    guard = []
    with inventory.SOURCE.open(encoding="utf-8") as source:
        for line in source:
            if line.startswith("#if TS_BOARD_JC3248 || !RFID_TRANSPORT_HSU"):
                guard.append(line)
            elif guard:
                guard.append(line)
                if line.startswith("#endif"):
                    break
    if not guard:
        raise SystemExit("missing board/transport compile guard")
    for nonb, jc, hsu, i2c, allowed in [(0, 0, 1, 0, True), (1, 0, 1, 0, True),
                                      (0, 1, 1, 0, False), (0, 0, 0, 0, False),
                                      (0, 0, 1, 1, False)]:
        flags = [f"-DTS_BOARD_NONB={nonb}", f"-DTS_BOARD_JC3248={jc}",
                 f"-DRFID_TRANSPORT_HSU={hsu}", f"-DRFID_TRANSPORT_I2C={i2c}"]
        check = subprocess.run([cxx, "-E", "-x", "c++", *flags, "-"],
                               input="".join(guard), text=True, capture_output=True)
        if (check.returncode == 0) != allowed:
            raise SystemExit("unexpected board/transport acceptance: " + " ".join(flags))
    print("Board restriction: 5 checks passed")
    code = SHIM.replace("__POWERDOWN__", inventory.extract("bool enterPowerDown()"))
    code = code.replace("__SLEEPALL__", inventory.extract("static void rfidPowerDownAll()"))
    code = code.replace("__SCREENSAVER__", inventory.extract("static void runScreensaver()"))
    code += inventory.extract("static void configureBatteryCharging()") + TESTS
    with tempfile.TemporaryDirectory(prefix="tigerscale-custom-power-") as folder:
        cpp, exe = Path(folder) / "power.cpp", Path(folder) / "power"
        cpp.write_text(code, encoding="utf-8")
        subprocess.run([cxx, "-std=c++11", str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
