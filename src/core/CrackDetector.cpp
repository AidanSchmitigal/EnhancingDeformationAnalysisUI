#include <core/CrackDetector.hpp>

#include <opencv2/opencv.hpp>

void CrackDetector::DetectCracks(const std::vector<uint32_t*>& images, int width, int height, int crack_darkness, int fill_threshold, int sharpness, int resolution, int amount) {
	cv::Mat result;
	for (uint32_t* img_ptr : images) {
		cv::Mat image(height, width, CV_8UC4, img_ptr);

		cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);

		cv::Mat blurred;
		cv::GaussianBlur(image, blurred, cv::Size(5, 5), 0);

		cv::Mat dark_mask;
		cv::inRange(blurred, 0, crack_darkness, dark_mask);

		cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
		cv::Mat dilated;
		cv::dilate(dark_mask, dilated, kernel, cv::Point(-1, -1), fill_threshold);

		cv::Mat inverted;
		cv::bitwise_not(dilated, inverted);

		cv::Mat labels, stats, centroids;
		int num_labels = cv::connectedComponentsWithStats(inverted, labels, stats, centroids);

		cv::Mat filled_img = dilated.clone();
		const int max_hole_area = 20000;
		for (int i = 1; i < num_labels; ++i) {
			int area = stats.at<int>(i, cv::CC_STAT_AREA);
			if (area < max_hole_area) {
				filled_img.setTo(255, labels == i);
			}
		}

		kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
		cv::Mat eroded;
		cv::erode(filled_img, eroded, kernel);

		cv::Mat clean_img = eroded.clone();
		int clean_num_labels = cv::connectedComponentsWithStats(eroded, labels, stats, centroids);
		std::vector<int> areas;
		for (int i = 1; i < clean_num_labels; ++i) {
			int area = stats.at<int>(i, cv::CC_STAT_AREA);
			areas.push_back(area);
		}
		std::sort(areas.begin(), areas.end());
		for (int i = 1; i < clean_num_labels; ++i) {
			int area = stats.at<int>(i, cv::CC_STAT_AREA);
			if (area < areas[std::max(0, (int)areas.size() - amount)]) {
				clean_img.setTo(0, labels == i);
			}
		}

		cv::Mat smooth_mask;
		cv::GaussianBlur(clean_img, smooth_mask, cv::Size(13, 13), 0);
		cv::inRange(smooth_mask, sharpness, 255, smooth_mask);

		std::vector<std::vector<cv::Point>> contours;
		cv::findContours(smooth_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

		std::vector<std::vector<cv::Point>> approx_polygons;
		for (int i = 0; i < std::min((int)contours.size(), amount); ++i) {
			std::vector<cv::Point> approx;
			double epsilon = resolution;
			cv::approxPolyDP(contours[i], approx, epsilon, true);
			approx_polygons.push_back(approx);
		}

		cv::Mat color_img;
		image.copyTo(color_img);
		cv::cvtColor(image, color_img, cv::COLOR_GRAY2BGRA);
		cv::polylines(color_img, approx_polygons, true, cv::Scalar(0, 0, 255), 2);

		result = color_img.clone();
		memcpy(img_ptr, result.data, width * height * 4);
	}
}
