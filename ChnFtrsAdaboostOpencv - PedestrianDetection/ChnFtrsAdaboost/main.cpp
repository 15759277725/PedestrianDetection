// ChnFtrsAdaboost.cpp : �������̨Ӧ�ó������ڵ㡣FPDW��
//

#include "stdafx.h"
/*#include <vld.h>*/
#include "PedDetector.h"

void DetectionExample();
int main(int argc, _TCHAR* argv[])
{
	DetectionExample();

	return 0;
}

void DetectionExample()
{
	// ʵ�������˼����
	PedDetector PD;
	// �������˼�������
	PD.loadStrongClassifier("ClassifierOut.txt");
	// ��ȡ�����ͼ��
	IplImage *img = cvLoadImage("D:\\test\\I00016.png");
	// ��Ⲣ��������ʾ
	CvMat *ReMat=NULL;
	PD.Detection_FPDW(img, &ReMat, 3);
	PD.show_detections(img, ReMat);

	cvNamedWindow("test");
	cvShowImage("test", img);
	cvWaitKey(0);
}