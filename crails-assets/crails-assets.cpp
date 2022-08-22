#include <iostream>
#include <boost/program_options.hpp>
#include <boost/process.hpp>
#include <string_view>
#include <regex>
#include <crails/render_file.hpp>
#include "file_mapper.hpp"
#include "compression.hpp"

bool generate_reference_files(const FileMapper& file_map, std::string_view output_path, const std::vector<std::string>& blacklist);
bool generate_public_folder(FileMapper& filemap, const std::string& output_directory, CompressionStrategy strategy);

bool with_source_maps = true;

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

static void extract_alias_from_directory_option(const std::string& option, std::string& directory, std::string& alias)
{
  unsigned int i = 0;

  directory = option;
  for (; i < option.length() ; ++i)
  {
    if (option[i] == ':')
    {
      alias = option.substr(0, i);
      directory = option.substr(i + 1);
      if (alias.length() > 0 && alias[alias.length() - 1] != '/')
        alias += '/';
      break ;
    }
  }
}

int main (int argc, char* argv[])
{
  using namespace std;
  FileMapper files;
  boost::program_options::options_description desc("Options");
  boost::program_options::variables_map options;

  desc.add_options()
    ("inputs,i",      boost::program_options::value<std::vector<std::string>>(), "list of input folders. You may prefix each path with an alias, separated by a colon.")
    ("output,o",      boost::program_options::value<std::string>(), "output folder")
    ("compression,c", boost::program_options::value<std::string>(), "gzip, brotli, all or none; defaults to gzip")
    ("sourcemaps,d",  boost::program_options::value<bool>(),        "generates sourcemaps (true by default)");
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), options);
  boost::program_options::notify(options);
  if (options.count("inputs") && options.count("output"))
  {
    std::string pattern(".*");
    auto        directory_options = options["inputs"].as<std::vector<std::string>>();
    std::string output = options["output"].as<std::string>();
    CompressionStrategy compression = options.count("compression") ? get_compression_strategy(options["compression"].as<std::string>()) : Gzip;

    if (options.count("sourcemaps"))
      with_source_maps = options["sourcemaps"].as<bool>();
    for (const std::string& directory_option : directory_options)
    {
      std::string alias;
      std::string directory;

      extract_alias_from_directory_option(directory_option, directory, alias);
      std::cout << "Collecting files from directory: " << directory << std::endl;
      if (files.collect_files(boost::filesystem::path(directory), alias, pattern))
        continue ;
      else
        return -1;
    }
    std::cout << "Outputing files to " << output << std::endl;
    generate_public_folder(files, output, compression);
    generate_reference_files(files, "lib/", {});
    return 0;
  }
  else
    std::cerr << "inputs and output arguments are required" << std::endl;
  return -1;
}
