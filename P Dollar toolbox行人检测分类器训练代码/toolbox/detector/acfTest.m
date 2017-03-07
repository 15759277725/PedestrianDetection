function [miss,roc,gt,dt] = acfTest( varargin )
% Test aggregate channel features object detector given ground truth.
%
% USAGE
%  [miss,roc,gt,dt] = acfTest( pTest )
%
% INPUTS
%  pTest    - parameters (struct or name/value pairs)
%   .name     - ['REQ'] detector name
%   .imgDir   - ['REQ'] dir containing test images
%   .gtDir    - ['REQ'] dir containing test ground truth
%   .pLoad    - [] params for bbGt>bbLoad for test data (see bbGt>bbLoad)
%   .ref      - [10.^(-2:.25:0)] reference points (see bbGt>compRoc)
%   .lims     - [3.1e-3 1e1 .05 1] plot axis limits
%   .show     - [0] optional figure number for display
%
% OUTPUTS
%  miss     - log-average miss rate computed at reference points
%  roc      - [nx3] n data points along roc of form [score fp tp]
%  gt       - [mx5] ground truth results [x y w h match] (see bbGt>evalRes)
%  dt       - [nx6] detect results [x y w h score match] (see bbGt>evalRes)
%
% EXAMPLE
%
% See also acfTrain, acfDetect, acfDemoInria, bbGt
%
% Piotr's Image&Video Toolbox      Version 3.20
% Copyright 2013 Piotr Dollar & Ron Appel.  [pdollar-at-caltech.edu]
% Please email me if you find bugs, or have suggestions or questions!
% Licensed under the Simplified BSD License [see external/bsd.txt]

% get parameters
dfs={ 'name','REQ', 'imgDir','REQ', 'gtDir','REQ', 'pLoad',[], ...
  'ref',10.^(-2:.25:0), 'lims',[3.1e-3 1e1 .05 1], 'show',0 };
[name,imgDir,gtDir,pLoad,ref,lims,show] = getPrmDflt(varargin,dfs,1);

% run detector on directory of images
bbsNm=[name 'Dets.txt'];
if(~exist(bbsNm,'file'))
  detector = load([name 'Detector.mat']);
  detector = detector.detector;
  imgNms = bbGt('getFiles',{imgDir});
  acfDetect( imgNms, detector, bbsNm );
end

% run evaluation using bbGt
[gt,dt] = bbGt('loadAll',gtDir,bbsNm,pLoad);
[gt,dt] = bbGt('evalRes',gt,dt);
[fp,tp,score,miss] = bbGt('compRoc',gt,dt,1,ref);
miss=exp(mean(log(1-miss))); roc=[score fp tp];

% optionally plot roc
if( ~show ), return; end
figure(show); plotRoc([fp tp],'logx',1,'logy',1,'xLbl','fppi',...
  'lims',lims,'color','g','smooth',1,'fpTarget',ref);
title(sprintf('log-average miss rate = %.2f%%',miss*100));
savefig([name 'Roc'],show,'png');

end
