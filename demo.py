from ctypes import *
import numpy as np
import os
import cv2
import ctypes
sotest = cdll.LoadLibrary(os.getcwd()+ "/libcudasiftlib.so")
import datetime

class SiftPoint(Structure):
    _fields_ = [
        ('xpos', c_float),
        ('ypos', c_float),
        ('scale' , c_float),
        ('sharpness', c_float),
        ('edgeness', c_float),
        ('orientation' , c_float),
        ('score', c_float),
        ('ambiguity', c_float),
        ('match' , c_int),
        ('match_xpos', c_float),
        ('match_ypos', c_float),
        ('match_error' , c_float),
        ('subsampling', c_float),
        ('empty', (c_float * 3)),
        ('data' , (c_float * 128)),
      ]

class SiftData(Structure):
    _fields_ = [
        ('numPts', c_int),
        ('maxPts', c_int),
        ('h_data' , (SiftPoint *1024)),
        ('d_data',  (SiftPoint *1024)),
    ]

class StructPointerTest(Structure):   
    _fields_ =[('x', c_int),  
               ('y', c_int)]  

class RetStruct(Structure):
    _fields_ =[('numPts', c_int),
               ('x_pos', POINTER(c_float)),
               ('y_pos', POINTER(c_float)),
               ('m_x_pos', POINTER(c_float)),
               ('m_y_pos', POINTER(c_float)),
               ('match_error', POINTER(c_float))]

starttime = datetime.datetime.now()
img1_path = "picture/ad_units/ad_unit_1133.jpg"
img2_path = "picture/ad_units/ad_unit_1177.jpg"

np.set_printoptions(threshold=np.inf)
img1 = cv2.imread(img1_path,0)
img2 = cv2.imread(img2_path,0)
img1d = img1.flatten()
img2d = img2.flatten()
frame_data1 = np.asarray(img1d, dtype=np.uint8)
frame_data1 = frame_data1.ctypes.data_as(ctypes.c_char_p)
frame_data2 = np.asarray(img2d, dtype=np.uint8)
frame_data2 = frame_data2.ctypes.data_as(ctypes.c_char_p)
h1,w1=img1.shape[0],img1.shape[1] 
h2,w2=img2.shape[0],img2.shape[1] 
sotest.sift_process.restype = POINTER(RetStruct)
t = sotest.sift_process(h1,w1,frame_data1,h2,w2,frame_data2)
print(t.contents.numPts)
print()
print()
count = 0
for i in range(t.contents.numPts):
    if t.contents.match_error[i]<3.0:
        count+=1
print(count)


print('res' + ' = ' + str(t))
endtime = datetime.datetime.now()
