#pragma once

#include <vector>
#include <cv.h>
#include <highgui.h>
using namespace std;
/*
*/
enum NMS_METHOD
{
	NMS_METHOD_NONE=0,
	NMS_METHOD_MeanShift,
	NMS_METHOD_MaxGreedy,
	NMS_METHOD_PairWiseMax,
	NMS_METHOD_PairWiseMax_1
};
// AdaBoost���������ṹ
struct WeakClassifier	/*	������ȵľ�����depth-2 decision tree	*/
{
	float threshold[3];	/* ��������ÿ���ڵ����ֵthreshold	*/
	float hs[7];		/* ÿ���ڵ��Ȩֵ 1Ϊ��һ��ڵ� 2��3Ϊ�ڶ���ڵ� 3,4,5,6Ϊ����������Ҷ�ӽڵ� */

	//ȡҶ�ӽڵ������Ȩ��
	float Classify(float *f)
	{
		return hs[GetLeafNode(f)];
	}
	// ���ݾ�������ֵ�жϣ�Ѱ�Ҷ�ӦҶ�ӽڵ��λ��
	int GetLeafNode(float *f)
	{
		if (f[0] < threshold[0])
		{
			return (f[1] < threshold[1]) ? 3:4;
		}
		else
		{
			return (f[2] < threshold[2]) ? 5:6;
		}
	}
};

// �洢�����������������Ӧ�Ļ���ͨ������λ��
struct FtrIndex 
{
	int Channel;
	int x;
	int y;
};

class PedDetector
{
public:
	int num;							/* ������������ number of weak classifier	*/
	int *FeaID;							/* ������������ÿ���ڵ�����Ӧ��������ţ������������һ�������ڵ㣬4��Ҷ�ӽڵ㲻��Ӧ���� */
	int xStride, yStride;				/*�����Ĳ���*/
	float scaleStride;					/*�߶ȵĲ���*/
	WeakClassifier *DCtree;				/*˫�������������*/
	int m_Shrink;						/*ģ��ϡ�ͱ������൱���������ο�ı߳���һ��Ĭ��Ϊ4 */
	int ChnFtrsChannelNum;				/*����ͨ������Ĭ��Ϊ10,3��LUV��ɫͨ��+1���ݶȷ�ֵ+6���ݶȷ���*/
	FtrIndex *FeaIn;				/*�洢�����������������Ӧ�Ļ���ͨ��ͼ����λ��*/
	int nPerOct;						/*ͼ��ƽ��ÿ����2������Ҫ�����������Ĳ���*/

	NMS_METHOD nms;						/*�Ǽ���ֵ�����㷨*/
	float OverLapRate;					/*������̰���㷨���зǼ���ֵ����ʱ����ֵ����������ֵ���޳��������ʴ��ڴ�ֵ�ļ���*/

	CvSize objectSize;					/* ģ�ʹ�С: 64x128 */
	float softThreshold;				/* softThreshold��ֵ��Ĭ��ȡ-1 */

public:
	PedDetector(void);
	~PedDetector(void);
	void Release();
	/* 
	������MatLab����ѵ���õķ���������
	���룺������·�������������� */
	bool loadStrongClassifier(const char *pFilePath);
	/* 
	AdaBoostǿ������
	���룺��������
	��������ռ����� */
	float StrongClassify(CvMat *pChnFtrsData);

	/*
	�����˼������ӡ��ԭʼͼ���в���ʾ
	���룺
	pImg ԭʼͼ��
	pAllResults �����
	color ������ʾ��ɫ */
	bool show_detections(IplImage *pImg, CvMat *pAllResults, CvScalar color =  CV_RGB(0, 255, 0));
	/*
	��ȡ����ͨ������
	���룺
	pImgInput float��ʽ��ͼ������
	h0 ͼ���
	w0 ͼ���
	shrink_rate ͼ��ϡ���ʣ����������ο�߳�
	nowScale ͼ��������߶�ֵ
	ChnFtrs �洢����������10��ͨ�� */
	bool GetChnFtrs(float *pImgInput, float h0, float w0,int shrink_rate, float nowScale,CvMat *ChnFtrs[]);

	/*
	���˼��ӿں���������FPDW���������BMVC2010 - the fastest pedestrian detector in the west��
	���룺
	pImgInput �����ͼ��
	pAllResults �洢��������洢��ʽΪ��ÿ�����ݰ� ��������x���ꡢ��������y���ꡢ�������������ģ�͵����ű��������ռ��÷� ���У�
	UpScale ������˵����߶����ޣ���128*64��СΪ��׼ */
	bool Detection_FPDW(IplImage *pImgInput, CvMat **pAllResults, float UpScale=99999);

	// NMS�Ǽ���ֵ���ƣ��˶�̰�ĵİ취������������������򣬴�ǰ���󰴸�����ɸѡ���;
	void MaxGreedy_NonMaxSuppression(CvMat *pDetections, float overlap, CvMat **pModes);
};

