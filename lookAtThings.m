logfile = readtable('../build-RDU3-Desktop_Qt_6_2_3_MSVC2019_64bit-Debug/log.csv');

logfile.globalDiff = [0; diff(logfile.InternalCtr)];
logfile.lineDiff = [0; diff(logfile.LineNumber)];
logfile.timeDiff = [0; diff(logfile.Time)];

%%
figure;
plot(logfile.lineDiff)
title('Line Counter Delta')
%% 
figure;
plot(logfile.timeDiff )
title('Packet Counter Delta')
%% 