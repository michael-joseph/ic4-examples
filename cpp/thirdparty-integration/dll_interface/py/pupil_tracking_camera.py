#!/usr/bin/env python
"""!
@file pupil_tracking_camera.py

Python interface for the pupil tracking camera dll. This version just shows the
live data feed from the camera, acquiring data isn't supported yet.

@author mjs

2024-08-20

Created.
"""


import pathlib
import ctypes

import numpy as np
import numpy.ctypeslib as np_ctypes

import matplotlib.pyplot as plt

import datetime

import time
import h5py
import re

import time

###############################################################################
# Logging setup
#
# This will initialize a logger with two 'handlers'. One handler will be
# resonsible for writing to the console window, and the other will write to a
# log file in the same directory as this script (with the same name as this
# script too).
import os
import sys
import inspect


def get_script_dir(follow_symlinks=True):
    if getattr(sys, 'frozen', False):  # py2exe, PyInstaller, cx_Freeze
        path = os.path.abspath(sys.executable)
    else:
        path = inspect.getabsfile(get_script_dir)
    if follow_symlinks:
        path = os.path.realpath(path)
    return os.path.dirname(path)


import logging
from logging.handlers import RotatingFileHandler

log_name = os.path.splitext(os.path.basename(__file__))[0] + '.log.txt'
logFormatter = logging.Formatter(
    '%(asctime)s %(levelname)s %(filename)s [ %(funcName)s %(processName)s %(threadName)s ] %(message)s',
    datefmt="%Y%m%d-%H%M%S")

logger = logging.getLogger(log_name)
if (logger.hasHandlers()):
    # We've run the same script again without resetting the python env, so we
    # will not add the handlers again (otherwise you will see multiple copies
    # of each log message)
    pass
else:
    # filehandler = RotatingFileHandler(log_name, mode='a', maxBytes=10 * 2 ** 20, backupCount=1)
    # filehandler.setFormatter(logFormatter)
    # logger.addHandler(filehandler)

    consoleHandler = logging.StreamHandler()
    consoleHandler.setFormatter(logFormatter)
    logger.addHandler(consoleHandler)

LOG_LVL_TRACE = 1
##
# Set log level
logger.setLevel(LOG_LVL_TRACE)


def debug_trace():
    """Set a tracepoint in the Python debugger that works with Qt

    https://stackoverflow.com/questions/1736015/debugging-a-pyqt4-app
    """
    # Or for Qt5
    from PyQt5.QtCore import pyqtRemoveInputHook

    from pdb import set_trace
    pyqtRemoveInputHook()
    set_trace()



class ReadOldestFrameTimeoutError(Exception):
    """!"""
    pass


class ImageWidthAndHeightRefreshNeededError(Exception):
    """!"""
    pass


class pt_camera_dll():
    """!"""
    def __init__(self, path_to_dll=None):
        if(path_to_dll is None):
            path_to_dll = pathlib.Path(get_script_dir()) / pathlib.Path(
                r"..\dll\pupil_tracking_camera_dll_interface.dll"
            )

        # https://stackoverflow.com/questions/59330863/cant-import-dll-module-in-python
        # This would not load correctly without winmode=0. I'm guessing the opencv
        # stuff isn't found correctly. Also, all required dll's need to be in the
        # same directory when we load the dll. There's probably some way to add this
        # to the system path too but I haven't figure it out yet.
        prev_dir = pathlib.Path(get_script_dir())
        logger.debug('   dll exists: %d' % path_to_dll.exists())
        os.chdir(path_to_dll.parent.as_posix())
        dll = ctypes.CDLL(path_to_dll.as_posix(), winmode=0)
        os.chdir(prev_dir.as_posix())
        logger.debug('    done loading dll')


        # /*
        # The start_interface function will now serve as a testbench for controlling
        # the worker thread through simple key presses (_getwch) so we can more easily
        # transition to a dll interface.
        # */
        # DLL_EXPORT int DLL_CALLSPEC start_interface();
        dll.start_interface.argtypes = []
        dll.start_interface.restypes = ctypes.c_int


        # /*
        # Sets the global stop flag to stop the worker/camera thread.
        # */
        # DLL_EXPORT int DLL_CALLSPEC stop_interface();
        dll.stop_interface.argtypes = []
        dll.stop_interface.restypes = ctypes.c_int

        # /*
        # Calls worker_thread.join()
        # */
        # DLL_EXPORT int DLL_CALLSPEC join_interface();
        dll.join_interface.argtypes = []
        dll.join_interface.restypes = ctypes.c_int


        # int set_external_trigger_enable(bool val)
        dll.set_external_trigger_enable.argtypes = [ctypes.c_bool]
        dll.set_external_trigger_enable.restypes = ctypes.c_int


        # DLL_EXPORT int DLL_CALLSPEC set_frames_grabbed(size_t val);
        dll.set_frames_grabbed.argtypes = [ctypes.c_size_t]
        dll.set_frames_grabbed.restypes = ctypes.c_int


        # DLL_EXPORT size_t DLL_CALLSPEC get_frames_grabbed();
        dll.get_frames_grabbed.argtypes = []
        dll.get_frames_grabbed.restypes = ctypes.c_size_t


        # DLL_EXPORT int DLL_CALLSPEC set_frames_to_grab(size_t val);
        dll.set_frames_to_grab.argtypes = [ctypes.c_size_t]
        dll.set_frames_to_grab.restypes = ctypes.c_int


        # DLL_EXPORT size_t DLL_CALLSPEC get_frames_to_grab();
        dll.get_frames_to_grab.argtypes = []
        dll.get_frames_to_grab.restypes = ctypes.c_size_t


        # DLL_EXPORT int DLL_CALLSPEC print_info_on_frames();
        dll.print_info_on_frames.argtypes = []
        dll.print_info_on_frames.restypes = ctypes.c_size_t

        # DLL_EXPORT size_t DLL_CALLSPEC get_number_of_frames()
        dll.get_number_of_frames.argtypes = []
        dll.get_number_of_frames.restypes = ctypes.c_size_t

        # DLL_EXPORT size_t DLL_CALLSPEC get_frame_size_in_bytes()
        dll.get_frame_size_in_bytes.argtypes = []
        dll.get_frame_size_in_bytes.restypes = ctypes.c_size_t

        # DLL_EXPORT size_t DLL_CALLSPEC get_image_width();
        dll.get_image_width.argtypes = []
        dll.get_image_width.restypes = ctypes.c_size_t


        # DLL_EXPORT size_t DLL_CALLSPEC get_image_height();
        dll.get_image_height.argtypes = []
        dll.get_image_height.restypes = ctypes.c_size_t

        # DLL_EXPORT int DLL_CALLSPEC read_oldest_frame(char* user_buffer)
        data_t = np_ctypes.ndpointer(ctypes.c_uint8, flags="C_CONTIGUOUS")
        dll.read_oldest_frame.argtypes = [data_t]
        dll.read_oldest_frame.restypes = ctypes.c_int

        # DLL_EXPORT int DLL_CALLSPEC clear_frame_list()
        dll.clear_frame_list.argtypes = []
        dll.clear_frame_list.restypes = ctypes.c_int

        self._dll = dll

        self._height = 0
        self._width = 0

        self._need_to_refresh_height_width = True


    def start(self):
        """!
        Starts the camera interface. Call this once after opening the DLL.
        """
        self._dll.start_interface()


    def arm(self, frames_to_grab):
        """!
        Prepares the dll interface to acquire another sequence of frames.
        """
        # ensure that we're not grabbing any more frames by setting the frames
        # to grab to -1.
        self._dll.set_frames_to_grab(-1)
        # Clear the frame list
        self._dll.clear_frame_list()
        # Set the frames grabbed to 0.
        self._dll.set_frames_grabbed(0)
        # Set the frames to grab to the desired number, this will start
        # acquiring frames immediately when they arrive.
        self._dll.set_frames_to_grab(frames_to_grab)


    def fetch_image_sizes(self):
        """!
        Get the sizes of the image from the frames stored internally. This must
        be called when there is at least one frame stored internally in the dll.
        """
        self._height = self._dll.get_image_height()
        self._width = self._dll.get_image_width()
        if(self._height != 0 and self._width != 0):
            # If both are nonzero then we sucessfully fetched the image sizes.
            self._need_to_refresh_height_width = False
        else:
            # If both are not zero then we still need to fetch the image sizes
            # again.
            self._need_to_refresh_height_width = True


    def read_oldest_frame(self, timeout_s=5):
        """!"""
        d = np.zeros((self._dll.get_frame_size_in_bytes(),), dtype=np.uint8)
        start_time = time.time()
        err = -1
        while(err == -1):
            err = self._dll.read_oldest_frame(d)
            if(time.time() - start_time > timeout_s):
                raise ReadOldestFrameTimeoutError()
        if(self._need_to_refresh_height_width):
            self.fetch_image_sizes()
            if(self._need_to_refresh_height_width):
                raise ImageWidthAndHeightRefreshNeededError()

        d = d.reshape((self._height, self._width))
        return d


    def get_frames_to_grab(self):
        """!
        """
        # return self._dll.get_frames_to_grab()
        return self._dll.get_frames_grabbed()


    def read_all_frames_into_frame_list(self, only_read_available=True):
        """!
        Function to read all frames (according to get_frames_to_grab()) into
        an internal list.
        """
        self._frame_list = []
        if(only_read_available):
            frames_to_read = self._dll.get_frames_grabbed()
        else:
            frames_to_read = self.get_frames_to_grab()
        for n in range(0, self.get_frames_to_grab()):
            self._frame_list.append(self.read_oldest_frame())


    def save_frame_list_to_hdf5(self, path_to_hdf5):
        """!"""
        with h5py.File(path_to_hdf5.as_posix(), 'a') as f:
            f.create_group(
                name='/pt_camera',
            )
            if(len(self._frame_list) > 0):
                # only save if we acquired at least 1 frame.
                f.create_dataset(
                    name='/pt_camera/data',
                    data=np.stack(self._frame_list, axis=0),
                )



    def stop(self):
        """!"""
        self._dll.stop_interface()


    def join(self):
        """!"""
        self._dll.stop_interface()
        self._dll.join_interface()







################################################################################
# Testbench acquisition (assumes internal triggering or external triggers will
# be provided from elsewhere).
################################################################################
if(__name__ == "__main__"):
    import matplotlib.pyplot as plt


    x = pt_camera_dll(
        path_to_dll=pathlib.Path(r"C:\Users\ohns-user\Documents\GitHub\ic4-examples\cpp\thirdparty-integration\dll_interface\dll_interface\x64\Release\dll_interface.dll")
    )

    x._dll.set_external_trigger_enable(False)

    x.start()
    ###
    # arm
    x.arm(3000)


    # Change to external triggering for a few seconds
    time.sleep(5)
    x._dll.set_external_trigger_enable(True)
    time.sleep(2)
    # Change back to internal triggering
    x._dll.set_external_trigger_enable(False)

    # wait for frames
    time.sleep(5)
    # Read the frames
    acq1_frames = []
    for n in range(0, x.get_frames_to_grab()):
        acq1_frames.append(x.read_oldest_frame())

    # ###
    # # arm
    # x.arm(2000)
    # # wait for frames
    # time.sleep(1)
    # # Read the frames
    # acq2_frames = []
    # for n in range(0, x.get_frames_to_grab()):
    #     acq2_frames.append(x.read_oldest_frame())

    x.stop()
    x.join()








