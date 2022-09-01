#pragma once
#include <crails/utils/split.hpp>
#include <vector>
#include <sstream>
#include <algorithm>
#include <functional>
#include <iostream>

class ExclusionPattern
{
public:
  ExclusionPattern()
  {
  }

  ExclusionPattern(const std::string& source)
  {
    auto parts = Crails::split(source, ':');
    if (parts.size() >= 2)
    {
      define = *parts.begin();
      std::copy(++parts.begin(), parts.end(), std::back_inserter(files));
    }
    else
      std::cerr << "invalid --ifndef parameter `" << source << '`' << std::endl;
  }

  bool matches(const std::string& file) const
  {
    return std::find(files.begin(), files.end(), file) != files.end();
  }

  void protect(const std::string& file, std::stringstream& stream, std::function<void()> callback) const
  {
    if (matches(file))
      ifndef(stream, callback);
    else
      callback();
  }

  void ifndef(std::stringstream& stream, std::function<void()> callback) const
  {
    stream << "#ifndef " << define << std::endl;
    callback();
    stream << "#endif " << std::endl;
  }

private:
  std::string define;
  std::vector<std::string> files;
};
