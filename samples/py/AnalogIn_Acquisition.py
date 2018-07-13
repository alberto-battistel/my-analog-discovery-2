"""
   DWF Python Example
   Author:  Digilent, Inc.
   Revision: 10/17/2013

   Requires:                       
       Python 2.7, numpy, matplotlib
       python-dateutil, pyparsing
"""
from __future__ import division
from __future__ import print_function
from builtins import str
from past.utils import old_div
from ctypes import *
from dwfconstants import *
import math
import time
import matplotlib.pyplot as plt
import sys
import numpy as np

if sys.platform.startswith("win"):
    dwf = cdll.dwf
elif sys.platform.startswith("darwin"):
    dwf = cdll.LoadLibrary("/Library/Frameworks/dwf.framework/dwf")
else:
    dwf = cdll.LoadLibrary("libdwf.so")

#declare ctype variables
hdwf = c_int()
sts = c_byte()
rgdSamples = (c_double*4000)()

#print DWF version
version = create_string_buffer(16)
dwf.FDwfGetVersion(version)
print("DWF Version: "+version.value.decode("utf-8"))

#open device
print("Opening first device")
dwf.FDwfDeviceOpen(c_int(-1), byref(hdwf))

if hdwf.value == hdwfNone.value:
    szerr = create_string_buffer(512)
    dwf.FDwfGetLastErrorMsg(szerr)
    print(szerr.value.decode("utf-8"))
    print("failed to open device")
    quit()

print("Preparing to read sample...")

#set up acquisition
dwf.FDwfAnalogInFrequencySet(hdwf, c_double(20000000.0))
dwf.FDwfAnalogInBufferSizeSet(hdwf, c_int(4000)) 
dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(0), c_bool(True))
dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(0), c_double(5))

#wait at least 2 seconds for the offset to stabilize
time.sleep(2)

#begin acquisition
dwf.FDwfAnalogInConfigure(hdwf, c_bool(False), c_bool(True))
print("   waiting to finish")

while True:
    dwf.FDwfAnalogInStatus(hdwf, c_int(1), byref(sts))
    print("STS VAL: " + str(sts.value) + "STS DONE: " + str(DwfStateDone.value))
    if sts.value == DwfStateDone.value :
        break
    time.sleep(0.1)
print("Acquisition finished")

dwf.FDwfAnalogInStatusData(hdwf, 0, rgdSamples, 4000) # get channel 1 data
#dwf.FDwfAnalogInStatusData(hdwf, 1, rgdSamples, 4000) # get channel 2 data
dwf.FDwfDeviceCloseAll()

#plot window
dc = old_div(sum(rgdSamples),len(rgdSamples))
print("DC: "+str(dc)+"V")

plt.plot(np.fromiter(rgdSamples, dtype = np.float))
plt.show()


