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
Last frame's width.
*/
std::atomic<size_t> last_frame_width;

/*
Last frame's height.
*/
std::atomic<size_t> last_frame_height;

/*
Enable or disable the external trigger, this will take effect immediately.
*/
std::atomic<bool> external_trigger_enable = false;


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


DLL_EXPORT int DLL_CALLSPEC set_external_trigger_enable(bool val) {
	external_trigger_enable.store(val);
	return 0;
}



void example_imagebuffer_opencv_snap()
{
	ic4::Error err;

	// Create a new Grabber and open the first device. We should only have
	// one compatible camera connected so this will be safe.
	auto devices = ic4::DeviceEnum::enumDevices();
	ic4::Grabber grabber;
	grabber.deviceOpen(devices.front());

	/*****************************************************************************
	* Configure the camera
	*/ 

	auto map = grabber.devicePropertyMap();

	// Reset all device settings to default
	// Not all devices support this, so ignore possible errors.
	map.setValue(ic4::PropId::UserSetSelector, "Default", ic4::Error::Ignore());
	map.executeCommand(ic4::PropId::UserSetLoad, ic4::Error::Ignore());


	// Enable trigger mode.
	if (external_trigger_enable.load()) {
		if (!map.setValue(ic4::PropId::TriggerMode, "On", err)){
			std::cerr << "Failed to enable trigger mode: " << err.message() << std::endl;
			return;
		}
	}
	

	// Set trigger polarity to RisingEdge (actually called TriggerActivation)
	if (!map.setValue(ic4::PropId::TriggerActivation, "RisingEdge", err)){
		std::cerr << "Failed to set trigger polarity: " << err.message() << std::endl;
		return;
	}

	// Enable IMXLowLatencyMode (actually called IMXLowLatencyTriggerMode)
	if (!map.setValue(ic4::PropId::IMXLowLatencyTriggerMode, "True", err)){
		std::cerr << "Failed to set IMXLowLatencyTriggerMode: " << err.message() << std::endl;
		return;
	}

	// https://www.theimagingsource.com/en-us/documentation/ic4cpp/guide_configuring_device.html
	// https://www.theimagingsource.com/en-us/documentation/ic4cpp/technical_article_properties.html
	grabber.devicePropertyMap().setValue(ic4::PropId::PixelFormat, ic4::PixelFormat::Mono8);


	/*****************************************************************************
	* 
	*/

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

	bool last_external_trigger_enable = external_trigger_enable.load();

	// https://www.asciitable.com/
	int key_code = -1;
	try {
		while (key_code != 27 && !stop_all_flag.load()) {
			//
			if (last_external_trigger_enable != external_trigger_enable.load()) {
				if (external_trigger_enable.load()) {
					if (!map.setValue(ic4::PropId::TriggerMode, "On", err)) {
						std::cerr << "Failed to enable trigger mode: " << err.message() << std::endl;
					}
				}
				else {
					if (!map.setValue(ic4::PropId::TriggerMode, "Off", err)) {
						std::cerr << "Failed to disable trigger mode: " << err.message() << std::endl;
					}
				}
				last_external_trigger_enable = external_trigger_enable.load();
			}
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


DLL_EXPORT size_t DLL_CALLSPEC get_frames_grabbed() {
	return frames_grabbed.load();
}


DLL_EXPORT int DLL_CALLSPEC set_frames_to_grab(size_t val) {
	{
		const std::lock_guard<std::mutex> lock(camera_frame_acq_mutex);
		frames_to_grab.store(val);
	}
	return 0;
}


DLL_EXPORT size_t DLL_CALLSPEC get_frames_to_grab() {
	return frames_to_grab.load();
}



DLL_EXPORT int DLL_CALLSPEC print_info_on_frames() {
	size_t num_frames = frame_list.size();
	std::cout << "num_frames = " << num_frames << std::endl;
	//
	if (num_frames > 0) {
		cv::Mat first_frame = frame_list.front();
		cv::Size msz = first_frame.size();
		std::cout << "    first_frame.size().width = " << msz.width << std::endl;
		std::cout << "    first_frame.size().height = " << msz.height << std::endl;

		/*
		We'll just handle the basic types here, otherwise I will
		write a python script to handle generating this code.

		#define CV_8U   0
		#define CV_8S   1
		#define CV_16U  2
		#define CV_16S  3
		#define CV_32S  4
		#define CV_32F  5
		#define CV_64F  6
		#define CV_16F  7

		*/

		int tmp_type = first_frame.type();
		std::string type_txt("UNSET");
		if (tmp_type == CV_8U) {
			type_txt.clear();
			type_txt.append("CV_8U");
		}
		else if (tmp_type == CV_8S) {
			type_txt.clear();
			type_txt.append("CV_8S");
		}
		else if (tmp_type == CV_16U) {
			type_txt.clear();
			type_txt.append("CV_16U");
		}
		else if (tmp_type == CV_16S) {
			type_txt.clear();
			type_txt.append("CV_16S");
		}
		else if (tmp_type == CV_32S) {
			type_txt.clear();
			type_txt.append("CV_32S");
		}
		else if (tmp_type == CV_32F) {
			type_txt.clear();
			type_txt.append("CV_32F");
		}
		else if (tmp_type == CV_64F) {
			type_txt.clear();
			type_txt.append("CV_64F");
		}
		else if (tmp_type == CV_16F) {
			type_txt.clear();
			type_txt.append("CV_16F");
		}

		std::cout << "    first_frame.type() = " << type_txt << std::endl;

		// Find the size of the frame in bytes
		// https://stackoverflow.com/questions/26441072/finding-the-size-in-bytes-of-cvmat
		size_t sizeInBytes = first_frame.step[0] * first_frame.rows;
		std::cout << "sizeInBytes = " << sizeInBytes << std::endl;

		// Check if the cv::mat is continuous (it should be...)
		std::cout << "first_frame.isContinuous() = " << first_frame.isContinuous() << std::endl;

	}
	return 0;
}


// TODO: figure out how to copy the data out of the cv::mat into python.
// https://stackoverflow.com/questions/26681713/convert-mat-to-array-vector-in-opencv
//


DLL_EXPORT size_t DLL_CALLSPEC get_number_of_frames() {
	return frame_list.size();
}


DLL_EXPORT size_t DLL_CALLSPEC get_frame_size_in_bytes() {
	size_t num_frames = frame_list.size();
	//
	if (num_frames > 0) {
		cv::Mat first_frame = frame_list.front();
		size_t sizeInBytes = first_frame.step[0] * first_frame.rows;
		return sizeInBytes;
	}
	else {
		return 0;
	}
}


DLL_EXPORT size_t DLL_CALLSPEC get_image_width() {
	return last_frame_width.load();
}


DLL_EXPORT size_t DLL_CALLSPEC get_image_height() {
	return last_frame_height.load();
}


DLL_EXPORT int DLL_CALLSPEC read_oldest_frame(uint8_t* user_buffer) {
	size_t num_frames = frame_list.size();
	if (num_frames > 0) {
		// Get the last (oldest) frame (we push_front when acquiring data).
		cv::Mat frame = frame_list.back();
		// Copy the frame into the user buffer. The user should have used
		// get_frame_size_in_bytes() to prepare this buffer.
		//
		// TODO: figure out a better solution than calling get_frame_size_in_bytes()
		// each time.
		memcpy((void*)user_buffer, (void*)frame.data, get_frame_size_in_bytes());
		// Pop the last frame now that we're done reading it.
		frame_list.pop_back();
		return 0;
	}
	else {
		return -1;
	}
}


DLL_EXPORT int DLL_CALLSPEC clear_frame_list() {
	frame_list.clear();
	return 0;
}










