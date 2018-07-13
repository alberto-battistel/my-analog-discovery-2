"""
   DWF Python Example
   Author:  Digilent, Inc.
   Revision:  10/17/2013

   Requires:                       
       Python 2.7
"""

from ctypes import *
import sys
import time

if sys.platform.startswith("win"):
    dwf = cdll.dwf
elif sys.platform.startswith("darwin"):
    dwf = cdll.LoadLibrary("/Library/Frameworks/dwf.framework/dwf")
else:
    dwf = cdll.LoadLibrary("libdwf.so")

# check library loading errors
szerr = create_string_buffer(512)
dwf.FDwfGetLastErrorMsg(szerr)
print szerr.value

# declare ctype variables
IsInUse = c_bool()
hdwf = c_int()
rghdwf = []
cchannel = c_int()
cdevices = c_int()
voltage = c_double();

# declare string variables
devicename = create_string_buffer(64)
serialnum = create_string_buffer(16)

# print DWF version
version = create_string_buffer(16)
dwf.FDwfGetVersion(version)
print "DWF Version: "+version.value

# enumerate connected devices
dwf.FDwfEnum(c_int(0), byref(cdevices))
print "Number of Devices: "+str(cdevices.value)

# open devices
for idevice in range(0, cdevices.value):
    dwf.FDwfEnumDeviceName(c_int(idevice), devicename)
    dwf.FDwfEnumSN(c_int(idevice), serialnum)
    print "------------------------------"
    print "Device "+str(idevice+1)+" : "
    print "\t" + devicename.value
    print "\t" + serialnum.value
    dwf.FDwfDeviceOpen(c_int(idevice), byref(hdwf))
    if hdwf.value == 0:
        szerr = create_string_buffer(512)
        dwf.FDwfGetLastErrorMsg(szerr)
        print szerr.value
        dwf.FDwfDeviceCloseAll()
        sys.exit(0)
        
    rghdwf.append(hdwf.value)
        
    dwf.FDwfAnalogInChannelEnableSet(hdwf, c_int(-1), c_int(1)) 
    dwf.FDwfAnalogInChannelOffsetSet(hdwf, c_int(-1), c_double(0)) 
    dwf.FDwfAnalogInChannelRangeSet(hdwf, c_int(-1), c_double(5)) 
    dwf.FDwfAnalogInConfigure(hdwf, c_int(0), c_int(0)) 

for isample in range(0, 10):
    print "Sample "+str(isample+1)
    for idevice in range(0, cdevices.value):
        print "Device " + str(idevice+1)
        hdwf.value = rghdwf[idevice]
        dwf.FDwfAnalogInStatus(hdwf, False, None)
        dwf.FDwfAnalogInChannelCount(hdwf, byref(cchannel))
        for ichannel in range(0, cchannel.value):
            dwf.FDwfAnalogInStatusSample(hdwf, c_int(ichannel), byref(voltage))
            print "Channel " + str(ichannel+1)+" : "+ str(voltage.value)+"V"
    time.sleep(1)
    
# ensure all devices are closed
dwf.FDwfDeviceCloseAll()
