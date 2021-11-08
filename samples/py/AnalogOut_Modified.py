""" Description: This is a modified version of AnalogOut.py, albeit employing FunctionsToolbox.py. """
# Imports
from __future__ import division
from __future__ import print_function
import ctypes
from FunctionsToolbox import *
import matplotlib.pyplot as plt
# Initialize the device
dwf, hdwf = device_initialize()
# Supply ±5V
power_supply(dwf)
# Input the Frequencies list and create the custom waveform
period_list = [3, 7, 11, 17, 25, 33, 39, 41, 47]
waveform_Samples = 4096  # Waveform Samples
data_extract = waveform_design(period_list, waveform_Samples)
data_c = (ctypes.c_double * len(data_extract))(*data_extract)
# Plot the created custom waveform
plt.figure(1)
plt.plot(data_extract)
plt.show()
# Declare Constants and c_type variables
number_samples = int(4096)  # Number of Samples: Integer type, not c_type
Sampling_frequency = c_double(950000)  # Sampling Frequency > 2 * max(period_list) * base frequency
baseFrequency = 1000  # 1kHz
Amplitude = 2
rgdSamplesCH1 = (c_double * waveform_Samples)()
rgdSamplesCH2 = (c_double * waveform_Samples)()
# Generate the Waveform
waveform_generator(dwf, data_c, baseFrequency, waveform_Samples, Amplitude)
# Perform the acquisition and store the data in RgdSamples
RgdSamplesCH1, RgdSamplesCH2 = waveform_acquisition(dwf, Sampling_frequency, number_samples, rgdSamplesCH1, rgdSamplesCH2)
# Generate plots
Frequency = 950000
waveform_plots(number_samples, RgdSamplesCH1, RgdSamplesCH2, Frequency)

# Close the Device
device_close(dwf)
