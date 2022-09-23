#include "file_mapper.hpp"
#include <boost/process.hpp>
#include <sstream>
#include <regex>
#include <iostream>

bool FileMapper::get_key_from_alias(const std::string& alias, std::string& key) const
{
  for (auto it = aliases.begin() ; it != aliases.end() ; ++it)
  {
    if (it->second == alias)
    {
      key = it->first;
      return true;
    }
  }
  return false;
}

bool FileMapper::collect_files(std::filesystem::path root, std::filesystem::path directory, const std::string& scope, const std::string& pattern)
{
  if (std::filesystem::is_directory(directory))
  {
    std::filesystem::recursive_directory_iterator dir(directory);
    std::regex matcher(pattern.c_str());

    for (auto& entry : dir)
    {
      std::string filename = entry.path().filename().string();
      auto match = std::sregex_iterator(filename.begin(), filename.end(), matcher);

      if (std::filesystem::is_directory(entry.path()))
      {
        if (collect_files(root, entry.path(), scope, pattern))
          continue ;
        return false;
      }
      if (match == std::sregex_iterator())
        continue ;
      if (!generate_checksum(root, entry.path(), scope))
        return false;
    }
  }
  return true;
}

bool FileMapper::generate_checksum(const std::filesystem::path& root, const std::filesystem::path& source, const std::string& scope)
{
  boost::process::ipstream pipe_stream;
  boost::process::child    sum_process(
    "md5sum " + source.string(),
    boost::process::std_out > pipe_stream
  );
  std::string line;
  char        sum[32];

  std::getline(pipe_stream, line);
  if (line.length() > 32)
  {
    std::strncpy(sum, line.c_str(), 32);
    emplace(source.string(), std::string(sum, 32));
    set_alias(source.string(), root, scope);
  }
  else
  {
    std::cerr << "Failed to generate checksum for " << source.string() << std::endl;
    return false;
  }
  return true;
}
