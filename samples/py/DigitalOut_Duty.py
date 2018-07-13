"""
   DWF Python Example
   Author:  Digilent, Inc.
   Revision: 

   Requires:                       
       Python 2.7, numpy
       python-dateutil, pyparsing
"""
from ctypes import *
from dwfconstants import *
import math
import time
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

if hdwf.value == hdwfNone.value:
    print "failed to open device"
    quit()

iChannel = 0
hzFreq = 1000 # PWM freq Hz
prcDuty = 1.23 # duty %

hzSys = c_double()
maxDiv = c_uint()
dwf.FDwfDigitalOutInternalClockInfo(hdwf, byref(hzSys))
dwf.FDwfDigitalOutCounterInfo(hdwf, c_int(0), 0, byref(maxDiv))

# for low frequencies use divider as pre-scaler to satisfy counter limitation of 32k
cDiv = int(math.ceil(hzSys.value/hzFreq/maxDiv.value))
# count steps to generate the give frequency
cPulse = int(round(hzSys.value/hzFreq/cDiv))
# duty
cHigh = int(cPulse*prcDuty/100)
cLow = int(cPulse-cHigh)

print "Generated: "+str(hzSys.value/cPulse/cDiv)+"Hz "+str(100.0*cHigh/cPulse)+"% divider: "+str(cDiv)

dwf.FDwfDigitalOutEnableSet(hdwf, c_int(iChannel), c_int(1))
dwf.FDwfDigitalOutTypeSet(hdwf, c_int(iChannel), c_int(0)) # DwfDigitalOutTypePulse
dwf.FDwfDigitalOutDividerSet(hdwf, c_int(iChannel), c_int(cDiv)) # max 2147483649, for counter limitation or custom sample rate
dwf.FDwfDigitalOutCounterSet(hdwf, c_int(iChannel), c_int(cLow), c_int(cHigh)) # max 32768

dwf.FDwfDigitalOutConfigure(hdwf, c_int(1))

print "Generating output for 10 seconds..."
time.sleep(10)

dwf.FDwfDigitalOutReset(hdwf);

dwf.FDwfDeviceCloseAll()
