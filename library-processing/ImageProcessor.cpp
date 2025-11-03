#include "ImageProcessor.h"
#include <opencv2/imgproc.hpp>

cv::Mat ImageProcessor::segmentImage(const cv::Mat& input) {
    cv::Mat hsv, mask;
    cv::cvtColor(input, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv, cv::Scalar(35, 100, 100), cv::Scalar(85, 255, 255), mask);
    return mask;
}
