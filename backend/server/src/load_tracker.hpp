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

#include "async_server.hpp"

class AsyncServer;

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

struct DiskStats {
  unsigned long long readBytes, writeBytes;
};

// Function to parse CPU stats from /proc/stat
CPUStats GetCPUStats();

// Function to parse network stats from /proc/net/dev
NetStats GetNetStats();
// Function to parse disk stats from /proc/diskstats
DiskStats GetDiskStats();

// Function to get current timestamp as a string
std::string GetT();

void START_COLLECTION(const std::string &logFile, AsyncServer *server,
                      CacheClient *cache_client);

void END_COLLECTION();

double get_cpu_load();