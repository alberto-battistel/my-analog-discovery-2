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
from past.utils import old_div
from ctypes import *
from dwfconstants import *
import math
import time
import matplotlib.pyplot as plt
import sys
import numpy as np
from scipy.fftpack import fft
from numpy import arange,array,ones,linalg
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
hzAcq = c_double(4e5)
nSamples = 2**22
ch0Samples = (c_int16*nSamples)()
ch1Samples = (c_int16*nSamples)()
cAvailable = c_int()
cLost = c_int()
cCorrupted = c_int()
fLost = 0
fCorrupted = 0

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
    print(szerr.value)
    print("failed to open device")
    quit()

print("Preparing to read sample...")

print("Generating sine wave...")
dwf.FDwfAnalogOutNodeEnableSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_bool(True))
dwf.FDwfAnalogOutNodeFunctionSet(hdwf, c_int(0), AnalogOutNodeCarrier, funcSine)
dwf.FDwfAnalogOutNodeFrequencySet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(old_div(1e8,2**16)))
dwf.FDwfAnalogOutNodeAmplitudeSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(2))
dwf.FDwfAnalogOutConfigure(hdwf, c_int(0), c_bool(True))

#set up acquisition
dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(0), c_bool(True))
dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(0), c_double(5))
dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(1), c_bool(True))
dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(1), c_double(5))
dwf.FDwfAnalogInAcquisitionModeSet(hdwf, acqmodeRecord)
dwf.FDwfAnalogInFrequencySet(hdwf, hzAcq)
dwf.FDwfAnalogInRecordLengthSet(hdwf, c_double(old_div(nSamples,hzAcq.value))) # -1 infinite record length

#wait at least 2 seconds for the offset to stabilize
time.sleep(2)

#begin acquisition
t0 = time.process_time()
dwf.FDwfAnalogInConfigure(hdwf, c_int(0), c_int(1))
print("   waiting to finish")

cSamples = 0
Lost = []
Corrupted = []
tstart = time.process_time()
print("setting time: ", tstart-t0)
t = []

while cSamples < nSamples:
    dwf.FDwfAnalogInStatus(hdwf, c_int(1), byref(sts))
    if cSamples == 0 and (sts == DwfStateConfig or sts == DwfStatePrefill or sts == DwfStateArmed) :
        # Acquisition not yet started.
        continue

    dwf.FDwfAnalogInStatusRecord(hdwf, byref(cAvailable), byref(cLost), byref(cCorrupted))
    
    cSamples += cLost.value

    if cLost.value :
        fLost = 1
        Lost.append(cLost.value)
    if cCorrupted.value :
        fCorrupted = 1
        Corrupted.append(cCorrupted.value)

    if cAvailable.value==0 :
        continue

    if cSamples+cAvailable.value > nSamples :
        cAvailable = c_int(nSamples-cSamples)
    
    dwf.FDwfAnalogInStatusData16(hdwf, c_int(0), byref(ch0Samples, sizeof(c_int16)*cSamples), c_int(0), cAvailable) # get channel 1 data
    dwf.FDwfAnalogInStatusData16(hdwf, c_int(1), byref(ch1Samples, sizeof(c_int16)*cSamples), c_int(1), cAvailable) # get channel 2 data
    cSamples += cAvailable.value
#    tstart = time.process_time()-tstart
    t.append(time.process_time()-tstart)

dwf.FDwfDeviceCloseAll()

print("Recording finished")
if fLost:
    print("Samples were lost! Reduce frequency")
if fCorrupted:
    print("Samples could be corrupted! Reduce frequency")


data = np.fromiter(ch0Samples, dtype = np.int16)

plt.figure(1)
plt.clf()
plt.plot(data)
plt.show()

# Plot fft
a = np.array(data)
A = fft(a)/len(a)
plt.figure(2)
plt.clf()
plt.semilogy(abs(A))

plt.figure(3)
plt.clf()
plt.plot(t)

xi = arange(0,len(t))
A = array([ xi, ones(len(t))])
w = linalg.lstsq(A.T,t)[0] # obtaining the parameters

print('Cycle time: ', w[0]*1000, 'ms')
print('Samples per cycle: ', w[0]*hzAcq.value)