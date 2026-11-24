#pragma once

#include <android/native_window.h>

#include <memory>

class Rt64Probe {
public:
    Rt64Probe();
    ~Rt64Probe();

    Rt64Probe(const Rt64Probe&) = delete;
    Rt64Probe& operator=(const Rt64Probe&) = delete;

    bool initialize(ANativeWindow* window);
    void shutdown();
    bool ready() const;

private:
    struct State;
    std::unique_ptr<State> state_;
};
