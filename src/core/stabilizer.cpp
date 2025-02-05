#include <core/stabilizer.hpp>

#include <opencv2/opencv.hpp>

void Stabilizer::stabilize_old(std::vector<uint32_t *> &frames, int width, int height) {
	std::vector<cv::Mat> cv_frames;
	for (auto frame : frames) {
		cv::Mat cv_frame(width, height, CV_8UC4, frame);
		cv::cvtColor(cv_frame, cv_frame, cv::COLOR_RGBA2GRAY);
		cv_frames.push_back(cv_frame);
	}

	for (int i = 1; i < cv_frames.size(); i++) {
		cv::Mat warp_matrix = cv::Mat::eye(2, 3, CV_32F);
		double r = cv::findTransformECC(cv_frames[0], cv_frames[i], warp_matrix, cv::MOTION_TRANSLATION, cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 1000, 1e-5));
		if (r == -1) {
			printf("Error in finding transformation\n");
			return;
		}
		cv::Mat result = cv::Mat(cv_frames[i].size(), CV_8UC1);
		cv::warpAffine(cv_frames[i], result, warp_matrix, cv_frames[i].size(), cv::INTER_LINEAR + cv::WARP_INVERSE_MAP);
		cv_frames[i] = result;
	}

	for (int i = 0; i < cv_frames.size(); i++) {
		//cv::cvtColor(cv_frames[i], cv_frames[i], cv::COLOR_GRAY2RGBA);
		for (int j = 0; j < width * height; j++) {
			frames[i][j] = 0xFF000000 | (cv_frames[i].data[j] << 16) | (cv_frames[i].data[j] << 8) | cv_frames[i].data[j];
		}
		//memcpy(frames[i], cv_frames[i].data, width * height * 4);
	}
}

void Stabilizer::Stabilize(std::vector<uint32_t*>& frames, int width, int height) {
    if (frames.empty()) return;
    
    std::vector<cv::Mat> mats;
    for (auto& ptr : frames) {
        cv::Mat img(height, width, CV_8UC4, ptr); // Assuming RGBA format
        mats.push_back(img.clone()); // Clone to avoid modifying original memory
    }
    
    std::vector<cv::Mat> stabilizedFrames;
    cv::Mat refGray, currGray, transformMatrix;
    
    cv::cvtColor(mats[0], refGray, cv::COLOR_RGBA2GRAY);
    stabilizedFrames.push_back(mats[0].clone());
    
    for (size_t i = 1; i < mats.size(); i++) {
        cv::cvtColor(mats[i], currGray, cv::COLOR_RGBA2GRAY);
        
        std::vector<cv::Point2f> refPts, currPts;
        std::vector<uchar> status;
        std::vector<float> err;
        
        cv::goodFeaturesToTrack(refGray, refPts, 200, 0.01, 30);
        if (refPts.empty()) {
            stabilizedFrames.push_back(mats[i].clone());
            continue; // Skip if no features found
        }

        cv::calcOpticalFlowPyrLK(refGray, currGray, refPts, currPts, status, err);
        
        std::vector<cv::Point2f> filteredRef, filteredCurr;
        for (size_t j = 0; j < status.size(); j++) {
            if (status[j]) {
                filteredRef.push_back(refPts[j]);
                filteredCurr.push_back(currPts[j]);
            }
        }
        
        if (filteredRef.size() >= 4) {
            transformMatrix = cv::estimateAffinePartial2D(filteredCurr, filteredRef); // Reverse order to align to ref frame
            if (!transformMatrix.empty()) {
                transformMatrix.convertTo(transformMatrix, CV_64F); // Ensure correct format
            }
        }
        
        // Apply transformation
        cv::Mat stabilized;
        cv::warpAffine(mats[i], stabilized, transformMatrix, mats[i].size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT);
        stabilizedFrames.push_back(stabilized);
    }
    
    for (size_t i = 0; i < frames.size(); i++) {
        std::memcpy(frames[i], stabilizedFrames[i].data, width * height * 4);
    }
}
