#pragma once
#include <map>
#include <set>
#include <vector>
#include <string>
#include <filesystem>

struct FileMapper : public std::map<std::string, std::string>
{
  std::map<std::string, std::string> aliases;
  std::map<std::string, std::string> source_checksums;
  std::vector<std::filesystem::path> roots;

  bool               get_key_from_alias(const std::string& alias, std::string& key) const;
  const std::string& get_alias(const std::string& key) const { return aliases.at(key); }
  void               set_alias(const std::string& key, std::filesystem::path directory, const std::string& scope)
  {
    const std::string& container = directory.string();

    aliases.emplace(key, scope + key.substr(container.length() + 1));
  }

  void        resolve_dependency_checksums(); // call after all collect_files()
  bool        collect_files(std::filesystem::path directory, const std::string& scope, const std::string& pattern) { return collect_files(directory, directory, scope, pattern); }
protected:
  bool        collect_files(std::filesystem::path root, std::filesystem::path directory, const std::string& scope, const std::string& pattern);
  bool        collect_file(std::filesystem::path root, std::filesystem::path filepath, const std::string& scope, const std::string& pattern);
  bool        generate_checksum(const std::filesystem::path& root, const std::filesystem::path& source, const std::string& scope);

  std::vector<std::string> dependencies_of(const std::string& key, const std::map<std::string,std::string>& index) const;
  std::vector<std::string> sass_dependencies_of(const std::string& key, const std::map<std::string,std::string>& index) const;
  std::string              combined_checksum(const std::string& key, const std::map<std::string,std::string>& index, std::map<std::string,std::string>& memo, std::set<std::string>& visiting) const;
};
