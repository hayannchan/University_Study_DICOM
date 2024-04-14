#include <iostream>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/core/mat.hpp>
#include "dcmtk/dcmimgle/dcmimage.h"
#include "dcmtk/dcmimage/diregist.h"
#include <filesystem>
#pragma comment(lib, "ws2_32.lib")
#define _CRT_SECURE_NO_WARNINGS
#define OPENCV_TRAITS_ENABLE_DEPRECATED

using namespace std;
namespace stdfs = std::filesystem;


cv::Mat prepare_image(string file)
{
	cv::Mat dst;

	DicomImage* image = new DicomImage(file.c_str());
	int nWidth = image->getWidth();
	int nHeight = image->getHeight();
	int depth = image->getDepth();
	//cout << "Size:" << nWidth << ", " << nHeight << endl;
	//cout << "Depth:" << depth << endl;
	cout << "Pixel Representation:" << image->getInterData()->getRepresentation() << endl;

	image->setWindow(100, 400);
	if (image != NULL)
	{
		if (image->getStatus() == EIS_Normal)
		{
			Uint8* pixelData = (Uint8*)(image->getOutputData(8));
			if (pixelData != NULL)
			{
				dst = cv::Mat(nHeight, nWidth, CV_8UC3, pixelData);
				/*imshow("image2", dst);
				cv::waitKey(0);
				system("pause");*/
			}
		}
		else
			cerr << "Error: cannot load DICOM image (" << DicomImage::getString(image->getStatus()) << ")" << endl;
	}
	return dst;
}

int main(int argc, char** argv)
{
	cv::Mat dst;
	vector<string> files;
	int i = 0;
	stdfs::path path = "marked";

	const stdfs::directory_iterator end{};
	for (stdfs::directory_iterator iter{ path }; iter != end; ++iter)
		files.push_back(string(iter->path().string()));
	
	sort(files.begin(), files.end());
	
	while (i <= files.size())
	{
		dst = prepare_image(files[i]);
		int nHeight = dst.rows;
		int nWidth = dst.cols;

		string name = "suspected";                                // + std::to_string(i);
		cv::imshow(name, dst);
        cout << files[i] <<  endl;
		int k = cv::waitKey(0);
		if (k == 120) //z
			i--;
		if (k == 122) //x
			i++;
        cout <<  k << endl;
		if (k == 27) //Esc
			break;
	}
	return 0;
}
