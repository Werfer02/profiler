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

void id_colon_t_suffix_out_of_sleepduration(const std::string&, profilerClock::duration);

void elapsed_time_colon_t_suffix(profilerClock::duration);

ProfilerOutputFunction defaultProfilerOutputFunction = id_took_t_suffix;
ProfilerOutputFunction defaultCumulativeTimerOutputFunction = id_colon_t_suffix_out_of_sleepduration;
AverageTimerInfoOutputFunction defaultAverageTimerInfoOutputFunction = elapsed_time_colon_t_suffix;

profilerClock::duration defaultAverageTimerSleepDuration = std::chrono::seconds(1);
profilerClock::duration defaultCumulativeTimerSleepDuration = std::chrono::seconds(1);

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
    static std::mutex collectedAverageTimesMapMutex;
    static std::map<std::string, std::vector<profilerClock::duration>> collectedAverageTimesMap;

    static std::mutex collectedCumulativeTimesMapMutex;
    static std::map<std::string, std::vector<profilerClock::duration>> collectedCumulativeTimesMap;

    static profilerClock::duration profilerStartTime;
    static bool startTimeSet;
    static bool loggingEnabled;
public:

    static void setStartTime(profilerClock::duration t) {
        profilerStartTime = t;
        startTimeSet = true;
    }

    static void averageLog() {
        std::lock_guard<std::mutex> lock(collectedAverageTimesMapMutex);
        for(auto& p : collectedAverageTimesMap) {
            profilerClock::duration avg = std::chrono::seconds(0);
            for(auto& t : p.second) avg += t;
            avg /= p.second.size();
            defaultAverageTimerInfoOutputFunction(profilerStartTime);
            defaultProfilerOutputFunction(p.first, avg);
        }
    }

    static void cumulativeLog() {
        std::lock_guard<std::mutex> lock(collectedCumulativeTimesMapMutex);
        for(auto& p : collectedCumulativeTimesMap) {
            profilerClock::duration total = std::chrono::seconds(0);
            for(auto& t : p.second) total += t;
            defaultAverageTimerInfoOutputFunction(profilerStartTime);
            defaultCumulativeTimerOutputFunction(p.first, total);
        }
    }

    static void addAverageTime(const std::string& id, profilerClock::duration t) {
        std::lock_guard<std::mutex> lock(collectedAverageTimesMapMutex);
        collectedAverageTimesMap[id].push_back(t);
    }

    static void addCumulativeTime(const std::string& id, profilerClock::duration t) {
        std::lock_guard<std::mutex> lock(collectedCumulativeTimesMapMutex);
        collectedCumulativeTimesMap[id + " (cumulative)"].push_back(t);
    }

    static void averageLogLoop() {
        while(true) {
            std::this_thread::sleep_for(defaultAverageTimerSleepDuration);
            averageLog();
            std::lock_guard<std::mutex> lock(collectedAverageTimesMapMutex);
            collectedAverageTimesMap.clear();
        }
    }

    static void cumulativeLogLoop() {
        while(true) {
            std::this_thread::sleep_for(defaultCumulativeTimerSleepDuration);
            cumulativeLog();
            std::lock_guard<std::mutex> lock(collectedCumulativeTimesMapMutex);
            collectedCumulativeTimesMap.clear();
        }
    }

    static void startAverageLoggingThread() {
        if(!startTimeSet) setStartTime(profilerClock::now());
        if(!loggingEnabled) {
            std::thread t(averageLogLoop);
            if (t.joinable()) {
                t.detach();
                startTimeSet = true;
            } else std::cerr << "could not detach average timer thread\n";
        }
    }

    static void startCumulativeLoggingThread() {
        if(!startTimeSet) setStartTime(profilerClock::now());
        if(!loggingEnabled) {
            std::thread t(cumulativeLogLoop);
            if (t.joinable()) {
                t.detach();
                startTimeSet = true;
            } else std::cerr << "could not detach cumulative timer thread\n";
        }
    }
};
std::mutex AverageTimerManager::collectedAverageTimesMapMutex;
std::map<std::string, std::vector<profilerClock::duration>> AverageTimerManager::collectedAverageTimesMap;

std::mutex AverageTimerManager::collectedCumulativeTimesMapMutex;
std::map<std::string, std::vector<profilerClock::duration>> AverageTimerManager::collectedCumulativeTimesMap;

profilerClock::duration AverageTimerManager::profilerStartTime;
bool AverageTimerManager::startTimeSet = false;
bool AverageTimerManager::loggingEnabled = false;

void elapsed_time_colon_t_suffix(profilerClock::duration start) {
    *defaultProfilerOutputStream
            << "|| elapsed time: "
            << std::chrono::duration<double>(profilerClock::now() - start).count() * profilerDurationScale
            << getUnitSuffix(profilerDurationScale) << "\n";
}

void id_colon_t_suffix_out_of_sleepduration(const std::string& id, profilerClock::duration t){
    *defaultProfilerOutputStream
        << std::setprecision(6)
        << "|| " << id << ": "
        << std::chrono::duration<double>(t).count() * profilerDurationScale
        << getUnitSuffix(profilerDurationScale)
        << " out of "
        << defaultCumulativeTimerSleepDuration.count() / 1000000000LL * profilerDurationScale
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
        AverageTimerManager::addAverageTime(id, t.stop());
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

class CumulativeTimer {
    const char* id;
    Timer t;
public:
    CumulativeTimer(const char* _id) : id(_id) {
        t.start();
    }
    ~CumulativeTimer() {
        AverageTimerManager::addCumulativeTime(id, t.stop());
    }
};

}

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

#define PF_ENABLE_AVERAGE_TIMER_AUTO_LOG() profiler::AverageTimerManager::startAverageLoggingThread()
#define PF_ENABLE_CUMULATIVE_TIMER_AUTO_LOG() profiler::AverageTimerManager::startCumulativeLoggingThread()

#define PF_SCOPE_TIMER(x) profiler::ScopeTimer CONCAT(scopetimer_, __LINE__)(x)
#define PF_AVERAGE_TIMER(x) profiler::AverageTimer CONCAT(averagetimer_, __LINE__)(x)
#define PF_CUMULATIVE_TIMER(x) profiler::CumulativeTimer CONCAT(cumulativetimer_, __LINE__)(x)

#define PF_AVERAGE_TIMER_LOG() profiler::AverageTimerManager::averageLog()
#define PF_CUMULATIVE_TIMER_LOG() profiler::AverageTimerManager::cumulativeLog()

#define PF_SET_PROFILER_CLOCK(x) profiler::setProfilerClock<x>()
#define PF_SET_PROFILER_DURATION_UNIT(x) profiler::setProfilerDurationScale<x>()
#define PF_SET_OUTPUT_STREAM(x) profiler::defaultProfilerOutputStream = (x)

#define PF_SET_OUTPUT_FUNCTION(x) profiler::defaultProfilerOutputFunction = (x)
#define PF_SET_CUMULATIVE_TIMER_OUTPUT_FUNCTION(x) profiler::defaultCumulativeTimerOutputFunction = (x)
#define PF_SET_AVERAGE_TIMER_INFO_OUTPUT_FUNCTION(x) profiler::defaultAverageTimerInfoOutputFunction = (x)

#define PF_SET_AVERAGE_TIMER_SLEEP_DURATION(x) profiler::defaultAverageTimerSleepDuration = (x)
#define PF_SET_CUMULATIVE_TIMER_SLEEP_DURATION(x) profiler::defaultCumulativeTimerSleepDuration = (x)

#define PF_SET_PROFILER_START_TIME() profiler::AverageTimerManager::setStartTime(profiler::profilerClock::now())
