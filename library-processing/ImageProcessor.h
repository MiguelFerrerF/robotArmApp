#ifndef IMAGEPROCESSOR_H
#define IMAGEPROCESSOR_H

#include <opencv2/opencv.hpp>

class ImageProcessor
{
public:
  // Método estático para segmentar una imagen (color verde, por ejemplo)
  static cv::Mat segmentImage(const cv::Mat& input);
};

#endif // IMAGEPROCESSOR_H
