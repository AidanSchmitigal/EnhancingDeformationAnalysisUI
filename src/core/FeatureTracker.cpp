#include <core/FeatureTracker.hpp>

#include <opencv2/opencv.hpp>

std::vector<std::pair<TrackingPoint, TrackingPoint>> FeatureTracker::TrackFeatures(
		const std::vector<uint32_t*>& imageSequence,
		int width,
		int height,
		TrackingPoint point1,
		TrackingPoint point2
		) {
	std::vector<std::pair<TrackingPoint, TrackingPoint>> results;
	std::vector<cv::Point2f> points = {
		cv::Point2f(point1.x, point1.y),
		cv::Point2f(point2.x, point2.y)
	};

	// Store initial points
	results.push_back({point1, point2});

	// Convert first image and prepare for tracking
	cv::Mat prevFrame(height, width, CV_8UC4, imageSequence[0]);
	cv::Mat prevFrameGray;
	cv::cvtColor(prevFrame, prevFrameGray, cv::COLOR_BGRA2GRAY);

	std::vector<uchar> status;
	std::vector<float> err;

	// Track points through sequence
	for (size_t i = 1; i < imageSequence.size(); ++i) {
		cv::Mat currentFrame(height, width, CV_8UC4, imageSequence[i]);
		cv::Mat frameGray;
		cv::cvtColor(currentFrame, frameGray, cv::COLOR_BGRA2GRAY);

		std::vector<cv::Point2f> nextPoints;
		cv::calcOpticalFlowPyrLK(
				prevFrameGray, frameGray, points, nextPoints,
				status, err,
				cv::Size(15, 15), 2,
				cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 10, 0.03)
				);

		points = nextPoints;
		results.push_back({
				{points[0].x, points[0].y},
				{points[1].x, points[1].y}
				});

		prevFrameGray = frameGray;
	}

	return results;
}
