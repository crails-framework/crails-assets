#pragma once
#include <map>
#include <string>
#include <filesystem>

struct FileMapper : public std::map<std::string, std::string>
{
  std::map<std::string, std::string> aliases;

  bool               get_key_from_alias(const std::string& alias, std::string& key) const;
  const std::string& get_alias(const std::string& key) const { return aliases.at(key); }
  void               set_alias(const std::string& key, std::filesystem::path directory, const std::string& scope)
  {
    const std::string& container = directory.string();

    aliases.emplace(key, scope + key.substr(container.length() + 1));
  }

  bool        collect_files(std::filesystem::path directory, const std::string& scope, const std::string& pattern) { return collect_files(directory, directory, scope, pattern); }
protected:
  bool        collect_files(std::filesystem::path root, std::filesystem::path directory, const std::string& scope, const std::string& pattern);
  bool        generate_checksum(const std::filesystem::path& root, const std::filesystem::path& source, const std::string& scope);
};
