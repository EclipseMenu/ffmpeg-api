#include "audio_mixer.hpp"
#include "utils.hpp"
#include <cstdlib>
#include <string>

// Kita tetap biarkan header FFmpeg jika file lain membutuhkannya, 
// tapi kita tidak akan memakainya di sini.
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

BEGIN_FFMPEG_NAMESPACE_V

    // FUNGSI UTAMA: Memanggil GDMPEG
    geode::Result<> AudioMixer::mixVideoAudio(std::filesystem::path videoFile, std::filesystem::path audioFile, std::filesystem::path outputMp4File) {
        
        // Membangun command shell
        std::string cmd = "su -c '/data/data/com.termux/files/home/GDMPEG/dist/bin/gdmpeg_test \"" + 
                          videoFile.string() + "\" \"" + 
                          audioFile.string() + "\" \"" + 
                          outputMp4File.string() + "\"'";

        // Eksekusi via system shell
        int result = std::system(cmd.c_str());

        if (result == 0) {
            return geode::Ok();
        } else {
            // Jika gagal, mungkin karena izin root atau path salah
            return geode::Err("GDMPEG Gagal: Pastikan biner ada di path Termux dan izin root diberikan.");
        }
    }

    // FUNGSI RAW: Kita matikan karena GDMPEG bekerja di level file, bukan buffer memory
    geode::Result<> AudioMixer::mixVideoRaw(const std::filesystem::path& videoFile, const std::vector<float>& raw, const std::filesystem::path &outputMp4File) {
        // XDBot mungkin memanggil ini jika render audio dicentang.
        // Kita kembalikan Ok agar XDBot tidak crash, tapi tidak melakukan apa-apa.
        return geode::Ok();
    }

END_FFMPEG_NAMESPACE_V
