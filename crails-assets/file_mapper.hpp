#pragma once
#include <map>
#include <string>
#include <boost/filesystem.hpp>

enum CompressionStrategy
{
  Gzip,
  Brotli,
  AllCompressions,
  NoCompression
};

struct FileMapper : public std::map<std::string, std::string>
{
  std::map<std::string, std::string> aliases;

  bool               get_key_from_alias(const std::string& alias, std::string& key) const;
  const std::string& get_alias(const std::string& key) const { return aliases.at(key); }
  void               set_alias(const std::string& key, boost::filesystem::path directory)
  {
    const std::string& container = directory.string();

    aliases.emplace(key, key.substr(container.length() + 1));
  }

  bool        output_to(const std::string& output_directory, CompressionStrategy compression);
  bool        collect_files(boost::filesystem::path directory, const std::string& pattern) { return collect_files(directory, directory, pattern); }

protected:
  bool        collect_files(boost::filesystem::path root, boost::filesystem::path directory, const std::string& pattern);
  bool        generate_checksum(const boost::filesystem::path& root, const boost::filesystem::path& source);
  bool        generate_file(const boost::filesystem::path& input_path, const boost::filesystem::path& output_path);
  std::string inject_asset_path(const std::string& data);
};
