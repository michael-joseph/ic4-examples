"""!
@file x20241027_tb_0.py

Testbench for external triggering. This requires a national instruments card.
In this testbench we generate a pre-defined number of triggers at a specific
rate and read out all frames collected by the camera. This will test if we can
acquire frames from the camera by counting the number of actual frames vs
expected frames.

    1. Run this script to start acquiring.

    2. Run the digital_output_for_trig_test.py script in another process to
        generate triggers for the camera.

@author mjs

2024-10-27

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


####
import pupil_tracking_camera  # pt_camera_dll


################################################################################
if(__name__ == "__main__"):
    x = pupil_tracking_camera.pt_camera_dll(
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
    time.sleep(30)
    # Change back to internal triggering
    x._dll.set_external_trigger_enable(False)

    # wait for frames
    time.sleep(1)
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








