"""!
@file x20240922_tb_0.py

This testbench works with the dll that is updated to acquire camera frames.
This adds to the previous testbench (x20240819_tb_0.py).

@author mjs

2024-09-22

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


class pt_camera_dll():
    """!"""
    def __init__(self):
        path_to_dll = pathlib.Path(
            r"C:\Users\ohns-user\Documents\GitHub\ic4-examples\cpp\thirdparty-integration\dll_interface\dll_interface\x64\Release\dll_interface.dll"
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


    def start(self):
        """!"""
        self._dll.start_interface()


    def fetch_image_sizes(self):
        """!
        Get the sizes of the image from the frames stored internally. This must
        be called when there is at least one frame stored internally in the dll.
        """
        self._height = self._dll.get_image_height()
        self._width = self._dll.get_image_width()


    def read_oldest_frame(self, timeout_s=5):
        """!"""
        d = np.zeros((self._dll.get_frame_size_in_bytes(),), dtype=np.uint8)
        start_time = time.time()
        err = -1
        while(err == -1):
            err = self._dll.read_oldest_frame(d)
            if(time.time() - start_time > timeout_s):
                raise ReadOldestFrameTimeoutError()
        d = d.reshape((self._height, self._width))
        return d


    def get_frames_to_grab(self):
        return dll._dll.get_frames_to_grab()


    def stop(self):
        """!"""
        self._dll.stop_interface()


    def join(self):
        """!"""
        self._dll.stop_interface()
        self._dll.join_interface()




################################################################
if(__name__ == "__main__"):
    #
    dll = pt_camera_dll()

    dll._dll.set_frames_to_grab(49)
    dll._dll.set_frames_grabbed(0)



    ############################################################################

    dll.start()

    time.sleep(3)

    dll.fetch_image_sizes()

    dll.stop()
    dll.join()


    dll._dll.print_info_on_frames()

    frame_list = []
    times_to_read = dll.get_frames_to_grab()
    for i in range(0, times_to_read):
        frame_list.append(dll.read_oldest_frame())

    plt.imshow(frame_list[-1])
    plt.show()







