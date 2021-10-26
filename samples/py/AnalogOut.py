"""
Analog_Out.py Problems Description: Generates the intended waveform in the amplitude set.
The waveforms figure and the FFT does not seem to be affected by the three commands FDwfAnalogOutRunSet,
FDwfAnalogOutWaitSet and FDwfAnalogOutRepeatSet. The number of periods also seem to be fixed around 5 or 6.
Further evaluations to fix this problem is to be undertaken.
Code Edit Notes: FDwfAnalogInBufferSizeSet was used and FDwfAnalogInRecordLengthSet and FDwfAnalogInAcquisitionModeSet
were not used in this file.
"""
from __future__ import division
from __future__ import print_function
import ctypes
from builtins import range
from matplotlib import pyplot as plt
from numpy import *
from past.utils import old_div
from dwfconstants import *
import time
import sys
import numpy as np
from scipy.fft import fft

if sys.platform.startswith("win"):
    dwf = cdll.dwf
elif sys.platform.startswith("darwin"):
    dwf = cdll.LoadLibrary("/Library/Frameworks/dwf.framework/dwf")
else:
    dwf = cdll.LoadLibrary("libdwf.so")


# Function to generate the waveform at various frequencies
def wave_generation(number_period_list, N=4096):
    time_period = np.arange(0, N) / N
    waveform = np.zeros((N,))
    for period in number_period_list:
        waveform += sin(2 * pi * period * time_period)
    waveform /= np.max(np.abs(waveform))
    return waveform


# Input the Frequencies list
period_list = [3, 5, 7]
data_extract = wave_generation(period_list)
data_c = (ctypes.c_double * len(data_extract))(*data_extract)
plt.figure(1)
plt.plot(data_extract)
plt.show()

# Declare Constants
nSamples = c_int(1024)  # Number of Samples: 131.072e3
Sampling_Frequency = c_double(150000)  # Sampling Frequency > 20 * max(period_list) * base frequency
cSamples = 4096  # Waveform Samples
baseFrequency = 1000  # 1kHz
fLost = 0
fCorrupted = 0

# declare ctype variables
hdwf = c_int()
rgdSamples = (c_double * cSamples)()
channel = c_int(0)
IsEnabled = c_bool()
usbVoltage = c_double()
usbCurrent = c_double()
auxVoltage = c_double()
auxCurrent = c_double()
cAvailable = c_int()
cLost = c_int()
cCorrupted = c_int()
sts = c_byte()

# print DWF version
version = create_string_buffer(16)
dwf.FDwfGetVersion(version)
print("DWF Version: " + version.value.decode("utf-8"))

# open device
"Opening first device..."
dwf.FDwfDeviceOpen(c_int(-1), byref(hdwf))

if hdwf.value == hdwfNone.value:
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

print("Preparing to read samples...")

t = time.time()

dwf.FDwfAnalogOutNodeEnableSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_bool(True))
dwf.FDwfAnalogOutNodeFunctionSet(hdwf, c_int(0), AnalogOutNodeCarrier, funcCustom)  # funcCustom
dwf.FDwfAnalogOutNodeDataSet(hdwf, c_int(0), AnalogOutNodeCarrier, data_c, c_int(cSamples))
dwf.FDwfAnalogOutNodeFrequencySet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(baseFrequency))
dwf.FDwfAnalogOutNodeAmplitudeSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(1.0))

dwf.FDwfAnalogOutRunSet(hdwf, c_int(0), c_double(1))
dwf.FDwfAnalogOutWaitSet(hdwf, c_int(0), c_double(0))  # wait one pulse time
dwf.FDwfAnalogOutRepeatSet(hdwf, c_int(0), c_int(0))

dwf.FDwfAnalogOutConfigure(hdwf, c_int(0), c_bool(True))

elapsed = time.time() - t
print("elapsed time:" + str(elapsed) + "seconds")
print("Generating waveform ...")
time.sleep(0.1)

print("done")
# Set up acquisition
dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(0), c_bool(True))
dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(0), c_double(4))
dwf.FDwfAnalogInFrequencySet(hdwf, Sampling_Frequency)
dwf.FDwfAnalogInBufferSizeSet(hdwf, nSamples)

# wait at least 2 seconds for the offset to stabilize
time.sleep(2)

# begin acquisition
dwf.FDwfAnalogInConfigure(hdwf, c_bool(False), c_bool(True))

print("waiting to finish")

while True:
    dwf.FDwfAnalogInStatus(hdwf, c_int(1), byref(sts))
    if sts.value == DwfStateDone.value:
        break
    time.sleep(0.1)
print("Acquisition done")

dwf.FDwfAnalogInStatusData(hdwf, c_int(0), rgdSamples, nSamples)

rgpy = [0.0] * len(rgdSamples)
for i in range(0, len(rgpy)):
    rgpy[i] = rgdSamples[i]

Time = []
for x in range(0, cSamples):
    Time.append(x / 500000)

Freq = []
for y in range(0, cSamples):
    Freq.append(y / 0.262144)

# Plot voltage range against Time
plt.figure(2)
plt.plot(Time, rgpy)
plt.xlabel('Time (s)')
plt.ylabel('Voltage (V)')
plt.title('Voltage range')
plt.show()

# Plot fft
a = np.array(rgpy)
A = fft(a) / len(a)
plt.figure(3)
plt.semilogy(Freq, abs(A))
plt.xlabel('Frequency (Hz)')
plt.ylabel('Logarithmic Scale - Voltage (V)')
plt.title('Voltage range - FFT')
plt.show()

dwf.FDwfDeviceClose(hdwf)
dwf.FDwfDeviceCloseAll()

