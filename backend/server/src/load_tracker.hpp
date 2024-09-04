#pragma once

#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

struct CPUStats {
  unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;

  unsigned long long Total() const {
    return user + nice + system + idle + iowait + irq + softirq + steal;
  }

  unsigned long long Idle() const { return idle + iowait; }
};

// Function to parse CPU stats from /proc/stat
CPUStats GetCPUStats() {
  std::ifstream file("/proc/stat");
  std::string line;
  CPUStats stats = {0};

  if (file.is_open()) {
    std::getline(file, line);  // Read the first line for overall CPU stats
    sscanf(line.c_str(), "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
           &stats.user, &stats.nice, &stats.system, &stats.idle, &stats.iowait,
           &stats.irq, &stats.softirq, &stats.steal);
  }

  return stats;
}

// Function to get current timestamp as a string
std::string GetT() {
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S",
           std::localtime(&now_time));
  return std::string(buffer);
}

// Global variables to control logging thread
std::atomic<bool> keepLogging(false);
std::thread loggingThread;

void LogCPULoad(const std::string &logFile) {
  CPUStats prevStats = GetCPUStats();

  while (keepLogging) {
    std::this_thread::sleep_for(std::chrono::seconds(1));  // Wait for 1 second

    CPUStats currentStats = GetCPUStats();

    // Calculate CPU utilization
    unsigned long long totalDelta = currentStats.Total() - prevStats.Total();
    unsigned long long idleDelta = currentStats.Idle() - prevStats.Idle();

    double cpuUtilization = 1.0 - (double(idleDelta) / totalDelta);

    // Log the timestamp and CPU utilization
    std::ofstream log(logFile, std::ios_base::app);  // Append to log file
    if (log.is_open()) {
      log << GetT() << " - CPU Utilization: " << cpuUtilization * 100 << "%\n";
    }

    // Update previous stats for the next iteration
    prevStats = currentStats;
  }
}

void START_COLLECTION(const std::string &logFile) {
  keepLogging = true;
  loggingThread =
      std::thread(LogCPULoad, logFile);  // Start logging in a separate thread
}

void END_COLLECTION() {
  keepLogging = false;  // Signal the logging thread to stop
  if (loggingThread.joinable()) {
    loggingThread.join();  // Wait for the logging thread to finish
  }
}

/*
int main()
{
    START_COLLECTION("cpu_log.txt"); // Start logging CPU load

    // Simulate some work
    std::this_thread::sleep_for(std::chrono::seconds(10));

    END_COLLECTION(); // Stop logging CPU load

    return 0;
}
*/