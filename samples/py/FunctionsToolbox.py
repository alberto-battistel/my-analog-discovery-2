from __future__ import division
from __future__ import print_function
import time
from numpy import *
import numpy as np
import scipy.fft
from matplotlib import pyplot as plt
from dwfconstants import *
import sys
import pandas as pd
from scipy.signal import find_peaks

# Define HDWF
hdwf = c_int()


# Device Initialization
def device_initialize():
    global hdwf  # Global variable
    if sys.platform.startswith("win"):
        dwf = cdll.dwf
    elif sys.platform.startswith("darwin"):
        dwf = cdll.LoadLibrary("/Library/Frameworks/dwf.framework/dwf")
    else:
        dwf = cdll.LoadLibrary("libdwf.so")

    # print DWF version
    version = create_string_buffer(16)
    dwf.FDwfGetVersion(version)
    print("DWF Version: " + version.value.decode("utf-8"))
    # open device
    print("Opening first device")
    dwf.FDwfDeviceOpen(c_int(-1), byref(hdwf))

    if hdwf.value == hdwfNone.value:
        sz_error = create_string_buffer(512)
        dwf.FDwfGetLastErrorMsg(sz_error)
        print(sz_error.value)
        print("Failed to open device")
        quit()
    print("Device Open")
    return dwf, hdwf


# Power Supply
def power_supply(dwf):
    # declare c_type variables for Power Supply
    IsEnabled = c_bool()
    usbVoltage = c_double()
    usbCurrent = c_double()
    auxVoltage = c_double()
    auxCurrent = c_double()

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
    print("Power Supply Initialized")


# Define the custom waveform for various frequencies
def waveform_design(number_period_list, N):
    time_period = arange(0, N) / N
    waveform = zeros((N,))
    for period in number_period_list:
        waveform += sin(2 * pi * period * time_period)
    waveform /= max(abs(waveform))
    print("Custom waveform created")
    return waveform


# Generate Waveform for at a given base frequency
def waveform_generator(dwf, data_c, base_frequency, Samples, Amplitude):
    dwf.FDwfAnalogOutNodeEnableSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_bool(True))
    dwf.FDwfAnalogOutNodeFunctionSet(hdwf, c_int(0), AnalogOutNodeCarrier, funcCustom)  # funcCustom
    dwf.FDwfAnalogOutNodeDataSet(hdwf, c_int(0), AnalogOutNodeCarrier, data_c, c_int(Samples))
    dwf.FDwfAnalogOutNodeFrequencySet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(base_frequency))
    dwf.FDwfAnalogOutNodeAmplitudeSet(hdwf, c_int(0), AnalogOutNodeCarrier, c_double(Amplitude))

    dwf.FDwfAnalogOutRunSet(hdwf, c_int(0), c_double(1))
    dwf.FDwfAnalogOutWaitSet(hdwf, c_int(0), c_double(0))  # wait one pulse time
    dwf.FDwfAnalogOutRepeatSet(hdwf, c_int(0), c_int(0))

    dwf.FDwfAnalogOutConfigure(hdwf, c_int(0), c_bool(True))
    print("Generating waveform")
    time.sleep(0.1)
    print("Waveform generated")


# Acquisition of the generated waveform
def waveform_acquisition_test(dwf, Sampling_frequency, number_samples, rgdSamples):
    sts = c_byte()

    # Set up acquisition
    dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(0), c_bool(True))
    dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(0), c_double(4))
    dwf.FDwfAnalogInFrequencySet(hdwf, Sampling_frequency)
    dwf.FDwfAnalogInBufferSizeSet(hdwf, number_samples)

    # wait at least 2 seconds for the offset to stabilize
    time.sleep(2)

    # begin acquisition
    dwf.FDwfAnalogInConfigure(hdwf, c_bool(False), c_bool(True))

    print("Waiting to finish acquisition")

    while True:
        dwf.FDwfAnalogInStatus(hdwf, c_int(1), byref(sts))
        if sts.value == DwfStateDone.value:
            break
        time.sleep(0.1)
    print("Acquisition done")

    dwf.FDwfAnalogInStatusData(hdwf, c_int(0), rgdSamples, number_samples)
    return rgdSamples


# Waveform Test with a different loop - Works as intended
def waveform_acquisition(dwf, Sampling_frequency, number_samples, rgdSamplesCH1, rgdSamplesCH2):
    sts = c_byte()
    cSamples = 0
    cAvailable = c_int()
    cLost = c_int()
    cCorrupted = c_int()
    fLost = 0
    fCorrupted = 0

    # Set up acquisition
    dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(0), c_bool(True))
    dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(1), c_bool(True))
    dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(-1), c_double(4))
    dwf.FDwfAnalogInFrequencySet(hdwf, Sampling_frequency)
    dwf.FDwfAnalogInBufferSizeSet(hdwf, number_samples)

    # wait at least 2 seconds for the offset to stabilize
    time.sleep(2)

    # begin acquisition
    dwf.FDwfAnalogInConfigure(hdwf, c_bool(False), c_bool(True))
    print("Waiting to finish acquisition")

    while cSamples < number_samples:
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

        if cSamples + cAvailable.value > int(number_samples):
            cAvailable = c_int(number_samples - cSamples)
        # get channel 1 data
        dwf.FDwfAnalogInStatusData(hdwf, c_int(0), byref(rgdSamplesCH1, sizeof(c_double) * cSamples), cAvailable)
        # get channel 2 data
        dwf.FDwfAnalogInStatusData(hdwf, c_int(1), byref(rgdSamplesCH2, sizeof(c_double) * cSamples), cAvailable)
        cSamples += cAvailable.value

    print("Recording finished")
    if fLost:
        print("Samples were lost! Reduce frequency")
    if fCorrupted:
        print("Samples could be corrupted! Reduce frequency")

    dwf.FDwfAnalogInStatusData(hdwf, c_int(0), rgdSamplesCH1, number_samples)
    dwf.FDwfAnalogInStatusData(hdwf, c_int(1), rgdSamplesCH2, number_samples)
    return rgdSamplesCH1, rgdSamplesCH2


def get_top_n_frequency_peaks(number_of_peaks, freqs, amplitudes, min_amplitude_threshold=None):
    """Finds the top N frequencies and returns a sorted list of tuples (freq, amplitudes)"""

    # Find the frequency peaks
    fft_peaks_indices, fft_peaks_props = find_peaks(amplitudes, height=min_amplitude_threshold)

    freqs_at_peaks = freqs[fft_peaks_indices]
    amplitudes_at_peaks = amplitudes[fft_peaks_indices]

    if number_of_peaks < len(amplitudes_at_peaks):
        ind = np.argpartition(amplitudes_at_peaks, -number_of_peaks)[-number_of_peaks:]
        ind_sorted_by_coefficient = ind[np.argsort(-amplitudes_at_peaks[ind])]  # reverse sort indices
    else:
        ind_sorted_by_coefficient = np.argsort(-amplitudes_at_peaks)

    return_list = list(zip(freqs_at_peaks[ind_sorted_by_coefficient], amplitudes_at_peaks[ind_sorted_by_coefficient]))
    return return_list


# Plots
def waveform_plots(number_samples, RgdSamplesCH1, RgdSamplesCH2, Sampling_Frequency):
    print("Generating plots")
    rgpyCH1 = [0.0] * len(RgdSamplesCH1)
    for i in range(0, len(rgpyCH1)):
        rgpyCH1[i] = RgdSamplesCH1[i]

    rgpyCH2 = [0.0] * len(RgdSamplesCH2)
    for i in range(0, len(rgpyCH2)):
        rgpyCH2[i] = RgdSamplesCH2[i]

    # number of samples / Time period = Sampling frequency
    Time = []
    for x in range(1, number_samples + 1):
        Time.append(x / Sampling_Frequency)
    t = max(Time)
    Freq = []
    for y in range(1, number_samples + 1):
        Freq.append(y / t)

    # Plot voltage range against Time
    plt.figure(2)
    plt.plot(Time, rgpyCH1)
    plt.xlabel('Time (s)')
    plt.ylabel('Voltage (V)')
    plt.title('Voltage range')
    plt.show()

    # Plot fft
    arrayCH1 = array(rgpyCH1)
    CH1 = scipy.fft.fft(arrayCH1) / len(arrayCH1)
    plt.figure(3)
    plt.semilogy(Freq, abs(CH1))
    plt.xlabel('Frequency (Hz)')
    plt.ylabel('Logarithmic Scale - Voltage (V)')
    plt.title('Voltage range - FFT')
    plt.show()

    plt.figure(4)
    plt.plot(Time, rgpyCH2)
    plt.xlabel('Time (s)')
    plt.ylabel('Current (A)')
    plt.title('Current range')
    plt.show()

    # Plot fft
    arrayCH2 = array(rgpyCH2)
    CH2 = scipy.fft.fft(arrayCH2) / len(arrayCH2)
    plt.figure(5)
    plt.semilogy(Freq, abs(CH2))
    plt.xlabel('Frequency (Hz)')
    plt.ylabel('Logarithmic Scale - Current (A)')
    plt.title('Current Range - FFT')
    plt.show()
    print("Plots of the Range and FFT generated")

    print("Extracting the FFT frequencies and amplitudes")
    # Set frequency limit
    freq_limit = 50000
    # Channel 1 - Voltage Range
    df1 = pd.DataFrame(Freq)
    df1['CH1'] = abs(CH1)
    df1 = df1.rename(columns={0: 'Freq'})
    n1 = get_top_n_frequency_peaks(9, np.array(df1[df1['Freq'] < freq_limit]['Freq']),
                                   np.array(df1[df1['Freq'] < freq_limit]['CH1']))
    n1 = pd.DataFrame(n1).sort_values(by=0, ascending=True)

    # Channel 2 - Current Range
    df2 = pd.DataFrame(Freq)
    df2['CH2'] = abs(CH2)
    df2 = df2.rename(columns={0: 'Freq'})
    n2 = get_top_n_frequency_peaks(9, np.array(df2[df2['Freq'] < freq_limit]['Freq']),
                                   np.array(df2[df2['Freq'] < freq_limit]['CH2']))
    n2 = pd.DataFrame(n2).sort_values(by=0, ascending=True)

    # Frequencies and peaks
    freq_ch1 = np.array(n1[0])
    freq_ch2 = np.array(n2[0])
    ch1 = np.array(n1[1])
    ch2 = np.array(n2[1])

    print('The frequencies of Channel 1 (Voltage) are: ', freq_ch1)
    print('The frequencies of Channel 2 (Current) are: ', freq_ch2)
    print('The Amplitude of Channel 1 (Voltage) are: ', ch1)
    print('The Amplitude of Channel 2 (Current) are: ', ch2)
    result = (ch1 / ch2)
    print('Absolute values: ', result)
    plt.figure(6)
    plt.scatter(freq_ch1, result)
    plt.xlabel('Frequencies (Hz)')
    plt.ylabel('|Amplitudes| of Voltage/Current')
    plt.show()

    # Attempt - Extraction
    # freqsCH1 = scipy.fft.fftfreq(number_samples, Sampling_Frequency)
    # absCH1 = abs(CH1)
    # absCH1 = absCH1[freqsCH1 > 0]
    # fCH1 = freqsCH1[freqsCH1 > 0]
    # peaks, _ = find_peaks(absCH1)
    # peaks
    # plt.figure(6)
    # plt.plot(fCH1, absCH1)
    # plt.plot(fCH1[peaks], absCH1[peaks], 'ro')
    # plt.show()


# Close device
def device_close(dwf):
    dwf.FDwfDeviceClose(hdwf)
    dwf.FDwfDeviceCloseAll()
    print("Device closed successfully")
