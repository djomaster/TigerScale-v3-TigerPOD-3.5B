#!/usr/bin/env python3
"""Exercise the actual HSU reader methods against complete simulated UART frames.

The parent fork's extractor selects only the functions under test. No hardware,
network or account is accessed. This does not measure RF performance.
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
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include <type_traits>
class String: public std::string {
public:
 String(const char* s):std::string(s){} String(const std::string& s):std::string(s){}
 template<typename T,typename std::enable_if<std::is_integral<T>::value,int>::type=0>
 String(T n):std::string(std::to_string(n)){}
};
std::vector<std::string> logs;void netLog(const String& s){logs.push_back(s);}
struct {template<typename... A>void printf(const char*,A...) {}}Serial;
uint32_t nowMs=1;uint32_t millis(){return nowMs;}void delay(uint32_t n){nowMs+=n;}
const int OUTPUT=1,LOW=0,HIGH=1;int resetLows=0;
void pinMode(uint8_t,int){}void digitalWrite(uint8_t pin,int value){if(pin==17&&value==LOW)resetLows++;}
const uint8_t PN532_COMMAND_RFCONFIGURATION=0x32,PN532_COMMAND_INLISTPASSIVETARGET=0x4A;
const uint8_t PN532_MIFARE_ISO14443A=0,PN532_COMMAND_POWERDOWN=0x16;
struct HardwareSerial {
 std::deque<uint8_t> rx;uint32_t timeout=1000;int begins=0;
 void begin(int,int,int8_t,int8_t){begins++;}
 void setTimeout(uint32_t n){timeout=n;}uint32_t getTimeout(){return timeout;}
 int available(){return rx.size();}int read(){int b=rx.front();rx.pop_front();return b;}
 size_t readBytes(uint8_t* out,size_t n){size_t got=0;while(got<n&&!rx.empty())out[got++]=read();return got;}
 void frame(std::vector<uint8_t> body,bool corrupt=false){
  uint8_t n=body.size(),sum=0;rx.insert(rx.end(),{0,0,0xFF,n,(uint8_t)-n});
  for(auto b:body){rx.push_back(b);sum+=b;}rx.push_back((uint8_t)(-sum+(corrupt?1:0)));rx.push_back(0);
 }
};
const int SERIAL_8N1=0;
struct Adafruit_PN532 {
 HardwareSerial* serial;int begins=0,wakes=0,versions=0,sams=0,versionFails=0;
 bool samOk=true,fieldAck=true,readAck=true,sleepAck=true,corruptFrame=false;
 int powerFailures=0;uint8_t fieldResponse=0x33,sleepStatus=0,lastRetry=0;
 uint16_t readTimeout=0;std::vector<std::vector<uint8_t>> sent;
 std::vector<uint8_t> tag={0xD5,0x4B,1,1,0,4,0,7,4,1,2,3,4,5,6};
 explicit Adafruit_PN532(HardwareSerial* p):serial(p){}
 void begin(){begins++;}void wakeup(){wakes++;}
 uint32_t getFirmwareVersion(){versions++;if(versionFails>0){versionFails--;return 0;}return 0x32010607;}
 bool SAMConfig(){sams++;return samOk;}
 bool sendCommandCheckAck(uint8_t* cmd,uint8_t n,uint16_t timeout){
  sent.emplace_back(cmd,cmd+n);
  if(cmd[0]==0x32&&cmd[1]==0x0A){if(powerFailures>0){powerFailures--;return false;}serial->frame({0xD5,0x33});return true;}
  if(cmd[0]==0x32&&cmd[1]==0x05){lastRetry=cmd[4];serial->frame({0xD5,0x33});return true;}
  if(cmd[0]==0x32&&cmd[1]==0x01){if(!fieldAck)return false;serial->frame({0xD5,fieldResponse});return true;}
  if(cmd[0]==0x4A){readTimeout=timeout;if(!readAck)return false;serial->frame(tag,corruptFrame);return true;}
  if(cmd[0]==0x16){if(!sleepAck)return false;serial->frame({0xD5,0x17,sleepStatus});return true;}
  return false;
 }
 bool setPassiveActivationRetries(uint8_t n){uint8_t cmd[]={0x32,0x05,0xFF,0x01,n};return sendCommandCheckAck(cmd,sizeof(cmd),500);}
};
'''

READER = r'''
struct PN532Reader {
 struct {uint8_t size=0;uint8_t uidByte[255]={};}uid;
 uint16_t atqa=0;uint8_t sak=0;
 Adafruit_PN532 _pn532;HardwareSerial& _serial;int8_t _rxPin=47,_txPin=48;
 bool _begun=false,_pendingUid=false,_fieldOn=true;
 explicit PN532Reader(HardwareSerial& serial):_pn532(&serial),_serial(serial){}
__METHODS__
};
'''

TESTS = r'''
int checks=0;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 CHECK(PN532_RF_LEVEL_COUNT==16&&gRfidPowerLevel==7);
 for(uint8_t n=0;n<5;n++)CHECK(migratedRfidPower(n,true)==n);
 CHECK(migratedRfidPower(5,true)==15&&migratedRfidPower(5,false)==5);
 CHECK(migratedRfidPower(255,false)==7);
 CHECK(PN532_RF_LEVELS[15].gsNOn==0xF4&&PN532_RF_LEVELS[15].cwGsP==0x3F);
 CHECK(rfidAutoScoreBetter(8,16,7,100));CHECK(!rfidAutoScoreBetter(7,100,8,16));
 CHECK(rfidAutoScoreBetter(8,20,8,16));CHECK(!rfidAutoScoreBetter(8,16,8,20));
 // UART resync waits for quiet and removes stale bytes without modifying pins.
 HardwareSerial serial;serial.rx={0,0,0xFF,0xD5};uint32_t start=millis();resyncPn532Uart(serial);
 CHECK(serial.rx.empty()&&millis()-start>=30);
 PN532Reader reader(serial);reader._pn532.versionFails=3;resetLows=0;
 CHECK(!initPN532Reader(reader,42,17,"RIGHT"));
 CHECK(reader._pn532.versions==3&&reader._pn532.begins==1&&reader._pn532.wakes==2);
 CHECK(reader._pn532.sams==3&&reader._pn532.lastRetry==2&&resetLows==1);
 CHECK(!reader._fieldOn&&serial.getTimeout()==100);
 CHECK(reader.init());CHECK(reader._pn532.begins==1); // no new hard reset on recovery
 // A recovered raw 7-byte and 4-byte UID preserve ATQA/SAK for the fork's import.
 CHECK(reader.isNewCardPresent());CHECK(reader.uid.size==7&&reader.uid.uidByte[6]==6);
 CHECK(reader.atqa==4&&reader.sak==0&&reader._pendingUid);
 CHECK(reader._pn532.readTimeout==100&&serial.rx.empty());
 reader._pn532.tag={0xD5,0x4B,1,1,0,4,8,4,0x11,0x22,0x33,0x44};
 CHECK(reader.isNewCardPresent());CHECK(reader.uid.size==4&&reader.sak==8&&reader.uid.uidByte[3]==0x44);
 reader._pn532.corruptFrame=true;CHECK(!reader.isNewCardPresent()&&!reader._pendingUid&&serial.rx.empty());
 reader._pn532.corruptFrame=false;reader._pn532.tag={0xD5,0x4B,0};CHECK(!reader.isNewCardPresent());
 reader._pn532.tag={0xD5,0x4B,1,1,0,4,8,250};CHECK(!reader.isNewCardPresent());
 reader._pn532.readAck=false;CHECK(!reader.isNewCardPresent()&&serial.rx.empty());
 // RF-off commands must be real and state changes require a valid response.
 CHECK(reader.configureRfField(true)&&reader._fieldOn);
 reader._pn532.fieldResponse=0x4B;CHECK(!reader.configureRfField(false)&&reader._fieldOn);
 reader._pn532.fieldResponse=0x33;CHECK(reader.configureRfField(false)&&!reader._fieldOn);
 CHECK(reader._pn532.sent.back()==std::vector<uint8_t>({0x32,0x01,0}));
 reader._pn532.fieldAck=false;CHECK(!reader.configureRfField(true)&&!reader._fieldOn);
 reader._pn532.fieldAck=true;
 // Analog configuration retries a failed exchange and preserves read timeout.
 reader._pn532.powerFailures=1;CHECK(reader.applyRfPower(7,"RIGHT"));CHECK(serial.timeout==100&&serial.rx.empty());
 const auto& cfg=reader._pn532.sent.back();CHECK(cfg.size()==13&&cfg[1]==0x0A&&cfg[3]==0x70);
 reader._pn532.powerFailures=2;CHECK(!reader.applyRfPower(7,"RIGHT"));CHECK(serial.timeout==100);
 CHECK(reader.enterPowerDown()&&!reader._pendingUid&&!reader._fieldOn&&serial.rx.empty());
 CHECK(reader._pn532.sent.back()==std::vector<uint8_t>({0x16,0x10}));
 reader._pn532.sleepStatus=0x27;CHECK(!reader.enterPowerDown());
 reader._pn532.sleepAck=false;CHECK(!reader.enterPowerDown());
 printf("RFID 3.13 port: %d protocol, recovery and profile checks passed\n",checks);
}
'''


def main():
    cxx = os.environ.get("CXX") or shutil.which("g++")
    if not cxx:
        raise SystemExit("g++ not found")
    profile, collecting = [], False
    constants = []
    with inventory.SOURCE.open(encoding="utf-8") as source:
        for line in source:
            if line.startswith("struct PN532RfLevel"):
                collecting = True
            if collecting:
                profile.append(line)
                if line.startswith("static const uint8_t PN532_RF_LEVEL_COUNT"):
                    collecting = False
            if line.startswith(("uint8_t gRfidPowerLevel", "static const uint16_t PN532_READ_TIMEOUT_MS", "static const uint8_t PN532_ACTIVATION_RETRIES")):
                constants.append(line)
    methods = "\n".join(inventory.extract(name) for name in [
        "bool init()", "bool isNewCardPresent()", "bool configureRfField(",
        "bool applyRfPower(", "bool enterPowerDown()", "bool readResponse("])
    helpers = "\n".join(inventory.extract(name) for name in [
        "static uint8_t migratedRfidPower(", "static bool rfidAutoScoreBetter(",
        "static void resyncPn532Uart(", "static bool applyPN532RfTuning("])
    code = SHIM + "".join(profile + constants) + helpers + READER.replace("__METHODS__", methods)
    code += inventory.extract("static bool initPN532Reader(") + TESTS
    with tempfile.TemporaryDirectory(prefix="tigerscale-rfid313-") as folder:
        cpp, exe = Path(folder) / "rfid.cpp", Path(folder) / "rfid"
        cpp.write_text(code, encoding="utf-8")
        subprocess.run([cxx, "-std=c++11", str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
