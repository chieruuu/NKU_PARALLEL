#include <windows.h> 

class HighResTimer {
private:
    long long head, tail, freq; 

public:
    HighResTimer() {
        QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    }

    void start() {
        QueryPerformanceCounter((LARGE_INTEGER*)&head);
    }

    void stop() {
        QueryPerformanceCounter((LARGE_INTEGER*)&tail);
    }

    double getElapsedMs() {
        return (tail - head) * 1000.0 / freq;
    }
};