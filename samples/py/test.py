# import scipy
# from matplotlib import pyplot as plt
# from numpy import *
#
#
# def waveform_generator(f1, f2, f3):
#     for t in range(10):
#         wave1 = sin(2 * pi * f1 * t)
#         wave2 = sin(2 * pi * f2 * t)
#         wave3 = sin(2 * pi * f3 * t)
#     wave = (wave1 + wave2) + wave3
#     return wave
#
#
# complete_wave = waveform_generator(3, 5, 7)
# plt.plot(complete_wave)
# plt.show()

import numpy as np
import scipy
from matplotlib import pyplot as plt
from numpy import sin, pi, linspace
from scipy.io import wavfile
import ctypes


# def wave_generation(fs, f1, f2, f3):
#     time_period = 10
#     samples = np.linspace(0, time_period, int(fs * time_period), endpoint=False)
#     signal = np.sin(2 * np.pi * f1 * samples) + np.sin(2 * np.pi * f2 * samples) + np.sin(2 * np.pi * f3 * samples)
#     signal *= 32767
#     signal = np.int16(signal)
#     wavfile.write(str("sinusoid.wav"), fs, signal)
#
#
# wave_generation(10000, 3, 5, 7)
# rate, data = scipy.io.wavfile.read('sinusoid.wav')
# print("Rate: " + str(rate))
# print("Size: " + str(data.size))
# print("Type: " + str(np.dtype(data[0])))
# # AnalogOut expects double normalized to +/-1 value
# data_float = data.astype(np.float64)
# print("Scaling: UINT8")
# data_float /= 128.0
# data_float -= 1.0
# data_c = (ctypes.c_double * len(data_float))(*data_float)
# plt.plot(data)
# plt.show()

def wave_generation(frequency_list, N=4096):
    time_period = np.arange(0, N) / N
    waveform = np.zeros((N, ))
    for frequency in frequency_list:
        waveform += sin(2 * pi * frequency * time_period)
    waveform /= np.max(np.abs(waveform))
    return waveform


freq_list = [3, 5, 7]
wave = wave_generation(freq_list)
plt.plot(wave)
plt.show()
