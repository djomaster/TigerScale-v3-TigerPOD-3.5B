#!/usr/bin/env python3
"""Exercise PlatformIO's actual .ino conversion, without compiling the firmware.
Run with PlatformIO's Python; set INO_PREPROCESSOR to the installed GCC executable.
An optional sketch path permits checking an older source as a negative control.
"""
import os,re,subprocess,sys,tempfile
from pathlib import Path
from platformio.builder.tools.pioino import InoToCPPConverter
root=Path(__file__).resolve().parents[1]
source=Path(sys.argv[1]) if len(sys.argv)>1 else root/'TigerTagSplashESP32/TigerTagSplashESP32.ino'
gcc=os.environ.get('INO_PREPROCESSOR')
if not gcc:
    candidates=list((Path.home()/'.platformio/packages').glob('toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-g++*'))
    if not candidates:raise SystemExit('Set INO_PREPROCESSOR to the installed GCC executable.')
    gcc=str(candidates[0])
with tempfile.TemporaryDirectory() as td:
    p=Path(td);raw=p/'input.cpp';stripped=p/'stripped.cpp'
    raw.write_text('#include <Arduino.h>\n# 1 "'+str(source)+'"\n'+source.read_text())
    # Same comment-removal pass as pioino._gcc_preprocess: no includes expanded,
    # no object generated, no firmware build, no network and no project cache edits.
    result=subprocess.run([gcc,'-o',str(stripped),'-x','c++','-fpreprocessed','-dD','-E',str(raw)],capture_output=True,text=True)
    if result.returncode:raise RuntimeError(result.stderr)
    c=InoToCPPConverter(None);c._main_ino=str(source)
    generated=c.append_prototypes(c._join_multiline_strings(stripped.read_text()))
    checks=0
    for name in ['InventorySpool','SpoolContainer','BambuTag','InventoryRequest','InventoryPicker']:
        definition=generated.index('struct '+name+' {')
        declarations=re.findall(r'^.*\b'+name+r'\b.*;\s*$',generated[:definition],re.M)
        assert not declarations,(name,'referenced before its definition',declarations)
        checks+=1
    for name in ['decodeBambuTag','bambuSameProduct','inventoryColor','inventoryColorLess']:
        proto=re.search(r'^static [^\n]*\b'+name+r'\([^\n]*\);',generated,re.M)
        assert proto,('prototype missing',name)
        for typename in ['BambuTag','InventorySpool']:
            if typename in proto.group():assert generated.index('struct '+typename+' {')<proto.start()
        checks+=1
    print(str(checks)+' actual PlatformIO prototype-order checks passed (preprocessing only).')
