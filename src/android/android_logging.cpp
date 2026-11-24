#include "android_logging.h"

#include <android/log.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <thread>

namespace zelda64::android {

void redirect_standard_streams() {
    static bool redirected = false;
    if (redirected) {
        return;
    }

    std::array<int, 2> pipeDescriptors{};
    if (pipe(pipeDescriptors.data()) != 0) {
        return;
    }

    redirected = true;
    dup2(pipeDescriptors[1], STDOUT_FILENO);
    dup2(pipeDescriptors[1], STDERR_FILENO);
    close(pipeDescriptors[1]);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IOLBF, 0);

    std::thread([readDescriptor = pipeDescriptors[0]] {
        FILE* stream = fdopen(readDescriptor, "r");
        if (stream == nullptr) {
            close(readDescriptor);
            return;
        }

        std::array<char, 2048> line{};
        while (fgets(line.data(), static_cast<int>(line.size()), stream) != nullptr) {
            __android_log_write(ANDROID_LOG_INFO, "ZeldaNative", line.data());
        }
        fclose(stream);
    }).detach();
}

}
