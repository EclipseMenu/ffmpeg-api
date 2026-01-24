#include "utils.hpp"
#include <string>

namespace ffmpeg::utils {
    std::string getErrorString(int code) {
        return "Error code: " + std::to_string(code);
    }
}
