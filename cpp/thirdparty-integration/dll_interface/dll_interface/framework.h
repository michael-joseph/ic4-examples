#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

// AVOID Weird crash locking mutex?
// https://stackoverflow.com/questions/78598141/first-stdmutexlock-crashes-in-application-built-with-latest-visual-studio
#define _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR

#include <opencv2/opencv.hpp>
#include <ic4/ic4.h>
#include <ic4-interop/interop-OpenCV.h>

#include <iostream>

#include <atomic>

#include <chrono>
#include <cmath>
#include <thread>
#include <mutex>


#define WIN32 1

// Include this at the start of the function definition to make it available
// to call from the dll.
#define DLL_EXPORT extern "C" __declspec(dllexport)
#define DLL_CALLSPEC __stdcall


extern std::mutex camera_frame_acq_mutex;
extern std::atomic<size_t> frames_grabbed;
extern std::atomic<size_t> frames_to_grab;
extern std::list<cv::Mat> frame_list;

/*
Last frame's width.
*/
extern std::atomic<size_t> last_frame_width;

/*
Last frame's height.
*/
extern std::atomic<size_t> last_frame_height;


/*
Gets the size of the screen (not tested on multi-monitor setups).

This is used in the customQueueSinkListener() class to position the
opencv window.

https://stackoverflow.com/questions/11041607/getting-screen-size-on-opencv
*/
void getScreenResolution(int& width, int& height);


/*
Prints the steam statistics.
*/
void print_streamStatistics(ic4::Grabber::StreamStatistics stats);


/*
Custom subclass of QueueSinkListener to handle interfacing with a QueueSink.

This class opens an opencv window and plots a down-sampled version of the
given frame in each call to framesQueued(). Note that when opencv is used
like this the call to cv::namedWindow() and cv::imshow() must all be in the
same thread. That's why I had to put it in framesQueued().

This class also updates the last_key variable so that the main thread can


@warning Avoid manually dragging the opencv window because this will block
the callback from finishing and will lead to overflow in the frame queue.
Unfortunately I don't know how to avoid this behavior with opencv.

file:///C:/Program%20Files/The%20Imaging%20Source%20Europe%20GmbH/ic4/share/theimagingsource/ic4/doc/cpp/classic4_1_1_queue_sink.html
file:///C:/Program%20Files/The%20Imaging%20Source%20Europe%20GmbH/ic4/share/theimagingsource/ic4/doc/cpp/classic4_1_1_queue_sink_listener.html


*/
class customQueueSinkListener : public ic4::QueueSinkListener {

public:

	/*
	Default constructor. We need the width and height of the stream
	to position the opencv window.
	*/
	customQueueSinkListener(int grabber_width, int grabber_height) :
		grabber_width(grabber_width), grabber_height(grabber_height)
	{}

	bool sinkConnected(ic4::QueueSink& sink, const ic4::ImageType imageType, size_t min_buffers_required) {
		std::cout << "min_buffers_required: " << min_buffers_required << std::endl;
		// Allocate buffers for the sink. First this will be hardcoded, then we'll 
		// make it more general.
		ic4::Error err;
		if (!sink.allocAndQueueBuffers(100, err)) {
			std::cout << "ERROR sink.allocAndQueueBuffers()" << std::endl;
		}
	}

	void framesQueued(ic4::QueueSink& sink) {
		static bool first_call = true;
		static size_t counter = 0;
		static auto frame_end_time = std::chrono::high_resolution_clock::now();



		if (first_call) {
			// Create an OpenCV display window
			std::cout << "opening window" << std::endl;
			cv::namedWindow("display", cv::WINDOW_AUTOSIZE);

			// Get the size of the screen (not sure how this works on multi-monitor setups)
			int scrn_width, scrn_height;
			getScreenResolution(scrn_width, scrn_height);

			double img_scale_factor = 0.2;

			// Move the window to the lower right.
			// Auto adjust this based on the resolution of the camera image after we
			// do down-scaling by img_scale_factor using opencv.
			int cv_window_extra_width_offset = -50;
			int cv_window_extra_height_offset = -100;
			cv::moveWindow(
				"display",
				scrn_width + cv_window_extra_width_offset - (int)(img_scale_factor * (double)grabber_width),
				scrn_height + cv_window_extra_height_offset - (int)(img_scale_factor * (double)grabber_height)
			);

			first_call = false;
		}


		auto buffer = sink.popOutputBuffer();

		double img_scale_factor = 0.2;

		// Create a cv::Mat
		auto mat = ic4interop::OpenCV::wrap(*buffer);

		// Update the last frame width and height. The code assumes that the frame 
		// sizes are not changing over the course of an acquisition!
		cv::Size mat_sz = mat.size();
		last_frame_height.store(mat_sz.height);
		last_frame_width.store(mat_sz.width);


		/*
		Acquire the frame if enabled.
		*/
		{
			const std::lock_guard<std::mutex> lock(camera_frame_acq_mutex);
			// Add the frame to the frame_list if we're acquiring, otherwise
			// skip. Then increment the frames_grabbed counter.
			if (frames_grabbed.load() < frames_to_grab.load()) {
				if (frames_grabbed == 0) {
					// on the first frame we need to make sure the list is 
					// cleared. This isn't ideal though because I don't know
					// if it will add a time delay.
					frame_list.clear();
				}
				// Note, we need to clone the mat when we add it to the list,
				// otherwise it just inserts a shallow copy to the list and
				// the data isn't valid later.
				frame_list.push_front(mat.clone());
				frames_grabbed.store(frames_grabbed.load()+1);
			}
			else {
				// do nothing, we're done acquiring or not acquiring.
				;
			}
		}

		// Generate a reduced size image for display purposes. How can I use this with the 
		// displayBuffer?
		auto mat_decimated = cv::Mat();
		auto dsize = cv::Size(0, 0);
		cv::resize(mat, mat_decimated, dsize, img_scale_factor, img_scale_factor, cv::INTER_LINEAR);

		// Calculate the FPS to display on the reduced image.
		int fps = (int)round(1.0 / (1e-9 * (double)std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::high_resolution_clock::now()
			- frame_end_time
		).count()));


		/*
		FYI: no easy newline functionality in putText
		https://stackoverflow.com/questions/27647424/opencv-puttext-new-line-character
		*/
		cv::putText(
			mat_decimated,
			std::to_string(fps)
			+ std::string(" fps, ctr: ")
			+ std::to_string(counter),
			cv::Point(10, 30),
			cv::FONT_HERSHEY_SIMPLEX,
			1.0,
			cv::Scalar(0.5, 0.0, 0.0, 1.0)
		);

		//// Update image, I don't think this updates until waitKey is called.			
		cv::imshow("display", mat_decimated);


		// Required to update the opencv imshow. We aren't doing anything
		// here with the actual key value. This is leftover from the exe
		// implementation.
		auto last_key_local = cv::waitKeyEx(1);


		// Does resetting the shared pointer give it back to the queue???
		buffer.reset();

		counter++;
		frame_end_time = std::chrono::high_resolution_clock::now();
	}

private:

	int grabber_width;
	int grabber_height;

};


/*
Sets the external trigger enable.
*/
DLL_EXPORT int DLL_CALLSPEC set_external_trigger_enable(bool);

/*
Main function for the worker thread that handles reading frames from
the queueSink stream and plotting them to the screen.
*/
void example_imagebuffer_opencv_snap();


/*
Main function that initializes the ic4 library, launches the worker 
thread, and closes the ic4 library.
*/
void queueSinkListener_and_opencv_display();


/*
The start_interface function will now serve as a testbench for controlling
the worker thread through simple key presses (_getwch) so we can more easily
transition to a dll interface.
*/
DLL_EXPORT int DLL_CALLSPEC start_interface();



/*
Call this to check the value of init_library_error. Currently this will be -1
before initialization is complete. Then it will be 0 if no error and 1 if
error.
*/
DLL_EXPORT int DLL_CALLSPEC check_for_init_error();


/*
Sets the global stop flag to stop the worker/camera thread.
*/
DLL_EXPORT int DLL_CALLSPEC stop_interface();



/*
Calls worker_thread.join(), this shoul
*/
DLL_EXPORT int DLL_CALLSPEC join_interface();


DLL_EXPORT int DLL_CALLSPEC set_frames_grabbed(size_t val);


DLL_EXPORT size_t DLL_CALLSPEC get_frames_grabbed();


DLL_EXPORT int DLL_CALLSPEC set_frames_to_grab(size_t val);


DLL_EXPORT size_t DLL_CALLSPEC get_frames_to_grab();


DLL_EXPORT int DLL_CALLSPEC print_info_on_frames();


DLL_EXPORT size_t DLL_CALLSPEC get_number_of_frames();


DLL_EXPORT size_t DLL_CALLSPEC get_frame_size_in_bytes();

DLL_EXPORT size_t DLL_CALLSPEC get_image_width();

DLL_EXPORT size_t DLL_CALLSPEC get_image_height();

/*
Reads the oldest frame. The caller should keep calling this until all frames
are read to read one frame at a time. The caller should allocate user_buffer
based on get_frame_size_in_bytes().

@returns 0 if success, -1 if there were no frames in the frame_list.
*/
DLL_EXPORT int DLL_CALLSPEC read_oldest_frame(uint8_t* user_buffer);

/*
Clears the frame list. Use this if acquisition was aborted and you need to
start from an empty frame list without reading all frames.
*/
DLL_EXPORT int DLL_CALLSPEC clear_frame_list();



