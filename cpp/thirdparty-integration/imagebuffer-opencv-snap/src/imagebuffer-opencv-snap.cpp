


#include <opencv2/opencv.hpp>

#include <ic4/ic4.h>
#include <ic4-interop/interop-OpenCV.h>

#include <console-helper.h>

#include <iostream>


#define WIN32 1

#if WIN32
#include <windows.h>
#else
#include <X11/Xlib.h>
#endif


// file:///C:/Program%20Files/The%20Imaging%20Source%20Europe%20GmbH/ic4/share/theimagingsource/ic4/doc/cpp/classic4_1_1_queue_sink.html
// file:///C:/Program%20Files/The%20Imaging%20Source%20Europe%20GmbH/ic4/share/theimagingsource/ic4/doc/cpp/classic4_1_1_queue_sink_listener.html

/*
Custom subclass of QueueSinkListener to handle interfacing with a QueueSink.
*/
class customQueueSinkListener : public ic4::QueueSinkListener {
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
		auto buffer = sink.popOutputBuffer();
		// Do something with the buffer
		
		double img_scale_factor = 0.2;

		// Can I do this opencv stuff in this callback?

		// Create a cv::Mat
		auto mat = ic4interop::OpenCV::copy(*buffer);

		// Generate a reduced size image for display purposes. How can I use this with the 
		// displayBuffer?
		auto mat_decimated = cv::Mat();
		auto dsize = cv::Size(0,0);
		cv::resize(mat, mat_decimated, dsize, img_scale_factor, img_scale_factor, cv::INTER_LINEAR);


		// Update image, I don't think this updates until waitKey is called.			
		cv::imshow("display", mat_decimated);

		


		// Does resetting the shared pointer give it back to the queue???
		buffer.reset();
	}
};


//https://stackoverflow.com/questions/11041607/getting-screen-size-on-opencv


void getScreenResolution(int& width, int& height) {
#if WIN32
	width = (int)GetSystemMetrics(SM_CXSCREEN);
	height = (int)GetSystemMetrics(SM_CYSCREEN);
#else
	Display* disp = XOpenDisplay(NULL);
	Screen* scrn = DefaultScreenOfDisplay(disp);
	width = scrn->width;
	height = scrn->height;
#endif
}

/*
Prints the steam statistics.
*/
void print_streamStatistics(ic4::Grabber::StreamStatistics stats) {
	std::string out_str("");
	std::cout
		<< "DEVICE: "
		<< stats.device_underrun
		<< "/"
		<< stats.device_transmission_error
		<< "/"
		<< stats.device_delivered;
	std::cout
		<< ";  sink: "
		<< stats.sink_underrun
		<< "/"
		<< stats.sink_ignored
		<< "/"
		<< stats.sink_delivered;
	std::cout
		<< ";  transform: "
		<< stats.transform_underrun
		<< "/"
		<< stats.transform_delivered
		<< std::endl;
}


void example_imagebuffer_opencv_snap()
{

	// Create a new Grabber and open the first device. We should only have
	// one compatible camera connected so this will be safe.
	auto devices = ic4::DeviceEnum::enumDevices();
	ic4::Grabber grabber;
	grabber.deviceOpen(devices.front());

	// Configure the camera
	// https://www.theimagingsource.com/en-us/documentation/ic4cpp/guide_configuring_device.html
	// https://www.theimagingsource.com/en-us/documentation/ic4cpp/technical_article_properties.html
	grabber.devicePropertyMap().setValue(ic4::PropId::PixelFormat, ic4::PixelFormat::Mono8);


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
		scrn_width + cv_window_extra_width_offset - (int)(img_scale_factor * (double)std::stoi(grabber.devicePropertyMap().getValueString(ic4::PropId::Width))),
		scrn_height + cv_window_extra_height_offset - (int)(img_scale_factor * (double)std::stoi(grabber.devicePropertyMap().getValueString(ic4::PropId::Height)))
	);


	// Create a sink that converts the data to something that OpenCV can work with (e.g. BGR8)
	std::cout << "auto listener = customQueueSinkListener();" << std::endl;
	auto listener = customQueueSinkListener();
	std::cout << "auto sink = ic4::QueueSink::create(listener, ic4::PixelFormat::Mono8);" << std::endl;
	auto sink = ic4::QueueSink::create(listener, ic4::PixelFormat::Mono8);
	std::cout << "grabber.streamSetup(sink);" << std::endl;
	grabber.streamSetup(sink);

	int time_to_wait_ms = 1000;

	// https://www.asciitable.com/
	int key_code = -1;

	try {
		while (key_code != 27) {
			// make the window update (required).
			//  It returns the code of the pressed key or -1 if no key was pressed before the specified time had elapsed.
			key_code = cv::waitKeyEx(1);
		}
		std::cout << "grabber.streamStop();" << std::endl;
		grabber.streamStop();
	}
	catch (...) {
		std::cout << "Error, trying to stop stream and exit [grabber.streamStop();]" << std::endl;
		grabber.streamStop();
	}
	
	



	//try {
	//	for (int i = 0; i < 100; ++i) {
	//		// Snap image from running data stream. How do I check if the buffer is valid?
	//		//std::cout << "auto buffer = sink->snapSingle(1000);" << std::endl;
	//		auto buffer = sink->snapSingle(1000);

	//		// Create a cv::Mat
	//		auto mat = ic4interop::OpenCV::copy(*buffer);

	//		// Generate a reduced size image for display purposes. How can I use this with the 
	//		// displayBuffer?
	//		auto mat_decimated = cv::Mat();
	//		auto dsize = cv::Size(0,0);
	//		cv::resize(mat, mat_decimated, dsize, img_scale_factor, img_scale_factor, cv::INTER_LINEAR);


	//		// Update image, I don't think this updates until waitKey is called.			
	//		cv::imshow("display", mat_decimated);

	//		// make the window update (required).
	//		cv::waitKey(1);
	//		
	//		auto stats = grabber.streamStatistics();
	//		print_streamStatistics(stats);

	//		// Debug, display loop iter.
	//		std::cout << "[" << i << "]" << std::endl;
	//	}
	//	std::cout << "grabber.streamStop();" << std::endl;
	//	grabber.streamStop();
	//}
	//catch (...) {
	//	std::cout << "Error, trying to stop stream and exit [grabber.streamStop();]" << std::endl;
	//	grabber.streamStop();
	//}




}

int main()
{
	//ic4::exitLibrary();

	// Startup like the demo app. Sometimes when the camera goes unresponsive the 
	// previous method failed to start, while the demo app worked. I'm not sure
	// why.
	ic4::InitLibraryConfig conf = {};
	conf.apiLogLevel = ic4::LogLevel::Warning;
	conf.logTargets = ic4::LogTarget::WinDebug;
	conf.defaultErrorHandlerBehavior = ic4::ErrorHandlerBehavior::Throw;
	std::cout << "ic4::initLibrary(conf);" << std::endl;
	ic4::initLibrary(conf);

	example_imagebuffer_opencv_snap();

	std::cout << "ic4::exitLibrary();" << std::endl;
	ic4::exitLibrary();
}