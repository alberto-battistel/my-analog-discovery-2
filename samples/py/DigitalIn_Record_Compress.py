"""
   DWF Python Example
   Author:  Digilent, Inc.
   Revision: 11/24/2014

   Requires:                       
       Python 2.7, numpy,
"""
from ctypes import *
from dwfconstants import *
import math
import sys
import time

if sys.platform.startswith("win"):
    dwf = cdll.dwf
elif sys.platform.startswith("darwin"):
    dwf = cdll.LoadLibrary("/Library/Frameworks/dwf.framework/dwf")
else:
    dwf = cdll.LoadLibrary("libdwf.so")

#declare ctype variables
hdwf = c_int()
sts = c_byte()

#print DWF version
version = create_string_buffer(16)
dwf.FDwfGetVersion(version)
print "DWF Version: "+version.value

#open device
print "Opening first device"
dwf.FDwfDeviceOpen(c_int(-1), byref(hdwf))

if hdwf.value == hdwfNone.value:
    print "failed to open device"
    quit()

print "Configuring Digital Out / In..."

# generate counter
for i in range(0, 16):
    dwf.FDwfDigitalOutEnableSet(hdwf, c_int(i), c_int(1))
    dwf.FDwfDigitalOutDividerSet(hdwf, c_int(i), c_int(1<<i))
    dwf.FDwfDigitalOutCounterSet(hdwf, c_int(i), c_int(1000), c_int(1000))

dwf.FDwfDigitalOutConfigure(hdwf, c_int(1))

# set number of sample to acquire
nSamples = 100000
rgwSamples = (c_uint16*nSamples)()
cAvailable = c_int()
cLost = c_int()
cCorrupted = c_int()
iSample = 0
fLost = 0
fCorrupted = 0

# in record mode samples after trigger are acquired only
dwf.FDwfDigitalInAcquisitionModeSet(hdwf, acqmodeRecord)
# sample rate = system frequency / divider, 100MHz/10 = 10MHz
dwf.FDwfDigitalInDividerSet(hdwf, c_int(10))
# 16bit per sample format
dwf.FDwfDigitalInSampleFormatSet(hdwf, c_int(16))
# number of samples after trigger
dwf.FDwfDigitalInTriggerPositionSet(hdwf, c_int(nSamples/2)) # 50%
# number of samples before trigger
dwf.FDwfDigitalInTriggerPrefillSet(hdwf, c_int(nSamples/2))
# enable data compression by select used lines
dwf.FDwfDigitalInSampleSensibleSet(hdwf, c_int(0xFFFF))
# trigger when all digital pins are low
dwf.FDwfDigitalInTriggerSourceSet(hdwf, trigsrcDetectorDigitalIn)
# trigger detector mask:                  low &   hight & ( rising | falling )
dwf.FDwfDigitalInTriggerSet(hdwf, c_uint(0xFFFF), c_uint(0), c_uint(0), c_uint(0))

# begin acquisition
dwf.FDwfDigitalInConfigure(hdwf, c_bool(1), c_bool(1))

print "Starting record"

while True:
    dwf.FDwfDigitalInStatus(hdwf, c_int(1), byref(sts))
    dwf.FDwfDigitalInStatusRecord(hdwf, byref(cAvailable), byref(cLost), byref(cCorrupted))
    
    iSample += cLost.value
    iSample %= nSamples
    
    if cLost.value :
        fLost = 1
    if cCorrupted.value :
        fCorrupted = 1

    iBuffer = 0
    while cAvailable.value>0:
        cSamples = cAvailable.value
        # we are using circular sample buffer, make sure to not overflow
        if iSample+cAvailable.value > nSamples:
            cSamples = nSamples-iSample
        dwf.FDwfDigitalInStatusData2(hdwf, byref(rgwSamples, 2*iSample), c_int(iBuffer), c_int(2*cSamples))
        iBuffer += cSamples
        cAvailable.value -= cSamples
        iSample += cSamples
        iSample %= nSamples

    if sts.value == DwfStateDone.value :
        break

dwf.FDwfDeviceClose(hdwf)

print "Recording finished"
if fLost:
    print "Samples were lost! Reduce sample rate"
if cCorrupted:
    print "Samples could be corrupted! Reduce sample rate"

f = open("record.csv", "w")
for i in range(0, nSamples):
    iAlign = (i+iSample)%nSamples # first sample in our buffer is at iSample index
    f.write("%s\n" % rgwSamples[iAlign])
f.close()




