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
from builtins import range
from past.utils import old_div
from ctypes import *
from dwfconstants import *
import math
import time
import matplotlib.pyplot as plt
import sys

from scipy.fftpack import fft
import numpy as np
#from Switch import *

if sys.platform.startswith("win"):
    dwf = cdll.dwf
elif sys.platform.startswith("darwin"):
    dwf = cdll.LoadLibrary("/Library/Frameworks/dwf.framework/dwf")
else:
    dwf = cdll.LoadLibrary("libdwf.so")

#declare ctype variables
hdwf = c_int()
sts = c_byte()
hzAcq = c_double(1000000)
nSamples = 1000000
rgdSamples = (c_double*nSamples)()
rgdSamples16 = (c_short*nSamples)()

pidxWrite = c_int()
acqMode = acqmodeScanScreen
filterMode = filterAverage

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

print("Generating sine wave...")
dwf.FDwfAnalogOutNodeEnableSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_bool(True))
dwf.FDwfAnalogOutNodeFunctionSet(hdwf, c_int(0), AnalogOutNodeCarrier, funcSine)
dwf.FDwfAnalogOutNodeFrequencySet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(500000))
dwf.FDwfAnalogOutNodeAmplitudeSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(2))
dwf.FDwfAnalogOutConfigure(hdwf, c_int(0), c_bool(True))

#set up acquisition
dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(0), c_bool(True))
dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(0), c_double(5))
dwf.FDwfAnalogInAcquisitionModeSet(hdwf, acqMode)
dwf.FDwfAnalogInFrequencySet(hdwf, hzAcq)
dwf.FDwfAnalogInChannelFilterSet(hdwf, c_int(0), filterMode)

dwf.FDwfAnalogInRecordLengthSet(hdwf, c_double(old_div(nSamples,hzAcq.value))) # -1 infinite record length

bufferLength = 8192
dwf.FDwfAnalogInBufferSizeSet(hdwf, c_int(bufferLength))
sts = c_int()
dwf.FDwfAnalogInBufferSizeGet(hdwf, byref(sts))
print("Buffer length ", sts.value)
bufferLength = sts.value

#wait at least 2 seconds for the offset to stabilize
time.sleep(2)

#begin acquisition
dwf.FDwfAnalogInConfigure(hdwf, c_int(0), c_int(1))
print("   waiting to finish")
#time.sleep(0.1)

cSamples = 0
sts = c_ubyte()
cSamplesValid = c_int()
idx = 1
pidxWrite = c_int()
iterations = 1;

while cSamples < nSamples:
    # Count iterations
    iterations += 1
    dwf.FDwfAnalogInStatus(hdwf, c_int(1), byref(sts))
    # print(sts.value)
    if cSamples == 0 and (sts == DwfStateConfig or sts == DwfStatePrefill or sts == DwfStateArmed) :
        # Acquisition not yet started.
        continue
    
    # Read index
    dwf.FDwfAnalogInStatusIndexWrite(hdwf, byref(pidxWrite))  
    # Calculate how many new samples are in the buffer    
    availableSamples = (pidxWrite.value - idx)%(bufferLength)
    
    if cSamples + availableSamples > nSamples :
        availableSamples = nSamples-cSamples
    
    if idx + availableSamples <= bufferLength:
        dwf.FDwfAnalogInStatusData2(hdwf, c_int(0), byref(rgdSamples, sizeof(c_double)*cSamples), c_int(idx), c_int(availableSamples))
        cSamples += availableSamples
        #print(iterations, "idx: ", idx, ", pidxWrite: ", pidxWrite.value, ", Valid samples: ", availableSamples, ", cSamples: ", cSamples)
        idx = pidxWrite.value
    else:
        #print("wrapping")
        #print(iterations, "idx: ", idx, ", pidxWrite: ", pidxWrite.value, ", Valid samples: ", availableSamples)
        availableSamples1 = bufferLength-idx
        dwf.FDwfAnalogInStatusData2(hdwf, c_int(0), byref(rgdSamples, sizeof(c_double)*cSamples), c_int(idx), c_int(availableSamples1))
        cSamples += availableSamples1
        #print(iterations, "idx: ", idx, ", pidxWrite: ", pidxWrite.value, ", Valid samples: ", availableSamples1, ", cSamples: ", cSamples)
        idx = 0
        availableSamples2 = availableSamples-availableSamples1+1
        dwf.FDwfAnalogInStatusData2(hdwf, c_int(0), byref(rgdSamples, sizeof(c_double)*cSamples), c_int(idx), c_int(availableSamples2))
        cSamples += availableSamples2
        #print(iterations, "idx: ", idx, ", pidxWrite: ", pidxWrite.value, ", Valid samples: ", availableSamples2, ", cSamples: ", cSamples)
        idx = pidxWrite.value

#print(iterations, "idx: ", idx, ", pidxWrite: ", pidxWrite.value, ", Valid samples: ", availableSamples, ", cSamples: ", cSamples)
print("Recording finished")


dwf.FDwfDeviceCloseAll()

  
rgpy=[0.0]*len(rgdSamples)
for i in range(0,len(rgpy)):
    rgpy[i]=rgdSamples[i]

plt.plot(rgpy, '.')
plt.show()


a = np.array(rgpy)
A = fft(a)/len(a)
plt.figure(2)
plt.semilogy(abs(A))
