#include <iostream>
#include <boost/program_options.hpp>
#include <boost/process.hpp>
#include <string_view>
#include <regex>
#include <crails/render_file.hpp>
#include "file_mapper.hpp"

typedef std::function<std::string(const std::string&)> PostFilter;
bool generate_sass(const boost::filesystem::path& input_path, const boost::filesystem::path& output_path, PostFilter post_filter);
bool generate_reference_files(const FileMapper& file_map, std::string_view output_path, const std::vector<std::string>& blacklist);

bool with_source_maps = true;
const std::string public_scope = "assets/";

static std::string filename_with_checksum(const std::pair<std::string, std::string>& name_and_checksum)
{
  boost::filesystem::path filepath(name_and_checksum.first);

  if (filepath.has_stem())
    return filepath.stem().string() + '-' + name_and_checksum.second + filepath.extension().string();
  return filepath.filename().string() + '-' + name_and_checksum.second;
}

std::string public_path_for(const std::pair<std::string,std::string>& name_and_checksum)
{
  return '/' + public_scope + filename_with_checksum(name_and_checksum);
}

static std::string compress_command(CompressionStrategy strategy, const boost::filesystem::path& source)
{
  std::stringstream stream;

  switch (strategy)
  {
  case Gzip:
    stream << "gzip -kf " << source.string();
    break;
  case Brotli:
    stream << "brotli -kf " << source.string();
    break ;
  default:
    break ;
  }
  return stream.str();
}

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

std::string FileMapper::inject_asset_path(const std::string& data)
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

    if (get_key_from_alias(asset_path, asset_key))
    {
      std::string public_asset_path = public_path_for({asset_key, at(asset_key)});

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

static bool copy_file(const boost::filesystem::path& input_path, const boost::filesystem::path& output_path)
{
  boost::system::error_code ec;
  boost::filesystem::copy_file(input_path, output_path, boost::filesystem::copy_option::none, ec);
  if (ec)
    std::cout << "[crails-assets] No changes with " << input_path.string() << std::endl;
  else
    std::cout << "[crails-assets] Copied `" << input_path.string() << "` to `" << output_path.string() << '`' << std::endl;
  return true;
}

bool FileMapper::generate_file(const boost::filesystem::path& input_path, const boost::filesystem::path& output_path)
{
  std::string extension = input_path.extension().string();
  PostFilter post_filter = std::bind(&FileMapper::inject_asset_path, this, std::placeholders::_1);

  if (extension == ".scss" || extension == ".sass")
    return generate_sass(input_path, output_path, post_filter);
  return ::copy_file(input_path, output_path);
}

bool FileMapper::output_to(const std::string& output_directory, CompressionStrategy compression)
{
  std::vector<std::shared_ptr<boost::process::child>> compression_processes;
  boost::filesystem::path output_base(output_directory + '/' + public_scope);

  if (!boost::filesystem::is_directory(output_base))
  {
    if (!boost::filesystem::create_directories(output_base))
    {
      std::cerr << "Cannot open directory " << output_base.string();
      return false;
    }
  }
  for (auto it = begin() ; it != end() ;)
  {
    boost::filesystem::path input_path(it->first);
    boost::filesystem::path output_path(output_base.string() + filename_with_checksum({it->first, it->second}));
    std::shared_ptr<boost::process::child> process;
    std::vector<CompressionStrategy> strategies = {compression};

    //
    // Attempt to generate file in the public directory
    //
    if (!generate_file(input_path, output_path))
      return false;
    //
    // If no file has been generated, remove it from the FileMapper
    //
    if (!boost::filesystem::exists(output_path))
    {
      it = erase(it);
      continue ;
    }
    ++it;
    //
    // Apply compression on the generated file
    //
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

bool FileMapper::collect_files(boost::filesystem::path root, boost::filesystem::path directory, const std::string& pattern)
{
  if (boost::filesystem::is_directory(directory))
  {
    boost::filesystem::recursive_directory_iterator dir(directory);
    std::regex matcher(pattern.c_str());

    for (auto& entry : dir)
    {
      std::string filename = entry.path().filename().string();
      auto match = std::sregex_iterator(filename.begin(), filename.end(), matcher);

      if (boost::filesystem::is_directory(entry.path()))
      {
        if (collect_files(root, entry.path(), pattern))
          continue ;
        return false;
      }
      if (match == std::sregex_iterator())
        continue ;
      if (!generate_checksum(root, entry.path()))
        return false;
    }
  }
  return true;
}

bool FileMapper::generate_checksum(const boost::filesystem::path& root, const boost::filesystem::path& source)
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
    set_alias(source.string(), root);
  }
  else
  {
    std::cerr << "Failed to generate checksum for " << source.string() << std::endl;
    return false;
  }
  return true;
}

static CompressionStrategy get_compression_strategy(const std::string& param)
{
  if (param == "gzip")
    return Gzip;
  else if (param == "brotli")
    return Brotli;
  else if (param == "all")
    return AllCompressions;
  else if (param != "none")
    std::cerr << "Unrecognized compression scheme. Skipping compression." << std::endl;
  return NoCompression;
}

static std::vector<std::string> find_directories(const std::string& inputs)
{
  std::vector<std::string> directories;
  unsigned int last_i = 0;

  for (unsigned int i = 0 ; i < inputs.length() ; ++i)
  {
    if (inputs[i] == ',')
    {
      char str[i];
      std::strncpy(str, &(inputs.c_str()[last_i]), i - last_i);
      directories.push_back(std::string(str, i - last_i));
      last_i = i;
    }
  }
  directories.push_back(inputs.substr(last_i));
  return directories;
}

int main (int argc, char* argv[])
{
  using namespace std;
  FileMapper files;
  boost::program_options::options_description desc("Options");
  boost::program_options::variables_map options;

  desc.add_options()
    ("inputs,i",      boost::program_options::value<std::string>(), "list of input folders")
    ("output,o",      boost::program_options::value<std::string>(), "output folder")
    ("compression,c", boost::program_options::value<std::string>(), "gzip, brotli, all or none; defaults to gzip")
    ("sourcemaps,d",  boost::program_options::value<bool>(),        "generates sourcemaps (true by default)");
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), options);
  boost::program_options::notify(options);
  if (options.count("inputs") && options.count("output"))
  {
    std::string pattern(".*");
    auto        directories = find_directories(options["inputs"].as<std::string>());
    std::string output = options["output"].as<std::string>();
    CompressionStrategy compression = options.count("compression") ? get_compression_strategy(options["compression"].as<std::string>()) : Gzip;

    if (options.count("sourcemaps"))
      with_source_maps = options["sourcemaps"].as<bool>();
    for (const std::string& directory : directories)
    {
      std::cout << "Collecting files from directory: " << directory << std::endl;
      if (files.collect_files(boost::filesystem::path(directory), pattern))
        continue ;
      else
        return -1;
    }
    std::cout << "Outputing files to " << output << std::endl;
    files.output_to(output, compression);
    generate_reference_files(files, "lib/", {});
    return 0;
  }
  else
    std::cerr << "inputs and output arguments are required" << std::endl;
  return -1;
}
