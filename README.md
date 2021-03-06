QPULSECAPTURE	
=========================================================================================
An application for the contactless heart rate measurements by means of video processing

Main feaetures:
- provides real time & contactless heart rate measurement (up to each second update), measurement error depends on webcam quality, stationarity of illumination and CPU speed, but in most cases it is less than 3 bpm;
- provides real time video (from web cam) & existing video file processing (.mp4, .wmv, .avi);
- provides a record of registered signals to a hard drive;
- provides mapping of arterial pulse on image (can be used as the face detector);
- compares your heart rate with normal heart rate at rest, with age and sex distribution (statistic courtesy by USA National Health Statistics).

Minimal system requirements:
- Intel Pentium IV 3.0 GHz;
- 512 Mb of RAM;
- A webcam 0,3 Mpx (Logitech B910 HD is the best choice).

Acknowledgements:
- OpenCV (http://opencv.org/);
- FFTW (http://www.fftw.org/);
- ALGLIB (http://www.alglib.net/);
- Qt (http://qt-project.org/);
- And for all engineers and programmers who make the open source products! Cheers!

For the developers:
- all dependencies are provided in Sources.pro;
- an application was properly builded in Qt-creator_5.2.1 with OpenCV_2.4.8 on MSVC2010x32, MSCV2012x64, and MinGW48x32 compilers;
- WVCF_utility is optional, it simply calls DirectShow settings dialog for a webcam, the utility was builded by bcc32 compiler on the base of http://mitov.com/ VisionLab_6.0 library. 

Taranov Alex, 2014.


