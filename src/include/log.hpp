#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

void logToFile(void* message) {
  std::ofstream logFile("app.log", std::ios::app);
  if (logFile.is_open()) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    logFile << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
            << " - " << message << std::endl;
    logFile.close();
  }
}
