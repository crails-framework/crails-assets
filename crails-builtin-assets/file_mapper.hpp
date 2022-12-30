#pragma once
#include <filesystem>
#include <map>

class FileMapper : public std::map<std::string, std::string>
{
public:
  void collect_files(std::filesystem::path directory);
private:
  void collect_files(std::filesystem::path root, std::filesystem::path path);
};
