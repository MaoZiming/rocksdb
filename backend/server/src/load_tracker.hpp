#pragma once

#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
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

struct NetStats {
  unsigned long long recvBytes, sendBytes;
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

// Function to parse network stats from /proc/net/dev
NetStats GetNetStats() {
  std::ifstream file("/proc/net/dev");
  std::string line;
  NetStats netStats = {0};

  if (file.is_open()) {
    // Skip the first two lines (headers)
    std::getline(file, line);
    std::getline(file, line);

    while (std::getline(file, line)) {
      std::istringstream ss(line);
      std::string iface;
      unsigned long long recvBytes, recvPackets, recvErrs, recvDrop, recvFifo,
          recvFrame, recvCompressed, recvMulticast;
      unsigned long long sendBytes, sendPackets, sendErrs, sendDrop, sendFifo,
          sendColls, sendCarrier, sendCompressed;

      // Parse the line, extracting the data for each interface
      ss >> iface >> recvBytes >> recvPackets >> recvErrs >> recvDrop >>
          recvFifo >> recvFrame >> recvCompressed >> recvMulticast >>
          sendBytes >> sendPackets >> sendErrs >> sendDrop >> sendFifo >>
          sendColls >> sendCarrier >> sendCompressed;

      if (iface == "ens4:" ||
          iface == "lo:") {  // Adjust this based on your network interfaces of
                             // interest
        netStats.recvBytes += recvBytes;
        netStats.sendBytes += sendBytes;
      }
    }
  }

  return netStats;
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
  CPUStats previousStats = GetCPUStats();
  NetStats previousNetStats = GetNetStats();

  while (keepLogging) {
    std::this_thread::sleep_for(std::chrono::seconds(1));  // Wait for 1 second

    CPUStats currentStats = GetCPUStats();
    NetStats currentNetStats = GetNetStats();

    unsigned long long totalDiff = currentStats.Total() - previousStats.Total();
    unsigned long long idleDiff = currentStats.Idle() - previousStats.Idle();

    double cpuUtilization =
        (totalDiff - idleDiff) / static_cast<double>(totalDiff);

    double usrPercent = (double(currentStats.user - previousStats.user) /
                         (currentStats.Total() - previousStats.Total())) *
                        100;
    double sysPercent = (double(currentStats.system - previousStats.system) /
                         (currentStats.Total() - previousStats.Total())) *
                        100;
    double idlePercent = (double(currentStats.idle - previousStats.idle) /
                          (currentStats.Total() - previousStats.Total())) *
                         100;
    double iowaitPercent = (double(currentStats.iowait - previousStats.iowait) /
                            (currentStats.Total() - previousStats.Total())) *
                           100;
    double stealPercent = (double(currentStats.steal - previousStats.steal) /
                           (currentStats.Total() - previousStats.Total())) *
                          100;

    unsigned long long recvBytes =
        currentNetStats.recvBytes - previousNetStats.recvBytes;
    unsigned long long sendBytes =
        currentNetStats.sendBytes - previousNetStats.sendBytes;

    std::ofstream log(logFile, std::ios_base::app);  // Append to log file
    if (log.is_open()) {
      log << GetT() << " - CPU Utilization: " << cpuUtilization * 100 << "% | "
          << "usr: " << usrPercent << "%, sys: " << sysPercent << "%, "
          << "idle: " << idlePercent << "%, iowait: " << iowaitPercent << "%, "
          << "steal: " << stealPercent << "% | "
          << "Network recv: " << recvBytes << " bytes, send: " << sendBytes
          << " bytes\n";
    }

    previousStats = currentStats;
    previousNetStats = currentNetStats;
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
