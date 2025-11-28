#include <iostream>
#include <chrono>
#include <string>
#include <functional>
#include <map>
#include <thread>
#include <iomanip>
#include <mutex>

namespace profiler {

static const std::map<long long, std::string_view> timeSuffixes = {
    {1000000000LL,    "s"  },    // 1s
    {1000000LL,       "ms" },   // 1ms
    {1000LL,          "us" },   // 1us
    {1LL,             "ns" },   // 1ns
    {60000000000LL,   "min"},  // 1min
    {3600000000000LL, "h"  },    // 1h
    {86400000000000LL,"d"  }     // 1d
};

std::string_view getUnitSuffix(double scale) {
    auto it = profiler::timeSuffixes.find((1/scale) * 1000000000LL);
    return it != profiler::timeSuffixes.end() ? it->second : "?";
}

class profilerClock {
public:
    using duration = std::chrono::high_resolution_clock::duration;
    using time_point = std::chrono::high_resolution_clock::time_point;
    static std::function<duration()> nowFunction;

    static duration now() {
        return nowFunction();
    }
};
std::function<profilerClock::duration()> profilerClock::nowFunction = [] { return std::chrono::high_resolution_clock::now().time_since_epoch(); };

template <typename clockT>
void setProfilerClock() {
    profilerClock::nowFunction = [] {
        return std::chrono::duration_cast<profilerClock::duration>(
            clockT::now().time_since_epoch()
        );
    };
}

inline double profilerDurationScale = 1.0;
template <typename durationT>
void setProfilerDurationScale() {
    profilerDurationScale = (double)durationT::period::den / (double)durationT::period::num;
}

using ProfilerOutputFunction = std::function<void(const std::string&, profilerClock::duration)>;
using AverageTimerInfoOutputFunction = std::function<void(profilerClock::duration)>;

std::ostream* defaultProfilerOutputStream = &std::cout;

void id_took_t_suffix(const std::string& id, profilerClock::duration t) {
    *defaultProfilerOutputStream
            << std::setprecision(6)
            << "|| " << id << " took "
            << std::chrono::duration<double>(t).count() * profilerDurationScale
            << getUnitSuffix(profilerDurationScale) << "\n";
}

void id_colon_t_suffix(const std::string& id, profilerClock::duration t) {
    *defaultProfilerOutputStream
            << std::setprecision(6)
            << "|| " << id << ": "
            << std::chrono::duration<double>(t).count() * profilerDurationScale
            << getUnitSuffix(profilerDurationScale) << "\n";
}

void elapsed_time_colon_t_suffix(profilerClock::duration);

ProfilerOutputFunction defaultProfilerOutputFunction = id_took_t_suffix;
AverageTimerInfoOutputFunction defaultAverageTimerInfoOutputFunction = elapsed_time_colon_t_suffix;

profilerClock::duration defaultAverageTimerSleepDuration = std::chrono::seconds(1);

class Timer {
    profilerClock::duration begin;
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
    static profilerClock::duration profilerStartTime;
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

    static void setStartTime(profilerClock::duration t) {
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
        if(!loggingEnabled) {
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
profilerClock::duration AverageTimerManager::profilerStartTime;
bool AverageTimerManager::startTimeSet = false;
bool AverageTimerManager::loggingEnabled = false;

void elapsed_time_colon_t_suffix(profilerClock::duration start) {
    *defaultProfilerOutputStream
            << "|| elapsed time: "
            << std::chrono::duration<double>(profilerClock::now() - start).count() * profilerDurationScale
            << getUnitSuffix(profilerDurationScale) << "\n";
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
#define PF_SET_PROFILER_DURATION_UNIT(x) profiler::setProfilerDurationScale<x>()
#define PF_SET_OUTPUT_FUNCTION(x) profiler::defaultProfilerOutputFunction = (x)
#define PF_SET_OUTPUT_STREAM(x) profiler::defaultProfilerOutputStream = (x)
#define PF_SET_AVERAGE_TIMER_INFO_OUTPUT_FUNCTION(x) profiler::defaultAverageTimerInfoOutputFunction = (x)
#define PF_SET_AVERAGE_TIMER_SLEEP_DURATION(x) profiler::defaultAverageTimerSleepDuration = (x)
#define PF_SET_AVERAGE_TIMER_START_TIME() profiler::AverageTimerManager::setStartTime(profiler::profilerClock::now())
