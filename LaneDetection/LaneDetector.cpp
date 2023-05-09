
#include "LaneDetector.h"

LaneDetector::LaneDetector()
	: img_size(0.)
	, img_center(0.)
	, left_m(0.)
	, right_m(0.)
	, left_b(Point(0, 0))
	, right_b(Point(0, 0))
	, left_detect(false)
	, right_detect(false)
	, poly_bottom_width(1)
	, poly_top_width(0.16)
	, poly_height(0.35)
	//, predict_x(0.)
{
}

LaneDetector::~LaneDetector()
{
}

Mat LaneDetector::filterColors(Mat img_frame)
{
	/*
		���/����� ������ ������ ���� �ش�Ǵ� ������ ���͸��Ѵ�.
	*/
	Mat output;
	UMat img_hsv;
	UMat white_mask, white_image;
	UMat yellow_mask, yellow_image;
	img_frame.copyTo(output);

	//���� ���� ���� 
	Scalar lower_white = Scalar(200, 200, 200); //��� ���� (RGB)
	Scalar upper_white = Scalar(255, 255, 255);
	Scalar lower_yellow = Scalar(10, 100, 140); //����� ���� (HSV)
	Scalar upper_yellow = Scalar(40, 255, 255);

	//��� ���͸�
	inRange(output, lower_white, upper_white, white_mask);
	bitwise_and(output, output, white_image, white_mask);

	cvtColor(output, img_hsv, COLOR_BGR2HSV);

	//����� ���͸�
	inRange(img_hsv, lower_yellow, upper_yellow, yellow_mask);
	bitwise_and(output, output, yellow_image, yellow_mask);

	//�� ������ ��ģ��.
	addWeighted(white_image, 1.0, yellow_image, 1.0, 0.0, output);
	return output;
}

Mat LaneDetector::limitRegion(Mat img_edges)
{
	/*
		���� ������ �����ڸ��� �����ǵ��� ����ŷ�Ѵ�.
		���� ������ �����ڸ��� ǥ�õǴ� ���� ������ ��ȯ�Ѵ�.
	*/
	int width = img_edges.cols;
	int height = img_edges.rows;

	Mat output;
	Mat mask = Mat::zeros(height, width, CV_8UC1);

	//���� ���� ���� ���
	Point points[4]{
		Point(width * (1 - poly_bottom_width) / 2, height),
		Point(width * (1 - poly_top_width) / 2, height - height * poly_height),
		Point(width - (width * (1 - poly_top_width)) / 2,
												height - height * poly_height),
		Point(width - (width * (1 - poly_bottom_width)) / 2, height)
	};

	//�������� ���ǵ� �ٰ��� ������ ������ ä�� �׸���.
	fillConvexPoly(mask, points, 4, Scalar(255, 0, 0));

	//����� ��� ���� edges �̹����� mask�� ���Ѵ�.
	bitwise_and(img_edges, mask, output);
	return output;
}

Mat LaneDetector::makeTopView(Mat img_frame)
{
	int width = img_frame.size().width;
	int height = img_frame.size().height;

	const int poly_pts = 4;
	vector<Point2f> points = {
		Point2f(width * (1-poly_top_width) / 2, height - height * poly_height),
		Point2f(width - (width * (1 - poly_top_width)) / 2,
												height - height * poly_height),
		Point2f(width * (1-poly_bottom_width) / 2, height),
		Point2f(width - (width * (1-poly_bottom_width)) / 2, height)
	};
	Size warp_size(width, height);
	Mat img_top(warp_size, img_frame.type());
	//Warping ���� ��ǥ
	vector<Point2f> warp_corners(4);
	warp_corners[0] = Point2f(0, 0);
	warp_corners[1] = Point2f(img_top.cols, 0);
	warp_corners[2] = Point2f(0, img_top.rows);
	warp_corners[3] = Point2f(img_top.cols, img_top.rows);
	//Transformation Matrix ���ϱ�
	Mat trans = getPerspectiveTransform(points, warp_corners);
	//Warping
	warpPerspective(img_frame, img_top, trans, warp_size);
#ifdef DRAW_POINT_TOP
	for (int i = 0; i < points.size(); i++)
	{
		circle(img_frame, points[i], 3, Scalar(0, 255, 0), 3);
	}
#endif // DRAW_POINT_TOP
	return img_top;
}

Mat LaneDetector::makeROI(Mat img_filter)
{
	
	const int rois = 10;  //roi ����
	
	Mat img_gray, img_bin;
	cvtColor(img_filter, img_gray, COLOR_BGR2GRAY);  
	threshold(img_gray, img_bin, 100, 255, ThresholdTypes::THRESH_BINARY);
	cv::Rect l_roi[rois], r_roi[rois];
	float ratio_width = 0.3;
	int subHeight = img_filter.rows / rois;
	int subWidth = img_filter.cols * ratio_width; 

	int l_offset = 10;
	int r_offset = 230;

	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;

	for (size_t k = 0; k < rois; k++)
	{
		l_roi[k].x = 0+ l_offset; 
		r_roi[k].x = img_filter.cols/2 + r_offset; 
		l_roi[k].y = r_roi[k].y = k*subHeight;
		l_roi[k].width = r_roi[k].width = subWidth; 
		l_roi[k].height = r_roi[k].height = subHeight;

		rectangle(img_filter, l_roi[k], Scalar(255, 0, 0), 2); 
		rectangle(img_filter, r_roi[k], Scalar(0, 0, 255), 2);

		Mat subL = img_bin(l_roi[k]);
		findContours(subL, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);
		for (size_t i = 0; i < contours.size(); i++)
		{
			RotatedRect rt = minAreaRect(contours[i]); //�����ϴ� �ּ� ũ�� ���� �簢�� ã��
			
			double area = contourArea(contours[i]);
			if (area < 10)
				continue;

			Point ptCenter = rt.center;
			Point ptCross = Point(ptCenter.x + l_roi[k].x, ptCenter.y + l_roi[k].y);
			drawMarker(img_filter, ptCross, Scalar(255, 0, 255), MARKER_CROSS);
			
		}
		Mat subR = img_bin(r_roi[k]);
		findContours(subR, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);
		for (size_t i = 0; i < contours.size(); i++)
		{
			RotatedRect rt = minAreaRect(contours[i]);

			double area = contourArea(contours[i]);
			if (area < 10)
				continue;
			Point ptCenter = rt.center;
			Point ptCross = Point(ptCenter.x + r_roi[k].x, ptCenter.y + r_roi[k].y);
			drawMarker(img_filter, ptCross, Scalar(255, 0, 255), MARKER_CROSS);
		}
	}
	return img_filter;
}

Mat LaneDetector::drawLine(Mat img_input, vector<Point> lane, string dir)
{
	return Mat();
}