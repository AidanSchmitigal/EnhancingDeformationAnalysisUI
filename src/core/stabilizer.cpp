#include <core/stabilizer.hpp>

#include <opencv2/core.hpp>
#include <opencv2/video/tracking.hpp>

void Stabilizer::stabilize(std::vector<uint32_t *> &frames, int width, int height) {
	std::vector<cv::Mat> cv_frames;
	for (auto frame : frames) {
		cv::Mat cv_frame(width, height, CV_8UC4, frame);
		cv::cvtColor(cv_frame, cv_frame, cv::COLOR_RGBA2GRAY);
		cv_frames.push_back(cv_frame);
	}

	for (int i = 1; i < cv_frames.size(); i++) {
		cv::Mat warp_matrix = cv::Mat::eye(2, 3, CV_32F);
		double r = cv::findTransformECC(cv_frames[0], cv_frames[i], warp_matrix, cv::MOTION_TRANSLATION, cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 1000, 1e-6));
		if (r == -1) {
			printf("Error in finding transformation\n");
			return;
		}
		cv::Mat result = cv::Mat(cv_frames[i].size(), CV_8UC1);
		cv::warpAffine(cv_frames[i], result, warp_matrix, cv_frames[i].size(), cv::INTER_LINEAR + cv::WARP_INVERSE_MAP);
		cv_frames[i] = result;
	}

	for (int i = 0; i < cv_frames.size(); i++) {
		cv::cvtColor(cv_frames[i], cv_frames[i], cv::COLOR_GRAY2RGBA);
		memcpy(frames[i], cv_frames[i].data, width * height * 4);
	}
}
