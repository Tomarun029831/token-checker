#pragma once
#ifdef _WIN64
#define WEBCAMERA_NAME "Logi C270 HD WebCam"
#define INTEGRATEDCAMERA_NAME "Integrated Camera"
#elif __linux__
#define WEBCAMERA_NAME "C270 HD WEBCAM"
#define INTEGRATEDCAMERA_NAME "Integrated Camera: Integrated C"
#endif
