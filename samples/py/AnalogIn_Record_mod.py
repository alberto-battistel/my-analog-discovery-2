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
hzAcq = c_double(5000)
nSamples = 15000
rgdSamples = (c_double*nSamples)()
cAvailable = c_int()
cLost = c_int()
cCorrupted = c_int()
fLost = 0
fCorrupted = 0
acqMode = acqmodeRecord


pidxWrite = c_int()
acqMode = acqmodeScanShift


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
dwf.FDwfAnalogOutNodeFrequencySet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(3))
dwf.FDwfAnalogOutNodeAmplitudeSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(2))
dwf.FDwfAnalogOutConfigure(hdwf, c_int(0), c_bool(True))

#set up acquisition
dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(0), c_bool(True))
dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(0), c_double(5))
dwf.FDwfAnalogInAcquisitionModeSet(hdwf, acqMode)
dwf.FDwfAnalogInFrequencySet(hdwf, hzAcq)
dwf.FDwfAnalogInRecordLengthSet(hdwf, c_double(old_div(nSamples,hzAcq.value))) # -1 infinite record length

sts = c_int()
dwf.FDwfAnalogInBufferSizeGet(hdwf, byref(sts))
print("Buffer length ", sts.value)
bufferLength = sts.value

#wait at least 2 seconds for the offset to stabilize
time.sleep(2)

#begin acquisition
dwf.FDwfAnalogInConfigure(hdwf, c_int(0), c_int(1))
print("   waiting to finish")
time.sleep(0.5)

cSamples = 0
sts = c_ubyte()
cSamplesValid = c_int()
idx = 0
pidxWrite = c_int()



while cSamples < nSamples:
    dwf.FDwfAnalogInStatus(hdwf, c_int(1), byref(sts))
    # print(sts.value)
    if cSamples == 0 and (sts == DwfStateConfig or sts == DwfStatePrefill or sts == DwfStateArmed) :
        # Acquisition not yet started.
        continue
    dwf.FDwfAnalogInStatusIndexWrite(hdwf, byref(pidxWrite))  
    
#    if cSamplesValid.value == 0:
#        continue
    
    cSamplesValid = c_int((pidxWrite.value - idx-1)%(bufferLength-1))
    if cSamples+cSamplesValid.value > nSamples :
        cSamplesValid = c_int(nSamples-cSamples)
        
    dwf.FDwfAnalogInStatusData2(hdwf, c_int(0), byref(rgdSamples, sizeof(c_double)*cSamples), c_int(idx), cSamplesValid)
    idx = (idx + cSamplesValid.value)%(bufferLength-1)
    #dwf.FDwfAnalogInStatusData(hdwf, c_int(0), byref(rgdSamples, sizeof(c_double)*cSamples), cSamplesValid)
    cSamples += cSamplesValid.value 
    
    print("pidxWrite: ", pidxWrite.value, ", Valid samples: ", cSamplesValid.value, ", cSamples: ", cSamples)
#    cSamples += cLost.value
#
#    if cLost.value :
#        fLost = 1
#    if cCorrupted.value :
#        fCorrupted = 1
#
#    if cAvailable.value==0 :
#        continue
#
#    if cSamples+cAvailable.value > nSamples :
#        cAvailable = c_int(nSamples-cSamples)
#    
#    dwf.FDwfAnalogInStatusData(hdwf, c_int(0), byref(rgdSamples, sizeof(c_double)*cSamples), cAvailable) # get channel 1 data
#    #dwf.FDwfAnalogInStatusData(hdwf, c_int(1), byref(rgdSamples, sizeof(c_double)*cSamples), cAvailable) # get channel 2 data
#    cSamples += cAvailable.value

print("idx: ", idx, ", Valid samples: ", cSamplesValid.value, ", cSamples: ", cSamples)
print("Recording finished")
if fLost:
    print("Samples were lost! Reduce frequency")
if fCorrupted:
    print("Samples could be corrupted! Reduce frequency")

dwf.FDwfDeviceCloseAll()

#f = open("record.csv", "w")
#for v in rgdSamples:
#    f.write("%s\n" % v)
#f.close()
  
rgpy=[0.0]*len(rgdSamples)
for i in range(0,len(rgpy)):
    rgpy[i]=rgdSamples[i]

plt.plot(rgpy)
plt.show()


#a = np.array(rgpy)
#A = fft(a)/len(a)
#plt.figure(2)
#plt.semilogy(abs(A))
