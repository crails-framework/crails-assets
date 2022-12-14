#include "file_mapper.hpp"
#include "compression.hpp"
#include <boost/process.hpp>
#include <filesystem>
#include <regex>
#include <iostream>

typedef std::function<std::string(const std::string&)> PostFilter;

extern bool verbose_mode;

const std::string public_scope = "assets/";

bool generate_sass(const std::filesystem::path& input_path, const std::filesystem::path& output_path, PostFilter post_filter);
bool generate_js(const std::filesystem::path& input_path, const std::filesystem::path& output_path, const FileMapper&);

static std::string filename_with_checksum(const std::pair<std::string, std::string>& name_and_checksum)
{
  std::filesystem::path filepath(name_and_checksum.first);

  if (filepath.has_stem())
    return filepath.stem().string() + '-' + name_and_checksum.second + filepath.extension().string();
  return filepath.filename().string() + '-' + name_and_checksum.second;
}

std::string public_path_for(const std::pair<std::string,std::string>& name_and_checksum)
{
  return '/' + public_scope + filename_with_checksum(name_and_checksum);
}

static std::string inject_asset_path(const FileMapper& filemap, const std::string& data)
{
  std::regex  pattern("asset_path\\(([^)])+\\)");
  auto        match = std::sregex_iterator(data.begin(), data.end(), pattern);
  std::string result;
  std::size_t last_pos = 0;
  std::size_t prefix_len = 11;

  while (match != std::sregex_iterator())
  {
    std::string asset_path = data.substr(match->position() + prefix_len, match->length() - prefix_len - 1);
    std::string asset_key;

    if (filemap.get_key_from_alias(asset_path, asset_key))
    {
      std::string public_asset_path = public_path_for({asset_key, filemap.at(asset_key)});

      result += data.substr(last_pos, match->position());
      result += public_asset_path;
      last_pos = match->position() + match->length();
    }
    else
    {
      std::cout << "inject_asset_path: asset not found: " << asset_path << std::endl;
      return "";
    }
    match++;
  }
  result += data.substr(last_pos);
  return result;
}

static bool copy_file(const std::filesystem::path& input_path, const std::filesystem::path& output_path)
{
  std::error_code ec;
  std::filesystem::copy_file(input_path, output_path, std::filesystem::copy_options::none, ec);
  if (verbose_mode)
  {
    if (ec)
      std::cout << "[crails-assets] No changes with " << input_path.string() << std::endl;
    else
      std::cout << "[crails-assets] Copied `" << input_path.string() << "` to `" << output_path.string() << '`' << std::endl;
  }
  return true;
}

static bool generate_file(const FileMapper& filemap, const std::filesystem::path& input_path, const std::filesystem::path& output_path)
{
  std::string extension = input_path.extension().string();
  PostFilter post_filter = std::bind(&inject_asset_path, filemap, std::placeholders::_1);

  if (extension == ".scss" || extension == ".sass")
    return generate_sass(input_path, output_path, post_filter);
  if (extension == ".js")
    return generate_js(input_path, output_path, filemap);
  return ::copy_file(input_path, output_path);
}

bool generate_public_folder(FileMapper& filemap, const std::string& output_directory, CompressionStrategy compression)
{
  std::vector<std::shared_ptr<boost::process::child>> compression_processes;
  std::filesystem::path output_base(output_directory + '/' + public_scope);

  if (!std::filesystem::is_directory(output_base))
  {
    if (!std::filesystem::create_directories(output_base))
    {
      std::cerr << "Cannot open directory " << output_base.string();
      return false;
    }
  }
  for (auto it = filemap.begin() ; it != filemap.end() ;)
  {
    std::filesystem::path input_path(it->first);
    std::filesystem::path output_path(output_base.string() + filename_with_checksum(*it));
    std::shared_ptr<boost::process::child> process;
    std::vector<CompressionStrategy> strategies = {compression};

    // If the name finishes with .map, it is a map file, and needs to be named after the file it maps
    if (it->first.substr(it->first.length() - 4) == ".map")
    {
      auto mapped_file = filemap.find(it->first.substr(0, it->first.length() - 4));
      if (mapped_file == filemap.end())
      {
        std::cerr << "[crails-assets] could not find mapped file for " << it->first << std::endl;
        return false;
      }
      output_path = output_base.string() + filename_with_checksum(*mapped_file) + ".map";
    }

    // If a file with that name already exists, then the file hasn't changed since the last run
    if (std::filesystem::exists(output_path))
    {
      ++it;
      continue ;
    }

    // Attempt to generate file in the public directory
    if (!generate_file(filemap, input_path, output_path))
    {
      std::cerr << "[crails-assets] you have an issue to fix in " << input_path.string() << std::endl;
      return false;
    }

    // If no file has been generated, remove it from the FileMapper
    if (!std::filesystem::exists(output_path))
    {
      it = filemap.erase(it);
      continue ;
    }
    ++it;

    // Apply compression on the generated file
    if (compression == NoCompression)
      continue ;
    else if (compression == AllCompressions)
      strategies = {Gzip, Brotli};
    for (auto compression : strategies)
    {
      process = std::make_shared<boost::process::child>(compress_command(compression, output_path));
      compression_processes.push_back(process);
    }
  }
  //
  // Wait for the compression processes to end
  //
  for (auto process : compression_processes)
  {
    process->wait();
    if (process->exit_code() != 0)
      return false;
  }
  return true;
}
