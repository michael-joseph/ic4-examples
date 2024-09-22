#include "pch.h"



std::atomic<int> last_key = -1;
std::atomic<bool> stop_all_flag = false;
/*
Worker thread started in start_interface() and joined in stop_interface().
*/
std::thread worker_thread;

std::atomic<int> init_library_error = -1;


/*
Mutex used to access all camera acquisition data frames_grabbed,
frames_to_grab, frame_list, etc.
*/
std::mutex camera_frame_acq_mutex;

/*
Number of frames that have been grabbed and saved in the frame_list.
This is directly set by the dll interface back to 0 to start acquiring
data on the next frame.
*/
std::atomic<size_t> frames_grabbed = 0;

/*
Number of frames that we will grab before finishing one acquisition.
*/
std::atomic<size_t> frames_to_grab = 0;

/*
List of frames that we have grabbed in the current acquisition. This
will be a list of cv::Mat objects. Each cv::Mat is created when we call
ic4interop::OpenCV::wrap().
*/
std::list<cv::Mat> frame_list;



/*
Gets the size of the screen (not tested on multi-monitor setups).

This is used in the customQueueSinkListener() class to position the
opencv window.

https://stackoverflow.com/questions/11041607/getting-screen-size-on-opencv
*/
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

	// Create a sink that converts the data to something that OpenCV can work with (e.g. BGR8)
	std::cout << "auto listener = customQueueSinkListener();" << std::endl;
	auto listener = customQueueSinkListener(
		std::stoi(grabber.devicePropertyMap().getValueString(ic4::PropId::Width)),
		std::stoi(grabber.devicePropertyMap().getValueString(ic4::PropId::Height))
	);
	std::cout << "auto sink = ic4::QueueSink::create(listener, ic4::PixelFormat::Mono8);" << std::endl;
	auto sink = ic4::QueueSink::create(listener, ic4::PixelFormat::Mono8);
	std::cout << "grabber.streamSetup(sink);" << std::endl;
	grabber.streamSetup(sink);

	// https://www.asciitable.com/
	int key_code = -1;

	try {
		while (key_code != 27 && !stop_all_flag.load()) {
			// make the window update (required).
			//  It returns the code of the pressed key or -1 if no key was pressed before the specified time had elapsed.
			key_code = last_key.load();
			Sleep(30); // ms
		}
		std::cout << "grabber.streamStop();" << std::endl;
		grabber.streamStop();
	}
	catch (...) {
		std::cout << "Error, trying to stop stream and exit [grabber.streamStop();]" << std::endl;
		grabber.streamStop();
	}
}


/*
Function for running all camera code so that this can be easily
called in a separate thread later in a dll.
*/
void queueSinkListener_and_opencv_display() {
	// Startup like the demo app. Sometimes when the camera goes unresponsive the 
	// previous method failed to start, while the demo app worked. I'm not sure
	// why.
	int init_err;
	ic4::InitLibraryConfig conf = {};
	conf.apiLogLevel = ic4::LogLevel::Warning;
	conf.logTargets = ic4::LogTarget::WinDebug;
	conf.defaultErrorHandlerBehavior = ic4::ErrorHandlerBehavior::Throw;
	std::cout << "ic4::initLibrary(conf);" << std::endl;
	init_err = ic4::initLibrary(conf);

	init_library_error.store(init_err);

	example_imagebuffer_opencv_snap();

	std::cout << "ic4::exitLibrary();" << std::endl;
	ic4::exitLibrary();
}



/*
The start_interface function will now serve as a testbench for controlling 
the worker thread through simple key presses (_getwch) so we can more easily
transition to a dll interface.
*/
DLL_EXPORT int DLL_CALLSPEC start_interface()
{
	worker_thread = std::thread{ queueSinkListener_and_opencv_display };
	return 0;
}

/*
Call this to check the value of init_library_error. Currently this will be -1
before initialization is complete. Then it will be 0 if no error and 1 if 
error.
*/
DLL_EXPORT int DLL_CALLSPEC check_for_init_error()
{
	return init_library_error.load();
}

/*
Sets the global stop flag to stop the worker/camera thread.
*/
DLL_EXPORT int DLL_CALLSPEC stop_interface()
{
	stop_all_flag.store(true);
	return 0;
}


/*
Calls worker_thread.join(), this should be called when the caller
is prepared to wait until the worker thread is done, usually after
already calling stop_interface().
*/
DLL_EXPORT int DLL_CALLSPEC join_interface() {
	stop_all_flag.store(true);
	worker_thread.join();
	return 0;
}


DLL_EXPORT int DLL_CALLSPEC set_frames_grabbed(size_t val) {
	{
		const std::lock_guard<std::mutex> lock(camera_frame_acq_mutex);
		frames_grabbed.store(val);
	}
	return 0;
}


DLL_EXPORT int DLL_CALLSPEC get_frames_grabbed(size_t val) {
	return frames_grabbed.load();
}


DLL_EXPORT int DLL_CALLSPEC set_frames_to_grab(size_t val) {
	{
		const std::lock_guard<std::mutex> lock(camera_frame_acq_mutex);
		frames_to_grab.store(val);
	}
	return 0;
}


DLL_EXPORT int DLL_CALLSPEC get_frames_to_grab(size_t val) {
	return frames_to_grab.load();
}














