#include <core/CrackDetector.hpp>

#include <opencv2/opencv.hpp>

void CrackDetector::detectCracks(std::vector<uint32_t*>& frames, int width, int height) {
        if (frames.empty()) return;

        for (auto& ptr : frames) {
                cv::Mat img(height, width, CV_8UC4, ptr);
                cv::Mat gray, edges, cracks;

                // Convert to grayscale
                cv::cvtColor(img, gray, cv::COLOR_RGBA2GRAY);

                // Apply Gaussian blur to reduce noise
                cv::GaussianBlur(gray, gray, cv::Size(5, 5), 1.5);

                // Apply Canny edge detection with adjusted thresholds
                cv::Canny(gray, edges, 100, 200);

                // Morphological operations to enhance cracks
                cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
                cv::morphologyEx(edges, cracks, cv::MORPH_CLOSE, kernel);

                // Highlight detected cracks in red
                cv::Mat result;
                cv::cvtColor(gray, result, cv::COLOR_GRAY2RGBA);
                for (int y = 0; y < result.rows; y++) {
                        for (int x = 0; x < result.cols; x++) {
                                if (cracks.at<uchar>(y, x) > 0) {
                                        result.at<cv::Vec4b>(y, x) = cv::Vec4b(0, 0, 255, 255);
                                }
                        }
                }

                // Copy result back to original frame
                std::memcpy(ptr, result.data, width * height * 4);
        }
}
