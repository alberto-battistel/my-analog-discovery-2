"""
This is a merged file created from 'AnalogIn_Record.py', 'AnalogOut_Play.py' and 'AnalogIO_AnalogDiscovery2_Power.py'.
"""
from __future__ import division
from __future__ import print_function
import ctypes
from builtins import range
import scipy
from matplotlib import pyplot as plt
from numpy import *
from past.utils import old_div
from ctypes import *
from scipy.io import wavfile
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
def wave_generation(frequency_list, N=4096):
    time_period = np.arange(0, N) / N
    waveform = np.zeros((N,))
    for frequency in frequency_list:
        waveform += sin(2 * pi * frequency * time_period)
    waveform /= np.max(np.abs(waveform))
    wavfile.write(str("sinusoid.wav"), N, waveform)


# Input the Frequencies list
freq_list = [3, 5, 7]
wave_generation(freq_list)
rate, data = scipy.io.wavfile.read('sinusoid.wav')
print("Rate: " + str(rate))
print("Size: " + str(data.size))
print("Type: " + str(np.dtype(data[0])))
# AnalogOut expects double normalized to +/-1 value
dataf = data.astype(np.float64)
if np.dtype(data[0]) == np.int8 or np.dtype(data[0]) == np.uint8:
    print("Scaling: UINT8")
    dataf /= 128.0
    dataf -= 1.0
elif np.dtype(data[0]) == np.int16:
    print("Scaling: INT16")
    dataf /= 32768.0
elif np.dtype(data[0]) == np.int32:
    print("Scaling: INT32")
    dataf /= 2147483648.0
data_c = (ctypes.c_double * len(dataf))(*dataf)
plt.plot(data)
plt.show()

# Declare Constants
nSamples = int(131.072e3)
hzFreq = c_double(1000)
cSamples = 4096
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

print("Preparing to read sample...")

# samples between -1 and +1
# for i in range(0, len(wave)):
#     wave[i] = 1.0 * i / cSamples

# # Generate samples normalized to ±1
# for i in range(0, 4096):
#     i = i+1
#     rgdSamples = 2.0*i/4096-1

print("Playing audio...")
iPlay = 0
dwf.FDwfAnalogOutNodeEnableSet(hdwf, c_int(0), 0, c_bool(True))
dwf.FDwfAnalogOutNodeFunctionSet(hdwf, c_int(0), 0, funcCustom)  # funcCustom
dwf.FDwfAnalogOutRepeatSet(hdwf, c_int(0), c_int(100))
sRun = 1.0 * data.size / rate
print("Length: " + str(sRun))
dwf.FDwfAnalogOutRunSet(hdwf, c_int(0), c_double(sRun))
dwf.FDwfAnalogOutNodeFrequencySet(hdwf, c_int(0), 0, c_double(1000))
dwf.FDwfAnalogOutNodeAmplitudeSet(hdwf, c_int(0), 0, c_double(1.0))
# prime the buffer with the first chunk of data
cBuffer = c_int(0)
dwf.FDwfAnalogOutNodeDataInfo(hdwf, c_int(0), 0, 0, byref(cBuffer))
if cBuffer.value > data.size:
    cBuffer.value = data.size
dwf.FDwfAnalogOutNodeDataSet(hdwf, c_int(0), 0, data_c, cBuffer)
iPlay += cBuffer.value
dwf.FDwfAnalogOutConfigure(hdwf, c_int(0), c_bool(True))

# # Enable First Channel
# dwf.FDwfAnalogOutNodeEnableSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_bool(True))
# # Set Custom Function
# dwf.FDwfAnalogOutNodeFunctionSet(hdwf, c_int(0), AnalogOutNodeCarrier, funcCustom)
# # Set custom waveform samples (and Normalize to ±1 values)
# sRun = 1.0 * data.size / rate
# print("Length: " + str(sRun))
# # dwf.FDwfAnalogOutNodeDataSet(hdwf, c_int(0), AnalogOutNodeCarrier, rgdSamples, cSamples)
# # Set Waveform Frequency
# dwf.FDwfAnalogOutNodeFrequencySet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(1000))  # 1kHz Waveform Frequency
# # 2V Amplitude, 4V Peak-to-peak (Sample value -1, output -2V and sample value +1, output 2V)
# dwf.FDwfAnalogOutNodeAmplitudeSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(1.0))
# # Prime the buffer
# cBuffer = c_int(0)
# dwf.FDwfAnalogOutNodeDataInfo(hdwf, c_int(0), AnalogOutNodeCarrier, 0, byref(cBuffer))
# if cBuffer.value > data.size : cBuffer.value = data.size
# dwf.FDwfAnalogOutNodeDataSet(hdwf, c_int(0), AnalogOutNodeCarrier, data_c, cBuffer)
# iPlay += cBuffer.value
# # Run the waveform for a specified period
# dwf.FDwfAnalogOutRunSet(hdwf, c_int(0), c_double(sRun))  # Run time
# # Wait for a specific pulse time
# dwf.FDwfAnalogOutWaitSet(hdwf, c_int(0), c_double(old_div(1.0, max(freq_list))))  # wait one pulse time
# # Repeat the custom wave for a specific number of times
# dwf.FDwfAnalogOutRepeatSet(hdwf, c_int(0), c_int(3))  # repeat 5 times
# print("Generating Custom wave...")
# # Start Signal Generation
# dwf.FDwfAnalogOutConfigure(hdwf, c_int(0), c_bool(True))

print("Generating waveform ...")
print("done")

########################################################################################################################
# Set up acquisition
dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(0), c_bool(True))
dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(0), c_double(5))
dwf.FDwfAnalogInAcquisitionModeSet(hdwf, acqmodeRecord)
dwf.FDwfAnalogInFrequencySet(hdwf, hzFreq)
dwf.FDwfAnalogInRecordLengthSet(hdwf, c_double(old_div(nSamples, hzFreq.value)))  # -1 infinite record length

# wait at least 2 seconds for the offset to stabilize
time.sleep(2)

# begin acquisition
dwf.FDwfAnalogInConfigure(hdwf, c_int(0), c_int(1))
print("waiting to finish")

# cSamples changed to InitSamples
InitSamples = 0
while InitSamples < cSamples:
    dwf.FDwfAnalogInStatus(hdwf, c_int(1), byref(sts))
    if InitSamples == 0 and (sts == DwfStateConfig or sts == DwfStatePrefill or sts == DwfStateArmed):
        # Acquisition not yet started.
        print("0")
        continue

    dwf.FDwfAnalogInStatusRecord(hdwf, byref(cAvailable), byref(cLost), byref(cCorrupted))

    InitSamples += cLost.value

    if cLost.value:
        fLost = 1
    if cCorrupted.value:
        fCorrupted = 1

    if cAvailable.value == 0:
        continue

    if InitSamples + cAvailable.value > cSamples:
        cAvailable = c_int(nSamples - InitSamples)
    # get channel 1 data
    dwf.FDwfAnalogInStatusData(hdwf, c_int(0), byref(rgdSamples, sizeof(c_double) * InitSamples), cAvailable)
    # get channel 2 data
    # dwf.FDwfAnalogInStatusData(hdwf, c_int(1), byref(rgdSamples, sizeof(c_double) * cSamples), cAvailable)
    InitSamples += cAvailable.value

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
for x in range(0, cSamples):
    Time.append(x / 500000)

Freq = []
for y in range(0, cSamples):
    Freq.append(y / 0.262144)

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

# dwf.FDwfDeviceClose(hdwf)
# dwf.FDwfDeviceCloseAll()
