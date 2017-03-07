#include "stdafx.h"
#include "PedDetector.h"
#include "DollarMex.h"
#include <algorithm>
using namespace std;


PedDetector::PedDetector(void)
{
	ChnFtrsChannelNum = 10;
	nPerOct = 8;
	m_Shrink = 4;
	OverLapRate = 0.65;
	num = 0;
	softThreshold = NULL;
	FeaID = NULL; 
	DCtree = NULL;
}


PedDetector::~PedDetector(void)
{
	Release();
}

void PedDetector::Release()
{
	if (DCtree != NULL)
		delete []DCtree;

	if (FeaID != NULL){
		delete[] FeaID;
		FeaID = NULL;
	}
}

bool PedDetector::loadStrongClassifier(const char *pFilePath)
{
	FILE *fs=fopen(pFilePath, "r");
	Release();
/*	this->num = WeakNum;*/
	fscanf(fs, "%d", &this->num);
	this->objectSize.width = 64, this->objectSize.height = 128; 
	this->softThreshold = -1;
	this->FeaID = new int[this->num*3];
	this->xStride = 4;
	this->yStride = 4;
	this->scaleStride = 1.08; // �߶��ڵĲ���û�в��ô˹̶�ֵ
	this->nms = NMS_METHOD_MaxGreedy;
	// ��ȡ2048�����������������ϸ����ڵ�����Ӧ���������
	for (int i=0; i<this->num; i++)
	{
		for (int j=0; j<3; j++)
		{
			fscanf(fs, "%d ", &this->FeaID[i*3+j]);
		}
		int temp1, temp2, temp3, temp4;
		fscanf(fs, "%d %d %d %d ", &temp1, &temp2, &temp3, &temp4);
	}
	// ��ȡ2048�����������������ϲ�ͬ�ڵ�����Ӧ�ľ�����ֵ
	this->DCtree=new WeakClassifier[this->num];
	for (int i=0; i<this->num; i++)
	{
		for (int j=0; j<3; j++)
		{
			fscanf(fs, "%f ", &this->DCtree[i].threshold[j]);
		}
		float temp1, temp2, temp3, temp4;
		fscanf(fs, "%f %f %f %f ", &temp1, &temp2, &temp3, &temp4);
	}
	// ��ȡ2048�����������������ϲ�ͬ�ڵ�����Ӧ��Ȩֵ
	for (int i=0; i<this->num; i++)
	{
		for (int j=0; j<7; j++)
		{
			fscanf(fs, "%f ", &this->DCtree[i].hs[j]);
		}
	}
	fclose(fs);
	// ��ʼ������λ������
	FeaIn = new FtrIndex[5120];
	int m=0;
	CvRect rect;
	rect.width = (this->objectSize.width)/m_Shrink;
	rect.height = (this->objectSize.height)/m_Shrink;
	for( int z=0; z<ChnFtrsChannelNum; z++ )
		for( int c=0; c<rect.width; c++ )
			for( int r=0; r<rect.height; r++ )
			{
				FeaIn[m].Channel=z;
				FeaIn[m].x=c;
				FeaIn[m++].y=r;
			}

			return true;
}

float PedDetector::StrongClassify(CvMat *pChnFtrsData)
{
	float* tempChnFtrs;
	tempChnFtrs = pChnFtrsData->data.fl;
	float ans=0.0f;
	for (int i=0; i<num; i++)
	{
		ans+=DCtree[i].Classify(tempChnFtrs+3*i);
		if (ans<-1) return ans;
	}
	return ans;
}

bool PedDetector::show_detections(IplImage *pImg, CvMat *pAllResults, CvScalar color)
{
	if (pAllResults == NULL)
	{
		return true;
	}
	CvScalar FondColor;
	CvFont font;
	char str[100];
	cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 1, 1, 0, 2);
	FondColor = CV_RGB(255,255,255);
	int i;
	for (i=0; i<pAllResults->rows; i++)
	{
		CvScalar tmp = cvGet1D(pAllResults, i);
		tmp.val[0] = tmp.val[0];
		tmp.val[1] = tmp.val[1];
		tmp.val[2] = tmp.val[2];
		CvRect rectDraw;
		rectDraw.width = 40;
		rectDraw.height = 100;
		if (tmp.val[3] > 0)
			color = CV_RGB(0, 255, 0);
		else
			color = CV_RGB(200, 100, 100);
		{
			// ��ʾ����
			sprintf(str, "%.4f", tmp.val[3]);
			cvPutText(pImg, str, 
				cvPoint(int(tmp.val[0]-rectDraw.width/2*tmp.val[2]), int(tmp.val[1]-rectDraw.height/2*tmp.val[2])+12), 
				&font, FondColor);
			// ��ʾ�������
			cvRectangle(pImg, 
				cvPoint(int(tmp.val[0]-rectDraw.width/2*tmp.val[2]), int(tmp.val[1]-rectDraw.height/2*tmp.val[2])), 
				cvPoint(int(tmp.val[0]+rectDraw.width/2*tmp.val[2]), int(tmp.val[1]+rectDraw.height/2*tmp.val[2])), 
				color, 2);
		}
	}
	return true;
}

bool PedDetector::Detection_FPDW(IplImage *pImgInput, CvMat **pAllResults, float UpScale)
{
	CvMat ***ChnFtrs; // �洢����ͨ������
	float ***ChnFtrs_float; // ƽ��ǰ������
	float scaleStridePerOctave = 2.0f; // FPDW��ÿ��Octive�ĳ߶ȿ��
	float nowScale; // ��ǰ�߶�
	CvRect rect, PicRect; // rect�Ǽ����С��PicRect�ǵ�ǰ�߶��µ�����ͼ���С
	float ans; // �������
	float *FeaData = new float[this->num*3]; // ����
	int shrink_rate = m_Shrink; // ϡ����
	CvScalar *tmpScalar; // �������ʽ

	int step, t;
	int t_AllIntegral = 0;
	float *Scales; // ͼ���������ͬ������Ӧ�����ű���
	int itemp1, itemp2;

	CvMemStorage *memory; // ����������ڴ�
	CvSeq *seqDetections; // ���������
	memory = cvCreateMemStorage();
	seqDetections = cvCreateSeq(0, sizeof(CvSeq), sizeof(CvScalar), memory);
	rect.width = 64, rect.height = 128;

	// ͼ��Ĵ洢��ʽת������ת����LUV��ɫ�ռ�
	int h = (pImgInput->height/shrink_rate)*shrink_rate, w = (pImgInput->width/shrink_rate)*shrink_rate;
	IplImage *pImg = cvCreateImage(cvSize(w, h), pImgInput->depth, pImgInput->nChannels);
	cvSetImageROI(pImgInput, cvRect(0, 0, w, h));
	cvCopyImage(pImgInput, pImg);
	cvResetImageROI(pImgInput);
	//int h = pImg->height, w = pImg->width;
	int d=pImg->nChannels;
	int ChnBox = h*w;
	unsigned char *data = new unsigned char[h*w*3];
	IplImage *imgB = cvCreateImage(cvGetSize(pImg),IPL_DEPTH_8U,1);
	IplImage *imgG = cvCreateImage(cvGetSize(pImg),IPL_DEPTH_8U,1);
	IplImage *imgR = cvCreateImage(cvGetSize(pImg),IPL_DEPTH_8U,1);
	cvSplit(pImg, imgB, imgG, imgR, NULL);
	for (int i=0; i<ChnBox; i++){
		data[i] = imgR->imageData[i];
		data[ChnBox+i] = imgG->imageData[i];
		data[2*ChnBox+i] = imgB->imageData[i];
	}
	cvReleaseImage(&imgR);
	cvReleaseImage(&imgG);
	cvReleaseImage(&imgB);
	// 	for (int col = 0; col < w; col++)
	// 	{
	// 		for(int row = 0; row < h; row++)
	// 		{
	// 			itemp1 = row * pImg->widthStep + col * pImg->nChannels;
	// 			data[2*ChnBox+w*row+col] = pImg->imageData[itemp1]; 
	// 			data[ChnBox+w*row+col] = pImg->imageData[itemp1+1];  
	// 			data[w*row+col] = pImg->imageData[itemp1+2]; 
	// 		}
	// 	}
	float *luvImg = rgbConvert(data, ChnBox, 3, 2, 1.0/255);
	delete[] data;

	//����ͼ�����������
	nowScale=1.0;
	int nScales = min((int)(nPerOct*(log(min((double)pImg->width/64, (double)pImg->height/128))/log(2.0))+1), 
		(int)(nPerOct*log(UpScale)/log(2.0)+1));
	ChnFtrs = new CvMat**[nScales];
	Scales = new float[nScales];
	// ����ÿ�����������Ӧ�����ű���
	for (int i=0; i<nScales; i++){
		Scales[i] = pow(2.0, (double)(i+1)/nPerOct);
	}
	int *Octives = new int[nScales];
	int nOctives = 0;
	bool *isChnFtrs = new bool[nScales]; // ���ÿ��������Ƿ����
	memset(isChnFtrs, 0, nScales*sizeof(bool));
	// ���Octives���ȼ���Octives��Ȼ�������߶������ڵ�Octives���Ƴ�����FPDW���������BMVC2010 - the fastest pedestrian detector in the west��
	while (nOctives*nPerOct<nScales){
		Octives[nOctives] = nOctives*nPerOct;
		nOctives++;
	}

	int *NearOctive = new int[nScales]; // ���ÿ��ͼ��������Ľ���Դ
	int iTemp = 0;
	for (int i=1; i<nOctives; i++){
		for (int j=iTemp; j<=(Octives[i-1]+Octives[i])/2; j++){
			NearOctive[j] = Octives[i-1];
		}
		iTemp = (Octives[i-1]+Octives[i])/2+1;
	}
	for (int i=iTemp; i<nScales; i++) NearOctive[i] = Octives[nOctives-1];
	//�ȼ���Octives�Ļ���ͨ������
	for (int i=0; i<nOctives; i++){
		isChnFtrs[Octives[i]] = true;
		ChnFtrs[Octives[i]] = new CvMat *[ChnFtrsChannelNum];
		GetChnFtrs(luvImg, h, w, shrink_rate, Scales[Octives[i]], ChnFtrs[Octives[i]]);
	}
	// ���Ƴ�ͼ����������� lambdas
	float lambdas[10] = {0};
	if (nOctives<2){
		for (int i=3; i<10; i++) lambdas[i] = 0.1158;
	}
	else{
		for (int i=3; i<10; i++){
			float f0, f1;
			CvScalar tempS;
			tempS = cvSum(ChnFtrs[Octives[0]][i]);
			f0 = tempS.val[0]/(ChnFtrs[Octives[0]][i]->width*ChnFtrs[Octives[0]][i]->height);
			tempS = cvSum(ChnFtrs[Octives[1]][i]);
			f1 = tempS.val[0]/(ChnFtrs[Octives[1]][i]->width*ChnFtrs[Octives[1]][i]->height);
			lambdas[i] = log(f1/f0)/log(2.0);
		}
	}

	// ����Octives����resample���������ͼ�������
	for (int i=0; i<nScales; i++){
		if (isChnFtrs[i]) continue;
		int hNow = (int)(h/Scales[i]/shrink_rate+0.5);
		int wNow = (int)(w/Scales[i]/shrink_rate+0.5);
		int h0 = ChnFtrs[NearOctive[i]][0]->height;
		int w0 = ChnFtrs[NearOctive[i]][0]->width;
		ChnFtrs[i] = new CvMat*[ChnFtrsChannelNum];
		for (int j=0; j<ChnFtrsChannelNum; j++){
			float ratio = pow(Scales[NearOctive[i]]/Scales[i], -lambdas[j]);
			ChnFtrs[i][j] = cvCreateMat(hNow, wNow, CV_32FC1);
			float_resample(ChnFtrs[NearOctive[i]][j]->data.fl, ChnFtrs[i][j]->data.fl, w0, wNow, h0, hNow, 1, ratio);
		}
	}
	for (int i=0; i<nScales; i++){
		for (int j=0; j<ChnFtrsChannelNum; j++){
			convTri1(ChnFtrs[i][j]->data.fl, ChnFtrs[i][j]->data.fl, ChnFtrs[i][j]->width, ChnFtrs[i][j]->height, 1, (float)2.0, 1);
		}
	}

	// AdaBoost����������
	for (step=0; step<nScales; step++)
	{
		//�߶Ƚ�������������
		if ((int)(pImg->width/Scales[step]+0.5f)<rect.width || (int)(pImg->height/Scales[step]+0.5f)<rect.height)
		{
			break;
		}
		CvSize ScaleSize = cvSize(ChnFtrs[step][0]->width*shrink_rate, ChnFtrs[step][0]->height*shrink_rate);

		PicRect.width = ChnFtrs[step][0]->width;
		PicRect.height = ChnFtrs[step][0]->height;
		// �ܼ���������
		for (PicRect.y = 0; PicRect.y+rect.height <= ScaleSize.height; PicRect.y += yStride)
		{
			for (PicRect.x = 0; PicRect.x+rect.width <= ScaleSize.width; PicRect.x += xStride)
			{
				rect.x=(PicRect.x/m_Shrink);
				rect.y=(PicRect.y/m_Shrink);

				//����ÿ�����ڵļ����������
				float score = 0.0;
				for (t=0; t<this->num; t++)
				{
					for (int j=0; j<3; j++)
					{
						int temp;
						temp=this->FeaID[t*3+j];
						FeaData[t*3+j]=ChnFtrs[step][FeaIn[temp].Channel]->data.fl[(FeaIn[temp].y+rect.y)*PicRect.width+FeaIn[temp].x+rect.x];
					}
					score += this->DCtree[t].Classify(FeaData+t*3);
					if (score<softThreshold) break;
				}
				if (score > 0.0)
				{
					tmpScalar = (CvScalar *)cvSeqPush(seqDetections, NULL);
					tmpScalar->val[0] = (PicRect.x + rect.width/2) * Scales[step];//��������x����;
					tmpScalar->val[1] = (PicRect.y + rect.height/2) * Scales[step];//��������y����;
					tmpScalar->val[2] = Scales[step];//�������������ģ�͵����ű���;
					tmpScalar->val[3] = score ; // ������
				}
			}
		}
	}
	cvReleaseImage(&pImg);
	// �ͷŻ���ͨ�������ڴ�
	for (step=0; step<nScales; step++){
		for (int i=0; i<ChnFtrsChannelNum; i++){
			cvReleaseMat(&ChnFtrs[step][i]);
		}
		delete[] ChnFtrs[step];
	}
	delete[] ChnFtrs;


	// non maximum suppression �Ǽ���ֵ���ƹ���
	CvMat *pDetections = NULL;
	CvMat *pModes = NULL;

	if (seqDetections->total > 0)
	{
		pDetections = cvCreateMat(seqDetections->total, 1, CV_64FC4);
		for (int i=0; i<seqDetections->total; i++)
		{
			tmpScalar = (CvScalar *)cvGetSeqElem(seqDetections, i);
			cvSet1D(pDetections, i, *tmpScalar);
		}

		if (nms == NMS_METHOD_MaxGreedy)
			MaxGreedy_NonMaxSuppression(pDetections, OverLapRate, &pModes);
		else
		{
			//�������detection
			pModes = (CvMat *)cvClone(pDetections);
		}
	}
	cvReleaseMemStorage(&memory);
	cvReleaseMat(&pDetections);

	if (*pAllResults != NULL)
		cvReleaseMat(pAllResults);
	*pAllResults = pModes;
	delete[] luvImg;
	delete[] FeaData;
	delete[] isChnFtrs;
	delete[] Scales;
	delete[] Octives;
	delete[] NearOctive;
	return true;
}

// NMS�Ǽ���ֵ���ƣ��˶�̰�ĵİ취������������������򣬴�ǰ���󰴸�����ɸѡ���;
static int cmp_detection_by_score(const void *_a, const void *_b)
{
	double *a = (double *)_a;
	double *b = (double *)_b;
	if (a[3] > b[3]) // [0]:x, [1]:y, [2]:s, [3]:score
		return -1;
	if (a[3] < b[3])
		return 1;
	return 0;
}
// ��ȡ����������໥������
double GetOverlapRate(double *D1, double *D2)
{
	double Pw, Ph;
	Pw = 41.0, Ph = 100.0;
	double xr1, yr1, xl1, yl1;
	double xr2, yr2, xl2, yl2;
	xl1 = D1[0]-D1[2]*Pw/2;	
	xr1 = D1[0]+D1[2]*Pw/2;
	xl2 = D2[0]-D2[2]*Pw/2;
	xr2 = D2[0]+D2[2]*Pw/2;
	double ix = min(xr1, xr2) - max(xl1, xl2);
	if (ix<0) return -1;
	yr2 = D2[1]+D2[2]*Ph/2;
	yl1 = D1[1]-D1[2]*Ph/2;
	yr1 = D1[1]+D1[2]*Ph/2;
	yl2 = D2[1]-D2[2]*Ph/2;
	double iy = min(yr1, yr2) - max(yl1, yl2);
	if (iy<0) return -1;
	return ix*iy/min((yr1-yl1)*(xr1-xl1), (yr2-yl2)*(xr2-xl2));

}
void PedDetector::MaxGreedy_NonMaxSuppression(CvMat *pDetections, float overlap, CvMat **pModes)
{
	// �������м��򰴷����ߵ�����
	qsort((void *)pDetections->data.db, pDetections->rows, 4*sizeof(double), cmp_detection_by_score);
	int n = pDetections->rows;
	int ReTotal=0;
	bool *isHold = new bool[n];
	memset(isHold, 1, n*sizeof(bool));
	// �������Ӹߵ��ף������߷ּ��򣬲��޳����뱣�����ڸ����ʳ���overlap�ĵͷ�ֵ���ڣ�̰�Ĳ��ԣ�
	for (int i=0; i<n; i++){
		if (!isHold[i]) continue;
		ReTotal++;
		CvScalar Di;
		Di = cvGet1D(pDetections, i);
		for (int j=i+1; j<n; j++){
			double overlapRate;
			CvScalar Dj;
			Dj = cvGet1D(pDetections, j);
			overlapRate = GetOverlapRate(Di.val, Dj.val);
			if (overlapRate<0) continue;
			if (overlapRate>overlap) isHold[j] = false;
		}
	}
	*pModes = cvCreateMat(ReTotal, 1, CV_64FC4);
	ReTotal=0;
	for (int i=0; i<n; i++){
		if (isHold[i]){
			CvScalar temp;
			temp = cvGet1D(pDetections, i);
			cvSet1D(*pModes, ReTotal, temp);
			ReTotal++;
		}
	}
	delete[] isHold;
}

float* convConst(float* I,int h, int w,int d, float r)
{
	int s=1;
	float *O = new float[h*w*d];
	if (r<=1){
		convTri1(I, O, h, w, d, (float)12/r/(r+2)-2, s);
	}
	else{
		convTri(I, O, h, w, d, (int)(r+0.1), s);
	}
	return O;
}

bool PedDetector::GetChnFtrs(float *pImgInput, float h0, float w0,int shrink_rate, float nowScale,CvMat *ChnFtrs[])
{
	float *I, *M, *O, *LUV, *S, *H;
	int h,w, wS, hS, d=3;
	int normRad=5;
	float normConst = 0.005;

	// ��ԭʼͼ���������ŵ���ǰ�߶�
	h = shrink_rate*(int)(h0/nowScale/shrink_rate+0.5), w = shrink_rate*(int)(w0/nowScale/shrink_rate+0.5);
	float *data = new float[h*w*3];
	wS = (w/m_Shrink), hS = (h/m_Shrink);
	float_resample(pImgInput, data, w0, w, h0, h, 3, 1.0);
	// ��ͼ�����ݽ��о������
	I = convConst(data, w, h, d, 1);

	//free(luvImg);
	M = new float[w*h];
	O = new float[w*h];
	// �����ݶȷ�ֵ
	gradMag(I, M, O, w, h, 3 );
	// ���ݶȷ�ֵͼ�����ݽ��о������
	S = convConst(M, w, h, 1, normRad);
	// ��һ��
	gradMagNorm(M, S, w, h, normConst);
	H = new float[wS*hS*6];
	memset(H, 0, wS*hS*6*sizeof(float));
	// �����ݶȷ���
	gradHist(M, O, H, w, h, m_Shrink, 6, false);

	float *M_shrink = new float[wS*hS];
	float *I_shrink = new float[wS*hS*3];
	// ��ͼ�����ϡ���ز����������൱�ڼ��㲢��m_Shrink*m_Shrink���ο�����
	float_resample(M, M_shrink, w, wS, h, hS, 1, (float)1.0);
	float_resample(I, I_shrink, w, wS, h, hS, 3, (float)1.0);

	// �������ս��
	for (int i=0; i<3; i++){
		ChnFtrs[i] = cvCreateMat(hS, wS, CV_32FC1);
		for (int j=0; j<hS*wS; j++){
			ChnFtrs[i]->data.fl[j] = I_shrink[i*hS*wS+j];
		}
	}
	ChnFtrs[3] = cvCreateMat(hS, wS, CV_32FC1);
	for (int i=0; i<hS*wS; i++){
		ChnFtrs[3]->data.fl[i] = M_shrink[i];
	}
	for (int i=4; i<10; i++){
		ChnFtrs[i] = cvCreateMat(hS, wS, CV_32FC1);
		for (int j=0; j<hS*wS; j++){
			int mod_i = (13-i)%6; // H������������з����෴���跴����
			ChnFtrs[i]->data.fl[j] = H[mod_i*hS*wS+j];
		}
	}

	free(I);
	free(M);
	free(O);
	free(S);
	free(M_shrink);
	free(I_shrink);
	free(H);
	free(data);

	return true;
}