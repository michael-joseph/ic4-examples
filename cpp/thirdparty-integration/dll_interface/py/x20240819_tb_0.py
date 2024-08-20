"""!
@file x20240819_tb_0.py

Testbench for the simple camera/dll that just plots a downsampled version of
the camera feed in an opencv imshow window.

@author mjs

2024-08-19

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




################################################################
if(__name__ == "__main__"):
    #
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

    #

    # /*
    # The start_interface function will now serve as a testbench for controlling
    # the worker thread through simple key presses (_getwch) so we can more easily
    # transition to a dll interface.
    # */
    # DLL_EXPORT int DLL_CALLSPEC start_interface();
    dll.start_interface.argtypes = []
    dll.start_interface.restypes = ctypes.c_int

    #
    #
    # /*
    # Sets the global stop flag to stop the worker/camera thread.
    # */
    # DLL_EXPORT int DLL_CALLSPEC stop_interface();
    dll.stop_interface.argtypes = []
    dll.stop_interface.restypes = ctypes.c_int

    # /*
    # Calls worker_thread.join(), this shoul
    # */
    # DLL_EXPORT int DLL_CALLSPEC join_interface();
    dll.join_interface.argtypes = []
    dll.join_interface.restypes = ctypes.c_int



    ############################################################################

    dll.start_interface()

    time.sleep(5)

    dll.stop_interface()
    dll.join_interface()














