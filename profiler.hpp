#include <iostream>
#include <chrono>
#include <string>
#include <functional>
#include <map>
#include <thread>
#include <iomanip>
#include <mutex>

namespace profiler {

using profilerClock = std::chrono::high_resolution_clock;
template <typename clockT>
void setProfilerClock(){
    using profilerClock = clockT;
}

using profilerDurationT = double;
template <typename durationT>
void setProfilerDurationT(){
    using profilerDurationT = durationT;
}

using ProfilerOutputFunction = std::function<void(const std::string&, profilerClock::duration)>;
using AverageTimerInfoOutputFunction = std::function<void(profilerClock::time_point)>;

std::ostream* defaultProfilerOutputStream = &std::cout;

void id_took_t_s(const std::string& id, profilerClock::duration t) {
    *defaultProfilerOutputStream
            << std::setprecision(std::numeric_limits<profilerDurationT>::digits10)
            << "|| " << id << " took "
            << std::chrono::duration<profilerDurationT>(t).count() << "s\n";
}

void id_colon_t_s(const std::string& id, profilerClock::duration t) {
    *defaultProfilerOutputStream
            << std::setprecision(std::numeric_limits<profilerDurationT>::digits10)
            << "|| " << id << ": "
            << std::chrono::duration<profilerDurationT>(t).count() << "s\n";
}

void elapsed_time_colon_t_s(profilerClock::time_point);

ProfilerOutputFunction defaultProfilerOutputFunction = id_took_t_s;
AverageTimerInfoOutputFunction defaultAverageTimerInfoOutputFunction = elapsed_time_colon_t_s;

profilerClock::duration defaultAverageTimerSleepDuration = std::chrono::seconds(1);

class Timer {
    profilerClock::time_point begin;
public:
    void start() {
        begin = profilerClock::now();
    }
    profilerClock::duration stop() {
        return profilerClock::now() - begin;
    }
};

class AverageTimerManager {
    static std::mutex collectedTimesMapMutex;
    static std::map<std::string, std::vector<profilerClock::duration>> collectedTimesMap;
    static profilerClock::time_point profilerStartTime;
    static bool startTimeSet;
    static bool loggingEnabled;
public:
    static void log() {
        std::lock_guard<std::mutex> lock(collectedTimesMapMutex);
        for(auto& p : collectedTimesMap) {
            profilerClock::duration avg = std::chrono::seconds(0);
            for(auto& t : p.second) avg += t;
            avg /= p.second.size();
            defaultAverageTimerInfoOutputFunction(profilerStartTime);
            defaultProfilerOutputFunction(p.first, avg);
        }
    }

    static void setStartTime(profilerClock::time_point t){
        profilerStartTime = t;
        startTimeSet = true;
    }

    static void addTime(const std::string& id, profilerClock::duration t) {
        std::lock_guard<std::mutex> lock(collectedTimesMapMutex);
        collectedTimesMap[id].push_back(t);
    }

    static void logLoop() {
        while(true) {
            std::this_thread::sleep_for(defaultAverageTimerSleepDuration);
            log();
            std::lock_guard<std::mutex> lock(collectedTimesMapMutex);
            collectedTimesMap.clear();
        }
    }

    static void startLoggingThread() {
        if(!startTimeSet) setStartTime(profilerClock::now());
        if(!loggingEnabled){
            std::thread t(logLoop);
            if (t.joinable()) {
                t.detach();
                startTimeSet = true;
            } else std::cerr << "could not detach average timer thread\n";
        }
    }
};
std::mutex AverageTimerManager::collectedTimesMapMutex;
std::map<std::string, std::vector<profilerClock::duration>> AverageTimerManager::collectedTimesMap;
profilerClock::time_point AverageTimerManager::profilerStartTime;
bool AverageTimerManager::startTimeSet = false;
bool AverageTimerManager::loggingEnabled = false;

void elapsed_time_colon_t_s(profilerClock::time_point start) {
    *defaultProfilerOutputStream
            << "|| elapsed time: "
            << std::chrono::duration<profilerDurationT>(profilerClock::now() - start).count()
            << "s\n";
}

class AverageTimer {
    const char* id;
    Timer t;
public:
    AverageTimer(const char* _id) : id(_id) {
        t.start();
    }
    ~AverageTimer() {
        AverageTimerManager::addTime(id, t.stop());
    }
};

class ScopeTimer {
    const char* id;
    Timer t;
public:
    ScopeTimer(const char* _id) : id(_id) {
        t.start();
    }
    ~ScopeTimer() {
        defaultProfilerOutputFunction(id, t.stop());
    }
};

}

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

#define PF_ENABLE_AVERAGE_TIMER_AUTO_LOG() profiler::AverageTimerManager::startLoggingThread()

#define PF_SCOPE_TIMER(x) profiler::ScopeTimer CONCAT(scopetimer_, __LINE__)(x)
#define PF_AVERAGE_TIMER(x) profiler::AverageTimer CONCAT(averagetimer_, __LINE__)(x)

#define PF_AVERAGE_TIMER_LOG() profiler::AverageTimerManager::log()

#define PF_SET_PROFILER_CLOCK(x) profiler::setProfilerClock<x>()
#define PF_SET_PROFILER_DURATION_T(x) profiler::setProfilerDurationT<x>()
#define PF_SET_OUTPUT_FUNCTION(x) profiler::defaultProfilerOutputFunction = (x)
#define PF_SET_OUTPUT_STREAM(x) profiler::defaultProfilerOutputStream = (x)
#define PF_SET_AVERAGE_TIMER_INFO_OUTPUT_FUNCTION(x) profiler::defaultAverageTimerInfoOutputFunction = (x)
#define PF_SET_AVERAGE_TIMER_SLEEP_DURATION(x) profiler::defaultAverageTimerSleepDuration = (x)
#define PF_SET_AVERAGE_TIMER_START_TIME() profiler::AverageTimerManager::setStartTime(profiler::profilerClock::now())
