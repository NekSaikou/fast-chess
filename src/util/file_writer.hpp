#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace fast_chess::util {

// Writes to a file in a thread safe manner.
class FileWriter {
   public:
    FileWriter(const std::string &filename) { file_.open(filename, std::ios::app); }

    void write(const std::string &data) {
        std::lock_guard<std::mutex> lock(file_mutex_);
        file_ << data;
    }

   private:
    std::ofstream file_;
    std::mutex file_mutex_;
};

}  // namespace fast_chess::util
