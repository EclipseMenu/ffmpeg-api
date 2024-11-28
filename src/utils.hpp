#pragma once

#include <string>

namespace ffmpeg::utils {

void customLogCallback(void* ptr, int level, const char* fmt, char* vargs);

std::string getErrorString(int errorCode);

}