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
img1_path = "picture/ad_units/ad_unit_839.jpg"
img2_path = "picture/ad_units/ad_unit_837.jpg"

np.set_printoptions(threshold=np.inf)
img1 = cv2.imread(img1_path,0)
#t_img1 = cv2.resize(img1, (138,138))
#img1 = np.zeros((640,720),dtype = np.uint8)
#print(t_img1.shape,img1.shape)
#for i in range(13):
#    for j in range(13):
#        img1[i][j] = t_img1[i][j]
img2 = cv2.imread(img2_path,0)
#t_img2 = cv2.resize(img2, (720,640))
#img2 = np.zeros((640,720),dtype = np.uint8)
"""
for i in range(640):
    for j in range(720):
        img2[i][j] = t_img2[i][j]
"""
img1d = img1.flatten()
img2d = img2.flatten()
"""
with open('11','w') as f:
    f.write(str(img1d))
with open('12','w') as f:
    f.write(str(img2d))
"""
frame_data1 = np.asarray(img1d, dtype=np.uint8)
frame_data1 = frame_data1.ctypes.data_as(ctypes.c_char_p)
frame_data2 = np.asarray(img2d, dtype=np.uint8)
frame_data2 = frame_data2.ctypes.data_as(ctypes.c_char_p)
h1,w1=img1.shape[0],img1.shape[1] 
h2,w2=img2.shape[0],img2.shape[1] 
sotest.sift_process.restype = POINTER(RetStruct)
t = sotest.sift_process(h1,w1,frame_data1,h2,w2,frame_data2,12)



print('res' + ' = ' + str(t))
endtime = datetime.datetime.now()
