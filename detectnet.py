#!/usr/bin/python3
#
# Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#

import jetson.inference
import jetson.utils

import argparse
import sys
from os import path
import serial
import time
from dogbot_client import ClientSide

# #Detect arduino serial path (Cater for different USB-Serial Chips)
# if path.exists("/dev/ttyACM0"):
# 	arduino = serial.Serial(port = '/dev/ttyACM0',baudrate = 115200)
# elif path.exists("/dev/ttyUSB0"):
# 	arduino = serial.Serial(port = '/dev/ttyUSB0',baudrate = 115200)
# else:
# 	print("Please plug in the Arduino")
# 	exit()
# if not(arduino.isOpen()):
#     arduino.open()

#Variables for command and control
pan =90
tilt = 90
tilt_offset = 30
pan_prev =90
tilt_prev = 90
pan_max = 100
pan_min = 80
tilt_max = 100
tilt_min = 80
width = 1280
height = 720
objX = width/2
objY = height/2
error_tolerance = 100

##################  new add
error = 100
symbol = ""
boundary_area = 180000
servo1_open = 30  #claw: 30->open  initial state
servo1_close = 95 #claw: 95->close
servo2_initial = 90 #肘子: 90->middle      initial state
grab_y = 210 # control 肘子

'''
Sample out:

   -- ClassID: 1
   -- Confidence: 0.973062
   -- Left:    50
   -- Top:     75.4102
   -- Right:   1279
   -- Bottom:  719
   -- Width:   1229
   -- Height:  643.59
   -- Area:    790972
   -- Center:  (664.5, 397.205)
'''

client_sock = ClientSide()

# parse the command line
parser = argparse.ArgumentParser(description="Locate objects in a live camera stream using an object detection DNN.", \
    formatter_class=argparse.RawTextHelpFormatter, epilog=jetson.inference.detectNet.Usage() +\
    jetson.utils.videoSource.Usage() + jetson.utils.videoOutput.Usage() + jetson.utils.logUsage())

#More arguments (For commandline arguments)
parser.add_argument("input_URI", type=str, default="", nargs='?', help="URI of the input stream")
parser.add_argument("output_URI", type=str, default="", nargs='?', help="URI of the output stream")
parser.add_argument("--network", type=str, default="ssd-mobilenet-v2", help="pre-trained model to load (see below for options)")
parser.add_argument("--overlay", type=str, default="box,labels,conf", help="detection overlay flags (e.g. --overlay=box,labels,conf)\nvalid combinations are:  'box', 'labels', 'conf', 'none'")
parser.add_argument("--threshold", type=float, default=0.5, help="minimum detection threshold to use") 

is_headless = ["--headless"] if sys.argv[0].find('console.py') != -1 else [""]

#Print help when no arguments given
try:
    opt = parser.parse_known_args()[0]
except:
    print("")
    parser.print_help()
    sys.exit(0)

# load the object detection network
net = jetson.inference.detectNet(opt.network, sys.argv, opt.threshold)

# create video sources & outputs
input = jetson.utils.videoSource(opt.input_URI, argv=sys.argv)
output = jetson.utils.videoOutput(opt.output_URI, argv=sys.argv+is_headless)

f = open('area.txt', 'w')
f.close()
# process frames until the user exits
while True:

    # capture the next image
    img = input.Capture()

    # detect objects in the image (with overlay)
    detections = net.Detect(img, overlay=opt.overlay)

    # print the detections
    print("detected {:d} objects in image".format(len(detections)))
    
    #Initialize the object coordinates and area
    objX = width/2
    objY = height/2
    Area = 0
    
    #Find largest detected objects (in case of deep learning confusion)
    for detection in detections:
        # print(detection)
        if(int(detection.Area)>Area):
            objX =int(detection.Center[0])
            objY = int(detection.Center[1])
            Area = int(detection.Area)
            Confidence = detection.Confidence

    # If more than one figures are detected, and those figures overlap, 
    # then a higher confidence level could be concluded.
    
    #Determine the adjustments needed to make to the cmaera
    
    panOffset = objX - (width/2)
    tiltOffset = objY - (height/2)
    
    #Puting the values in margins
    if (abs(panOffset)>error_tolerance):
        pan = pan-panOffset/100
    if (abs(tiltOffset)>error_tolerance):
        tilt = tilt+tiltOffset/100
    if pan>pan_max:
        pan = pan_max
    if pan<pan_min:
        pan=pan_min
    if tilt>tilt_max:
        tilt=tilt_max
    if tilt<tilt_min:
        tilt=tilt_min
    #Rounding them off
    pan = int(pan)
    tilt = int(tilt) + tilt_offset
    Area = int(Area)
    #Setting up command string
    myString = '(' +  str(pan) + ',' + str(tilt) + ',' + str(Area) + ')'  # original: pan, tilt, area --> objX, objY, Area
    # print("myString = %s" %myString)
    if len(detections) == 1 and Area != 0:
        f = open('area.txt', 'a+')
        print(Area, ' ', Confidence, file=f)
        f.close()
    
    ##======================##

    turnoffset = abs(objX - width/2)

    # condition1: no redball detect -> no change
    # doing nothing
    
    # condition2: redball detect, but not in the center
    # arguments: objx, objy, window size
    if (Area == 0): 
        client_sock.sending("undetected")
    else:
        client_sock.sending(f"detected. Area:{Area} Offset:{panOffset}")
        # condition2.1： 偏左 -> turn right
        if (objX < width/2 - error):
            client_sock.sending("r_cw") # return turnoffset
            
        # condition2.2 : 偏右 -> turn left
        elif (objX > width/2 + error) :
            client_sock.sending("r_ccw") # return turnoffset
    
        # condition3: redball detect and in the center, but not close enough -> go advance
        elif (objX > width/2 + error and objX < width/2 - error):
            # if window size, objx, objy ture -> compare window size
            if (Area < boundary_area):
                client_sock.sending("heading_target") # return turnoffset
                # return window size
    
            # condition4: readball detect, in the center, close engough --> grab
            else:
                client_sock.sending("grab")
                # return grab_y -> servo1_close -> servo2_initial
    
    ##===================##
    
    # #Print strings sent by arduino, if there's any
    # if arduino.inWaiting():
    #     print("From Arduino serial: %s" %arduino.readline().decode('utf-8'))
    #     arduino.flushInput()
    #     arduino.flushOutput()
    
    # #Determine if sending signals is necessary (trival adjustsments wastes time)
    # if (abs(pan - pan_prev) > 5 or abs(tilt - tilt_prev)> 5):
    #     pan_prev = pan
    #     tilt_prev = tilt
    #     #Send it if area is reasonable
    #     if (Area > 0 and Area < 300000):
    #         arduino.write(myString.encode())

    # render the image
    smallImg = jetson.utils.cudaAllocMapped(width=img.width*0.5, height=img.height*0.5, format=img.format)
    jetson.utils.cudaResize(img, smallImg)
    output.Render(smallImg)


    # update the title bar
    output.SetStatus("{:s} | Network {:.0f} FPS".format(opt.network, net.GetNetworkFPS()))
    
    # print out performance info
    net.PrintProfilerTimes()

    # exit on input/output EOS
    if not input.IsStreaming() or not output.IsStreaming():
        break


