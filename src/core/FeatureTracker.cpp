#include <core/FeatureTracker.hpp>

#include <opencv2/opencv.hpp>

void FeatureTracker::TrackFeatures(const std::vector<uint32_t*>& images, std::vector<cv::Point2f>& points, std::vector<std::vector<cv::Point2f>>& trackedPoints, int width, int height) {
	if (images.empty() || points.size() != 2) return; // Expect exactly 2 points

	cv::Mat prevGray, currGray;
	std::vector<cv::Point2f> prevPts, currPts;
	cv::TermCriteria criteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 50, 0.001);
	trackedPoints.clear();

	// Convert first image from uint32_t* to cv::Mat (assuming BGRA format)
	cv::Mat firstImage(height, width, CV_8UC4, images[0]);
	cv::Mat displayImage = firstImage.clone();
	cv::cvtColor(firstImage, prevGray, cv::COLOR_BGRA2GRAY);

	// Refine each point separately with its own mask
	int radius = 10; // Adjust radius as needed
	prevPts.resize(2); // Pre-allocate for 2 points
	for (int i = 0; i < 2; i++) {
		// Create a mask for this point only
		cv::Mat mask = cv::Mat::zeros(height, width, CV_8U);
		cv::circle(mask, points[i], radius, 255, -1); // Mask around this point

		// Find one good feature in this region
		std::vector<cv::Point2f> refinedPts;
		cv::goodFeaturesToTrack(prevGray, refinedPts, 1, 0.01, 10, mask, 3, false, 0.04); // maxCorners = 1
		if (refinedPts.empty()) {
			prevPts[i] = points[i]; // Fallback to user point if no feature found
		} else {
			prevPts[i] = refinedPts[0]; // Use the single refined point
		}
	}
	trackedPoints.push_back(prevPts);

	// Draw initial points and distance on first image
	float initialDist = cv::norm(prevPts[0] - prevPts[1]);
	for (int i = 0; i < 2; i++) {
		cv::circle(displayImage, prevPts[i], 5, cv::Scalar(0, 255, 0, 255), -1);
	}
	std::string initialText = "Initial Dist: " + std::to_string(initialDist) + " px";
	cv::putText(displayImage, initialText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255, 255), 2);
	memcpy(images[0], displayImage.data, width * height * 4); // Write back to original buffer

	// Track through sequence
	for (size_t i = 1; i < images.size(); i++) {
		cv::Mat currentImage(height, width, CV_8UC4, images[i]);
		cv::cvtColor(currentImage, currGray, cv::COLOR_BGRA2GRAY);

		std::vector<uchar> status;
		std::vector<float> err;
		cv::calcOpticalFlowPyrLK(prevGray, currGray, prevPts, currPts, status, err, cv::Size(21, 21), 3, criteria);

		std::vector<cv::Point2f> framePoints;
		for (int j = 0; j < 2; j++) {
			if (status[j]) framePoints.push_back(currPts[j]);
			else framePoints.push_back(cv::Point2f(-1, -1));
		}
		trackedPoints.push_back(framePoints);

		cv::swap(prevGray, currGray);
		prevPts = currPts;
	}

	// Annotate last image with final points, vectors, and distance
	cv::Mat finalImage(height, width, CV_8UC4, images.back());
	cv::Mat finalDisplay = finalImage.clone();
	std::vector<cv::Point2f>& finalPts = trackedPoints.back();
	if (finalPts[0].x >= 0 && finalPts[1].x >= 0) {
		float finalDist = cv::norm(finalPts[0] - finalPts[1]);
		for (int i = 0; i < 2; i++) {
			cv::circle(finalDisplay, finalPts[i], 5, cv::Scalar(0, 255, 0, 255), -1);
			cv::arrowedLine(finalDisplay, prevPts[i], finalPts[i], cv::Scalar(0, 0, 255, 255), 2);
		}
		std::string finalText = "Final Dist: " + std::to_string(finalDist) + " px";
		cv::putText(finalDisplay, finalText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255, 255), 2);
		memcpy(images[images.size() - 1], finalDisplay.data, width * height * 4); // Write back
	}
}
