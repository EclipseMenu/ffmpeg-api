#include "utils.hpp"

#include <Geode/loader/ModEvent.hpp>

extern "C" {
    #include <libavutil/log.h>
    #include <libavutil/error.h>
}

namespace ffmpeg::utils {

static std::vector<std::string> s_ffmpegLogs;

void customLogCallback(void* ptr, int level, const char* fmt, char* vargs) {
    char logBuffer[1024];
    vsnprintf(logBuffer, sizeof(logBuffer), fmt, vargs);

    if (level <= AV_LOG_WARNING)
        s_ffmpegLogs.push_back(logBuffer);

    av_log_default_callback(ptr, level, fmt, vargs);
}

std::string getErrorString(int errorCode) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
	av_strerror(errorCode, errbuf, AV_ERROR_MAX_STRING_SIZE);
    std::string errStr(errbuf);
    errStr += "\nDetails:\n";
    for(const std::string& log : s_ffmpegLogs)
        errStr += log;

    s_ffmpegLogs.clear();
    return errStr;
}

$on_mod(Loaded) {
    av_log_set_callback(customLogCallback);
}

}