


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

//int main() {
//	int width, height;
//	getScreenResolution(width, height);
//	printf("Screen resolution: %dx%d\n", width, height);
//}


void example_imagebuffer_opencv_snap()
{
	//// Let the user select a device
	//auto devices = ic4::DeviceEnum::enumDevices();
	//auto it = ic4_examples::console::select_from_list(devices);
	//if( it == devices.end() )
	//	return;

	// Create a new Grabber and open the first device. We should only have
	// one compatible camera connected so this will be safe.
	auto devices = ic4::DeviceEnum::enumDevices();
	ic4::Grabber grabber;
	grabber.deviceOpen(*devices.begin());

	// Create an OpenCV display window
	std::cout << "opening window" << std::endl;
	cv::namedWindow("display", cv::WINDOW_GUI_EXPANDED);

	int scrn_width, scrn_height;
	getScreenResolution(scrn_width, scrn_height);


	cv::moveWindow("display", scrn_width-300, scrn_height-300);
	//cv::setWindowProperty("display", cv::WND_PROP_FULLSCREEN, cv::WINDOW_GUI_EXPANDED);
	//cv::setWindowProperty("display", cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);

	// Create a sink that converts the data to something that OpenCV can work with (e.g. BGR8)
	auto sink = ic4::SnapSink::create(ic4::PixelFormat::Mono8);
	grabber.streamSetup(sink);

	try {
		for (int i = 0; i < 100; ++i) {
			/*std::cout << "Focus display window and press any key to continue..." << std::endl;
			cv::waitKey(100);*/

			// Snap image from running data stream
			//std::cout << "auto buffer = sink->snapSingle(1000);" << std::endl;
			auto buffer = sink->snapSingle(1000);

			// Create a cv::Mat pointing into the BGR8 buffer
			//std::cout << "auto mat = ic4interop::OpenCV::wrap(*buffer);" << std::endl;
			auto mat = ic4interop::OpenCV::copy(*buffer);

			// Generate a reduced size image for display purposes.
			auto mat_decimated = cv::Mat();
			auto dsize = cv::Size(0,0);
			cv::resize(mat, mat_decimated, dsize, 0.1, 0.1, cv::INTER_LINEAR);

			//std::cout << "Displaying captured image" << std::endl;
			//cv::imshow("display", mat);

			//std::cout << "Focus display window and press any key to continue..." << std::endl;;
			cv::waitKey(1);

			//// Blur the image in place (this still uses the ImageBuffer's memory)
			//cv::blur(mat, mat, cv::Size(75, 75));

			//std::cout << "Displaying blurred image" << std::endl;
			cv::imshow("display", mat_decimated);
			
			std::cout << i << std::endl;
		}
		std::cout << "grabber.streamStop();" << std::endl;
		grabber.streamStop();
	}
	catch (...) {
		std::cout << "Error, trying to stop stream and exit [grabber.streamStop();]" << std::endl;
		grabber.streamStop();
	}
}

int main()
{
	

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