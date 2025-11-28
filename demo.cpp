#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include "profiler.hpp"  

void customProfilerOutput(const std::string& id, profiler::profilerClock::duration t) {
    *profiler::defaultProfilerOutputStream << "(custom output) " 
                                           << id << " took " 
                                           << std::chrono::duration<double>(t).count() * profiler::profilerDurationScale 
                                           << profiler::getUnitSuffix(profiler::profilerDurationScale) << "\n";
}

void customAverageTimerOutput(profiler::profilerClock::duration start) {
    *profiler::defaultProfilerOutputStream << "(custom output) elapsed time: "
                                           << std::chrono::duration<double>(profiler::profilerClock::now() - start).count() * profiler::profilerDurationScale 
                                           << profiler::getUnitSuffix(profiler::profilerDurationScale) << "\n";
}

int main() {

    // manual timer 
    profiler::Timer t;
    t.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto duration = t.stop();
    std::cout << "manual timer took: " << std::chrono::duration<double>(duration).count()  * profiler::profilerDurationScale << "s.\n";

    // scope timer
    std::cout << "scope timer:\n";
    {
        PF_SCOPE_TIMER("scope timer");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // average timer manual logging
    PF_SET_AVERAGE_TIMER_START_TIME();
    std::cout << "\naverage timer, manual logging:\n";
    for(int i = 0; i < 20; i++){
        PF_AVERAGE_TIMER("avg timer");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    PF_AVERAGE_TIMER_LOG();


    // average timer automatic logging
    PF_ENABLE_AVERAGE_TIMER_AUTO_LOG();
    std::cout << "\naverage timer, automatic logging:\n";
    for(int i = 0; i < 50; i++){
        PF_AVERAGE_TIMER("avg timer");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // change automatic logging sleep duration
    PF_SET_AVERAGE_TIMER_SLEEP_DURATION(std::chrono::milliseconds(500));
    std::cout << "\naverage timer, automatic logging, 500ms sleep:\n";
    for(int i = 0; i < 30; i++){
        PF_AVERAGE_TIMER("avg timer");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // change profiler clock
    PF_SET_PROFILER_CLOCK(std::chrono::system_clock);

    // change profiler duration unit (seconds, milliseconds, etc)
    PF_SET_PROFILER_DURATION_UNIT(std::chrono::minutes);

    // change output functions
    PF_SET_OUTPUT_FUNCTION(customProfilerOutput);
    PF_SET_AVERAGE_TIMER_INFO_OUTPUT_FUNCTION(customAverageTimerOutput);
    std::cout << "\naverage timer, automatic logging, 500ms sleep, milliseconds duration unit, custom output functions:\n";
    for(int i = 0; i < 30; i++){
        PF_AVERAGE_TIMER("avg timer");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // change output stream
    std::ofstream file("output.txt");
    PF_SET_OUTPUT_STREAM(&file);
    std::cout << "\nchanged output stream to file, check output.txt\n";
    file << "average timer, automatic logging, 500ms sleep, milliseconds duration unit, custom output functions, output to file:\n";
    for(int i = 0; i < 30; i++){
        PF_AVERAGE_TIMER("avg timer");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    file.close();

}
