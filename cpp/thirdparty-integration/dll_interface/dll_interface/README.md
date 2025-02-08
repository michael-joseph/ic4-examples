These files are meant to be copied into the "https://github.com/TheImagingSource/ic4-examples" repository.
Once you clone the repository, extract the dll_interface.zip file into "cpp/thirdparty-integration".

- For example, on my machine this is the folder structure:
	- "C:\Users\ohns-user\Documents\GitHub\ic4-examples\cpp\thirdparty-integration"
		- "C:\Users\ohns-user\Documents\GitHub\ic4-examples\cpp\thirdparty-integration\dll_interface"
			- These are the files that I've written that you need to copy.
			- This folder contains two folders:
				- "C:\Users\ohns-user\Documents\GitHub\ic4-examples\cpp\thirdparty-integration\dll_interface\dll_interface"
					- The MSVC project for building the dll.
				- "C:\Users\ohns-user\Documents\GitHub\ic4-examples\cpp\thirdparty-integration\dll_interface\py"
					- The python testbenches.
	- "C:\Users\ohns-user\Documents\GitHub\ic4-examples\cpp\thirdparty-integration\imagebuffer-opencv-snap"
		- These are files included in the ic4-examples (not used, but included in this explaination for reference).


The main MSVC solution for building the dll: "dll_interface\dll_interface\dll_interface.sln"

Python testbench that calls the dll: "dll_interface\py\x20241027_tb_0.py"
- Other testbenches are in this directory.

Other requirements:
- OpenCV
	https://github.com/opencv/opencv/releases
	- Install opencv 4.9.0 to "C:\mjs\opencv-4.9.0" or modify the MSVC project settings to your own directory.
