"""
This is a merged file created from 'AnalogIn_Record.py' and 'AnalogIO_AnalogDiscovery2_Power.py'.
This file works as intended.
"""
from __future__ import division
from __future__ import print_function
from builtins import range

import numpy
from past.utils import old_div
from ctypes import *
from scipy.fft import fft
from dwfconstants import *
import math
import time
import matplotlib.pyplot as plt
import sys
# Recently Added Packages
import scipy
import numpy as np

if sys.platform.startswith("win"):
    dwf = cdll.dwf
elif sys.platform.startswith("darwin"):
    dwf = cdll.LoadLibrary("/Library/Frameworks/dwf.framework/dwf")
else:
    dwf = cdll.LoadLibrary("libdwf.so")

# declare ctype variables
hdwf = c_int()
sts = c_byte()
hzAcq = c_double(500000)
nSamples = int(131.072e3)
rgdSamples = (c_double * nSamples)()
cAvailable = c_int()
cLost = c_int()
cCorrupted = c_int()
fLost = 0
fCorrupted = 0
IsEnabled = c_bool()
usbVoltage = c_double()
usbCurrent = c_double()
auxVoltage = c_double()
auxCurrent = c_double()

# print DWF version
version = create_string_buffer(16)
dwf.FDwfGetVersion(version)
print("DWF Version: " + version.value.decode("utf-8"))

# open device
print("Opening first device")
dwf.FDwfDeviceOpen(c_int(-1), byref(hdwf))

if hdwf.value == hdwfNone.value:
    szerr = create_string_buffer(512)
    dwf.FDwfGetLastErrorMsg(szerr)
    print(szerr.value)
    print("failed to open device")
    quit()

# set up analog IO channel nodes
# enable positive supply
dwf.FDwfAnalogIOChannelNodeSet(hdwf, c_int(0), c_int(0), c_double(True))
# set voltage to 5 V
dwf.FDwfAnalogIOChannelNodeSet(hdwf, c_int(0), c_int(1), c_double(5.0))
# enable negative supply
dwf.FDwfAnalogIOChannelNodeSet(hdwf, c_int(1), c_int(0), c_double(True))
# set voltage to -5 V
dwf.FDwfAnalogIOChannelNodeSet(hdwf, c_int(1), c_int(1), c_double(-5.0))
# master enable
dwf.FDwfAnalogIOEnableSet(hdwf, c_int(True))

for i in range(1, 2):
    # wait 1 second between readings
    time.sleep(1)
    # fetch analogIO status from device
    if dwf.FDwfAnalogIOStatus(hdwf) == 0:
        break

    # supply monitor
    dwf.FDwfAnalogIOChannelNodeStatus(hdwf, c_int(2), c_int(0), byref(usbVoltage))
    dwf.FDwfAnalogIOChannelNodeStatus(hdwf, c_int(2), c_int(1), byref(usbCurrent))
    dwf.FDwfAnalogIOChannelNodeStatus(hdwf, c_int(3), c_int(0), byref(auxVoltage))
    dwf.FDwfAnalogIOChannelNodeStatus(hdwf, c_int(3), c_int(1), byref(auxCurrent))
    print("USB: " + str(round(usbVoltage.value, 3)) + "V\t" + str(round(usbCurrent.value, 3)) + "A")
    print("AUX: " + str(round(auxVoltage.value, 3)) + "V\t" + str(round(auxCurrent.value, 3)) + "A")

    # in case of over-current condition the supplies are disabled
    dwf.FDwfAnalogIOEnableStatus(hdwf, byref(IsEnabled))
    if not IsEnabled:
        # re-enable supplies
        print("Power supplies stopped. Restarting...")
        dwf.FDwfAnalogIOEnableSet(hdwf, c_int(False))
        dwf.FDwfAnalogIOEnableSet(hdwf, c_int(True))

print("Preparing to read sample...")
print("Generating sine wave...")
dwf.FDwfAnalogOutNodeEnableSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_bool(True))
dwf.FDwfAnalogOutNodeFunctionSet(hdwf, c_int(0), AnalogOutNodeCarrier, funcSine)
dwf.FDwfAnalogOutNodeFrequencySet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(6.103515625e3))
dwf.FDwfAnalogOutNodeAmplitudeSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(2))
dwf.FDwfAnalogOutConfigure(hdwf, c_int(0), c_bool(True))

# set up acquisition
dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(0), c_bool(True))
dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(0), c_double(5))
dwf.FDwfAnalogInAcquisitionModeSet(hdwf, acqmodeRecord)
dwf.FDwfAnalogInFrequencySet(hdwf, hzAcq)
dwf.FDwfAnalogInRecordLengthSet(hdwf, c_double(old_div(nSamples, hzAcq.value)))  # -1 infinite record length

# wait at least 2 seconds for the offset to stabilize
time.sleep(2)

# begin acquisition
dwf.FDwfAnalogInConfigure(hdwf, c_int(0), c_int(1))
print("   waiting to finish")

cSamples = 0

while cSamples < nSamples:
    dwf.FDwfAnalogInStatus(hdwf, c_int(1), byref(sts))
    if cSamples == 0 and (sts == DwfStateConfig or sts == DwfStatePrefill or sts == DwfStateArmed):
        # Acquisition not yet started.
        print("0")
        continue

    dwf.FDwfAnalogInStatusRecord(hdwf, byref(cAvailable), byref(cLost), byref(cCorrupted))

    cSamples += cLost.value

    if cLost.value:
        fLost = 1
    if cCorrupted.value:
        fCorrupted = 1

    if cAvailable.value == 0:
        continue

    if cSamples + cAvailable.value > nSamples:
        cAvailable = c_int(nSamples - cSamples)
    # get channel 1 data
    dwf.FDwfAnalogInStatusData(hdwf, c_int(0), byref(rgdSamples, sizeof(c_double) * cSamples), cAvailable)
    # get channel 2 data
    # dwf.FDwfAnalogInStatusData(hdwf, c_int(1), byref(rgdSamples, sizeof(c_double)*cSamples), cAvailable)
    cSamples += cAvailable.value

print("Recording finished")
if fLost:
    print("Samples were lost! Reduce frequency")
if fCorrupted:
    print("Samples could be corrupted! Reduce frequency")

# f = open("record.csv", "w")
# for v in rgdSamples:
#     f.write("%s\n" % v)
# f.close()

rgpy = [0.0] * len(rgdSamples)
for i in range(0, len(rgpy)):
    rgpy[i] = rgdSamples[i]

Time = []
for x in range(0, nSamples):
    Time.append(x/500000)

Freq = []
for y in range(0, nSamples):
    Freq.append(y/0.262144)

# Plot voltage range against Time
plt.figure(1)
plt.plot(Time, rgpy)
plt.xlabel('Time (s)')
plt.ylabel('Voltage (V)')
plt.title('Voltage range')
plt.show()

# Plot fft
a = np.array(rgpy)
A = fft(a) / len(a)
plt.figure(2)
plt.semilogy(Freq, abs(A))
plt.xlabel('Frequency (Hz)')
plt.ylabel('Logarithmic Scale - Voltage (V)')
plt.title('Voltage range - FFT')
plt.show()


dwf.FDwfDeviceClose(hdwf)
dwf.FDwfDeviceCloseAll()
