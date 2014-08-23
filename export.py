#!/usr/bin/python

import shutil
import os

inputDir="./"
prefix="Z:\Software"
lib=os.path.join(prefix,"lib")
include=os.path.join(prefix,"include")
basename="termlib"

for folder in ("MSP430 Release","MSP430 Debug","MSP430 Release Compact","MSP430 Debug Compact"):
    outname=basename+"_"+"_".join(folder.split()[1:])+".hza"
    outpath=os.path.join(lib,outname)
    inpath=os.path.join(inputDir,os.path.join(folder,basename+".hza"))
    print("Copying "+inpath+" to "+outpath)
    shutil.copyfile(inpath,outpath)

for file in ("terminal.h",):
    outpath=os.path.join(include,file)
    inpath=os.path.join(inputDir,file)
    print("Copying "+inpath+" to "+outpath)
    shutil.copyfile(inpath,outpath)

