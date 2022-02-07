logfile = readtable('../build-RDU3-Desktop_Qt_6_2_3_MSVC2019_64bit-Debug/log.csv');
%%
figure;
plot(diff(logfile.InternalCtr));