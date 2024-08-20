#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>



#pragma once

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
Sets the global stop flag to stop the worker/camera thread.
*/
DLL_EXPORT int DLL_CALLSPEC stop_interface();



/*
Calls worker_thread.join(), this shoul
*/
DLL_EXPORT int DLL_CALLSPEC join_interface();





