#include "file_mapper.hpp"
#include <crails/cli/process.hpp>
#include <boost/uuid/detail/md5.hpp>
#include <sstream>
#include <regex>
#include <cstring>
#include <iostream>

using namespace std;

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

bool FileMapper::collect_file(std::filesystem::path root, std::filesystem::path filepath, const std::string& scope, const std::string& pattern)
{
  std::regex matcher(pattern.c_str());
  std::string filename = filepath.filename().string();
  auto match = std::sregex_iterator(filename.begin(), filename.end(), matcher);

  if (std::filesystem::is_directory(filepath))
  {
    if (collect_files(root, filepath, scope, pattern))
      return true;
    return false;
  }
  else if (match != std::sregex_iterator())
    return generate_checksum(root, filepath, scope);
  return true;
}

bool FileMapper::collect_files(std::filesystem::path root, std::filesystem::path directory, const std::string& scope, const std::string& pattern)
{
  if (std::filesystem::is_directory(directory))
  {
    std::filesystem::recursive_directory_iterator dir(directory);

    roots.push_back(directory);
    for (auto& entry : dir)
    {
      if (!collect_file(root, entry.path(), scope, pattern))
        return false;
    }
  }
  else if (std::filesystem::is_regular_file(directory))
  {
    if (root == directory)
      root = directory.parent_path();
    if (!collect_file(root, directory, scope, pattern))
      return false;
  }
  return true;
}

bool FileMapper::generate_checksum(const std::filesystem::path& root, const std::filesystem::path& source, const std::string& scope)
{
  std::string line;
  char        sum[32];

  Crails::run_command({"md5sum", {source.string()}}, line);
  if (line.length() > 32)
  {
    std::strncpy(sum, line.c_str(), 32);
    emplace(source.string(), std::string(sum, 32));
    set_alias(source.string(), root, scope);
    source_checksums[source.string()] = sum;
  }
  else
  {
    std::cerr << "Failed to generate checksum for " << source.string() << std::endl;
    return false;
  }
  return true;
}

vector<string> FileMapper::dependencies_of(const string& key, const map<string,string>& index) const
{
  filesystem::path path(key);
  string           ext = path.extension().string();

  if (ext == ".js")
    return {filesystem::path(path).replace_extension("wasm")};
  if (ext == ".scss" && ext == ".sass")
    return sass_dependencies_of(key, index);
  return {};
}

static std::string md5_hex(const std::string& data)
{
  boost::uuids::detail::md5 hash;
  boost::uuids::detail::md5::digest_type digest;
  static const char hex[] = "0123456789abcdef";
  std::string out;

  hash.process_bytes(data.data(), data.size());
  hash.get_digest(digest);
  auto bytes = reinterpret_cast<const unsigned char*>(&digest);
  for (int i = 0 ; i < 16 ; ++i) { out += hex[bytes[i] >> 4]; out += hex[bytes[i] & 15]; }
  return out;
}

string FileMapper::combined_checksum(const string& key, const map<string,string>& index, map<string,string>& memo, set<string>& visiting) const
{
  vector<string> deps;
  auto           cached = memo.find(key);
  string         material;

  if (cached != memo.end())
    return cached->second;
  if (!visiting.insert(key).second)
    return source_checksums.at(key);
  deps = dependencies_of(key, index);
  material = source_checksums.at(key);
  for (const string& dep : deps)
    material += ':' + combined_checksum(dep, index, memo, visiting);
  visiting.erase(key);
  return memo[key] = deps.empty() ? source_checksums.at(key) : md5_hex(material);
}

void FileMapper::resolve_dependency_checksums()
{
  std::map<std::string, std::string> index, memo;
  std::set<std::string> visiting;

  for (const auto& entry : source_checksums)
    index.emplace(std::filesystem::path(entry.first).lexically_normal().string(), entry.first);
  for (auto& entry : *this)
    entry.second = combined_checksum(entry.first, index, memo, visiting);
}
