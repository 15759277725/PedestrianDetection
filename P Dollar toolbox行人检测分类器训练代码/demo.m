
t=load('models\AcfInriaDetector.mat');%%ѵ���õļ����
detector=t.detector;
filename = 'D:\test\03.BMP';%%��Ҫ���ͼƬ��λ��
%% modify detector (see acfModify)
detector = acfModify(detector,'cascThr',-1,'cascCal',0);

%% run detector on a sample image (see acfDetect)
I=imread(filename); tic, bbs=acfDetect(I,detector); toc
figure(1); im(I); bbApply('draw',bbs); pause(.1);