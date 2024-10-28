# -*- coding: utf-8 -*-
"""
digital_output_for_trig_test.py

Generates square waves from the same digital output used for framegrabber
triggering for testing purposes. For example, run this script while
controlling the framegrabber and camera with egrabber studio. This is easier
than switching to a function generator.

Author: mjs

2022-12-13

Created.
"""

import os
import traceback

import numpy as np

import datetime
import copy

import time

import matplotlib.pyplot as plt

import PyDAQmx as daqmx

import datetime

import ctypes

import pathlib
import h5py

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

"""
Current benchmark results (2022-11-30)

I was able to acquire a 2048x2048 volume at 83kHz with
these parameters.

GL_camera_integration_time_us = 6
GL_ao_update_freq_hz = 1e6
GL_camera_trig_freq_hz = 83e3
GL_grabber_device_CycleMinimumPeriod_us = 8

GL_img_size_x = int(2048)
GL_img_size_y = int(2048)

N_buffers = 20 # not sure if this needed to be 20
"""

GL_CAMERA_TRIG_OUTPUT_LINE = '/Dev3/port0/line0'
GL_CAMERA_TRIG_OUTPUT_LINE_DEBUG = '/Dev3/port0/line1'
GL_ao_update_freq_hz = 1e6
GL_camera_trig_freq_hz = 1e3
# GL_img_size_x must be between 2 and 2048 based on the frame rate calculator
# spreadsheet.
GL_img_size_x = int(100)
#
GL_img_size_y = int(1)


# #############################################################################
def make_one_trig_for_camera(fs, trig_freq):
    """Makes one period of the trig waveform for the camera

    Makes a square waveform with approx. 50% duty cycle with the high part
    in the middle (low parts at the start and end). Note, for best results
    choose fs and trig_freq such that fs / trig_freq is an integer, and
    ideally an even integer.

    Args:
        fs: (float): sampling frequency of output waveform in Hz. Typically 1e6.
        trig_freq: (float): desired trigger frequency in Hz. Typically 200e3.

    Returns:
        trig_wfm_digital: (np.array): trigger waveform for the camera, 1D numpy
            array.

    Example:
        make_one_trig_for_camera(fs=1e6, trig_freq=200e3)

    """
    num_pts_per_period = int(fs / trig_freq)
    pts_1_quarter = int(np.floor(num_pts_per_period * 0.25))
    pts_last_quarter = int(np.floor(num_pts_per_period * 0.75))
    trig_wfm_ramp = np.arange(1, num_pts_per_period + 1)
    trig_wfm_digital = np.zeros((num_pts_per_period,))
    trig_wfm_digital[np.logical_and(trig_wfm_ramp > pts_1_quarter, trig_wfm_ramp < pts_last_quarter)] = 1
    return trig_wfm_digital


class cameraTrigDigitalOutputTask(daqmx.Task):
    """Task for digital output for the trigger for the camera in the OCM system

    This was created from code snippets from PyVib2's
    DAQHardware.setupDigitalOutput.
    """

    def __init__(
            self,
            camera_trig_digital_wfm,
            camera_trig_output_line=GL_CAMERA_TRIG_OUTPUT_LINE,
    ):
        """
        Args:

            camera_trig_digital_wfm: (np.array): 1D numpy array containing the
                trigger waveform for the camera.

            camera_trig_output_line: (string): line on the NI DAQ card that
                the task will output the waveform to, this is typically set by
                the global variable GL_CAMERA_TRIG_OUTPUT_LINE and does not need
                to be specified when creating this object.
        """
        daqmx.Task.__init__(self)

        self.CreateDOChan(
            camera_trig_output_line,
            "",
            daqmx.DAQmx_Val_ChanPerLine,  # Or daqmx.DAQmx_Val_ChanForAllLines
        )

        clkSrc = ""
        sampleType = daqmx.DAQmx_Val_FiniteSamps
        numSamples = camera_trig_digital_wfm.size
        self.CfgSampClkTiming(
            clkSrc,
            GL_ao_update_freq_hz,
            daqmx.DAQmx_Val_Rising,
            sampleType,
            numSamples,
        )

        digTrigChan = ''
        if digTrigChan == '' or digTrigChan is None:
            self.DisableStartTrig()  # disable the start trigger if no digTrigChan is given
        else:
            self.CfgDigEdgeStartTrig(digTrigChan, daqmx.DAQmx_Val_Rising)

        isRetriggerable = False
        if isRetriggerable:
            val = daqmx.bool32(1)
            self.SetStartTrigRetriggerable(val)

        camera_trig_digital_wfm = np.require(
            camera_trig_digital_wfm,
            np.uint8,
            ['C', 'W'],
        )

        samplesWritten = daqmx.int32()
        self.WriteDigitalLines(
            numSampsPerChan=camera_trig_digital_wfm.size,
            autoStart=False,
            timeout=1.0,
            dataLayout=daqmx.DAQmx_Val_GroupByChannel,
            writeArray=camera_trig_digital_wfm,
            sampsPerChanWritten=ctypes.byref(samplesWritten),
            reserved=None,
        )
        logger.debug('cameraTrigDigitalOutputTask.__init__(): sample written %g' % samplesWritten.value)

    def startDigitalOutput(self):
        """Starts the digital output task
        """
        self.StartTask()

    def stopDigitalOutput(self):
        """Stops the digital output task
        """
        self.StopTask()

    def clearDigitalOutput(self):
        """Stops and clears the task
        """
        self.stopDigitalOutput()
        self.ClearTask()


class cameraTrigDigitalOutputTaskWithDebug(daqmx.Task):
    """Task for digital output for the trigger for the camera in the OCM system

    This was created from code snippets from PyVib2's
    DAQHardware.setupDigitalOutput.
    """

    def __init__(
            self,
            camera_trig_digital_wfm,
            camera_trig_output_line=GL_CAMERA_TRIG_OUTPUT_LINE,
            camera_trig_output_line_debug=GL_CAMERA_TRIG_OUTPUT_LINE_DEBUG,
    ):
        """
        Args:

            camera_trig_digital_wfm: (np.array): 1D numpy array containing the
                trigger waveform for the camera.

            camera_trig_output_line: (string): line on the NI DAQ card that
                the task will output the waveform to, this is typically set by
                the global variable GL_CAMERA_TRIG_OUTPUT_LINE and does not need
                to be specified when creating this object.
        """
        daqmx.Task.__init__(self)

        self.CreateDOChan(
            camera_trig_output_line,
            "",
            daqmx.DAQmx_Val_ChanPerLine,  # Or daqmx.DAQmx_Val_ChanForAllLines
        )
        self.CreateDOChan(
            camera_trig_output_line_debug,
            "",
            daqmx.DAQmx_Val_ChanPerLine,  # Or daqmx.DAQmx_Val_ChanForAllLines
        )

        clkSrc = ""
        sampleType = daqmx.DAQmx_Val_FiniteSamps
        numSamples = camera_trig_digital_wfm.size
        self.CfgSampClkTiming(
            clkSrc,
            GL_ao_update_freq_hz,
            daqmx.DAQmx_Val_Rising,
            sampleType,
            numSamples,
        )

        digTrigChan = ''
        if digTrigChan == '' or digTrigChan is None:
            self.DisableStartTrig()  # disable the start trigger if no digTrigChan is given
        else:
            self.CfgDigEdgeStartTrig(digTrigChan, daqmx.DAQmx_Val_Rising)

        isRetriggerable = False
        if isRetriggerable:
            val = daqmx.bool32(1)
            self.SetStartTrigRetriggerable(val)

        camera_trig_digital_wfm = np.concatenate((camera_trig_digital_wfm, camera_trig_digital_wfm))

        camera_trig_digital_wfm = np.require(
            camera_trig_digital_wfm,
            np.uint8,
            ['C', 'W'],
        )

        samplesWritten = daqmx.int32()
        self.WriteDigitalLines(
            numSampsPerChan=int(camera_trig_digital_wfm.size / 2),
            autoStart=False,
            timeout=1.0,
            dataLayout=daqmx.DAQmx_Val_GroupByChannel,
            writeArray=camera_trig_digital_wfm,
            sampsPerChanWritten=ctypes.byref(samplesWritten),
            reserved=None,
        )
        logger.debug('cameraTrigDigitalOutputTask.__init__(): sample written %g' % samplesWritten.value)

    def startDigitalOutput(self):
        """Starts the digital output task
        """
        self.StartTask()

    def stopDigitalOutput(self):
        """Stops the digital output task
        """
        self.StopTask()

    def clearDigitalOutput(self):
        """Stops and clears the task
        """
        self.stopDigitalOutput()
        self.ClearTask()



def TESTBENCH_digital_output():
    # Digital output testbench snippet
    do_wfm = np.tile(
        make_one_trig_for_camera(
            fs=GL_ao_update_freq_hz,
            trig_freq=GL_camera_trig_freq_hz
        ),
        (GL_img_size_x * GL_img_size_y,)
    ).astype('uint8')

    # ###
    task_camera_trig_digital_output = cameraTrigDigitalOutputTask(
        camera_trig_digital_wfm=do_wfm,
    )

    task_camera_trig_digital_output.startDigitalOutput()
    task_camera_trig_digital_output.WaitUntilTaskDone(-1)
    task_camera_trig_digital_output.stopDigitalOutput()


if (__name__ == "__main__"):
    GL_img_size_x = 10
    GL_img_size_y = 1
    extra_camera_trigs = 0 # NOTE 2 junk camera frames are hard-coded into the dll

    GL_camera_trig_freq_hz = 20
    GL_ao_update_freq_hz = GL_camera_trig_freq_hz * 10


    # Number of runs of the trigger generation code. Change this based on your
    # test conditions. Set n_trials to -1 to loop infinite.
    n_trials = 1

    # Digital output testbench snippet
    do_wfm = np.tile(
        make_one_trig_for_camera(
            fs=GL_ao_update_freq_hz,
            trig_freq=GL_camera_trig_freq_hz
        ),
        (GL_img_size_x * GL_img_size_y + extra_camera_trigs,)
    ).astype('uint8')

    # ###
    task_camera_trig_digital_output = cameraTrigDigitalOutputTaskWithDebug(
        camera_trig_digital_wfm=do_wfm,
    )
    # Initialize the loop counter.
    loop_ctr = 0
    while(True):
        try:
            logger.debug('task_camera_trig_digital_output.startDigitalOutput()')
            task_camera_trig_digital_output.startDigitalOutput()
        finally:
            logger.debug('task_camera_trig_digital_output.WaitUntilTaskDone(-1)')
            task_camera_trig_digital_output.WaitUntilTaskDone(-1)
            logger.debug('task_camera_trig_digital_output.stopDigitalOutput()')
            task_camera_trig_digital_output.stopDigitalOutput()

        # ###
        # end of trial actions.
        loop_ctr += 1
        if(n_trials > -1):
            if(loop_ctr >= n_trials):
                break
            else:
                # continue
                pass
        else:
            # Number of trials is negative, the user wants to loop infinite.
            pass




