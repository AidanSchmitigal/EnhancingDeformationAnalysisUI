#include <core/ImageAnalysis.hpp>

#include <utils.h>

#include <opencv2/opencv.hpp>

void ImageAnalysis::AnalyzeImages(std::vector<uint32_t *> &frames, int width,
				  int height,
				  std::vector<std::vector<float>> &histograms,
				  std::vector<float> &avg_histogram,
				  std::vector<float> &snrs, float &avg_snr) {
	PROFILE_FUNCTION();

	for (int i = 0; i < frames.size(); i++) {
		histograms.push_back(std::vector<float>());
		cv::Mat img(width, height, CV_8UC4, frames[i]);
		cv::cvtColor(img, img, cv::COLOR_BGRA2GRAY);
		cv::Scalar mean, stddev;
		cv::meanStdDev(img, mean, stddev);
		cv::Scalar snr = mean[0] / stddev[0];
		snrs.push_back(snr[0]);
		avg_snr += snr[0];

		int bins = 256;
		histograms[i].resize(bins);
		avg_histogram.resize(bins);
		cv::Mat hist;
		float range[] = {0, 256};
		const float *histRange = {range};
		bool uniform = true, accumulate = false;
		cv::calcHist(&img, 1, 0, cv::Mat(), hist, 1, &bins, &histRange,
			     uniform, accumulate);
		// Normalize for display
		cv::normalize(hist, hist, 0, 1, cv::NORM_MINMAX);

		for (int j = 0; j < bins; j++) {
			histograms[i][j] = hist.at<float>(j);
			avg_histogram[j] += hist.at<float>(j);
		}
	}
	for (int j = 0; j < avg_histogram.size(); j++) {
		avg_histogram[j] /= frames.size();
	}
	avg_snr = frames.size() != 0 ? avg_snr / frames.size() : 0;
}
