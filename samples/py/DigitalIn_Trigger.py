"""
   DWF Python Example 
   Author:  Digilent, Inc.
   Revision: 11/15/2016

   Requires:                       
       Python 2.7, numpy, matplotlib
       python-dateutil, pyparsing
"""
from ctypes import *
import math
import time
import matplotlib.pyplot as plt
import sys

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

if hdwf.value == 0:
    print "failed to open device"
    quit()

print "Preparing to read sample..."

# generate counter
for i in range(0, 8):
    dwf.FDwfDigitalOutEnableSet(hdwf, c_int(i), c_int(1))
    dwf.FDwfDigitalOutDividerSet(hdwf, c_int(i), c_int(1<<i))
    dwf.FDwfDigitalOutCounterSet(hdwf, c_int(i), c_int(1), c_int(1))

dwf.FDwfDigitalOutConfigure(hdwf, c_int(1))


#sample rate = system frequency / divider, 100MHz/1
dwf.FDwfDigitalInDividerSet(hdwf, c_int(1))
# 16bit per sample format
dwf.FDwfDigitalInSampleFormatSet(hdwf, c_int(16))
# set number of sample to acquire
cSamples = 1000
rgwSamples = (c_uint16*cSamples)()
dwf.FDwfDigitalInBufferSizeSet(hdwf, c_int(cSamples))
dwf.FDwfDigitalInTriggerSourceSet(hdwf, c_ubyte(3)) # trigsrcDetectorDigitalIn
dwf.FDwfDigitalInTriggerPositionSet(hdwf, c_int(cSamples/2))
dwf.FDwfDigitalInTriggerSet(hdwf, c_int(0), c_int(0), c_int(0), c_int(1<<7)) # DIO7 falling edge

# begin acquisition
dwf.FDwfDigitalInConfigure(hdwf, c_bool(0), c_bool(1))
print "   waiting to finish"

while True:
    dwf.FDwfDigitalInStatus(hdwf, c_int(1), byref(sts))
    print "STS VAL: " + str(sts.value)
    if sts.value == 2 : # done
        break
    time.sleep(1)
print "Acquisition finished"

# get samples, byte size
dwf.FDwfDigitalInStatusData(hdwf, rgwSamples, 2*cSamples)
dwf.FDwfDeviceCloseAll()

rgpy=[0.0]*len(rgwSamples)
for i in range(0,len(rgpy)):
    rgpy[i]=rgwSamples[i]&0xFF

plt.plot(rgpy)
plt.show()


