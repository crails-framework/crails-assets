#include <iostream>
#include <boost/program_options.hpp>
#include <boost/process.hpp>
#include <string_view>
#include <regex>
#include <crails/utils/split.hpp>
#include <cstdlib>
#include "file_mapper.hpp"
#include "compression.hpp"
#include "exclusion_pattern.hpp"

bool update_reference_files(const FileMapper& file_map, std::string_view output_path, const ExclusionPattern&);
bool generate_reference_files(const FileMapper& file_map, std::string_view output_path, const ExclusionPattern&);
bool generate_public_folder(FileMapper& filemap, const std::string& output_directory, CompressionStrategy strategy);

bool verbose_mode = false;
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
    ("ifndef",        boost::program_options::value<std::string>(), "exclude some assets from a C++ build based on a define (ex: --ifndef __CHEERP_CLIENT__:application.js:application.js.map)")
    ("sourcemaps,d",  boost::program_options::value<bool>(),        "generates sourcemaps (true by default)")
    ("update,u", "append or update to the existing asset register instead of generating a new register")
    ("verbose,v", "enable verbose mode")
    ("help,h", "display help message");
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), options);
  boost::program_options::notify(options);
  if (options.count("help"))
  {
    std::cout << desc << std::endl;
    return 0;
  }
  verbose_mode = options.count("verbose");
  if (options.count("inputs") && options.count("output"))
  {
    std::string pattern(".*");
    auto        directory_options = options["inputs"].as<std::vector<std::string>>();
    std::string output = options["output"].as<std::string>();
    CompressionStrategy compression = options.count("compression") ? get_compression_strategy(options["compression"].as<std::string>()) : Gzip;
    ExclusionPattern exclusion_pattern;

    if (options.count("sourcemaps"))
      with_source_maps = options["sourcemaps"].as<bool>();
    if (options.count("ifndef"))
      exclusion_pattern = ExclusionPattern(options["ifndef"].as<string>());
    for (const std::string& directory_option : directory_options)
    {
      std::string alias;
      std::string directory;

      extract_alias_from_directory_option(directory_option, directory, alias);
      if (verbose_mode)
        std::cout << "[crails-assets] collecting files from directory: " << directory << std::endl;
      if (files.collect_files(std::filesystem::path(directory), alias, pattern))
        continue ;
      else
        return -1;
    }
    if (verbose_mode)
      std::cout << "[crails-assets] outputing files to " << output << std::endl;
    if (generate_public_folder(files, output, compression))
    {
      bool success;
      const char* autogen_folder_var = std::getenv("CRAILS_AUTOGEN_DIR");
      string autogen_folder = autogen_folder_var ? autogen_folder_var : "app/autogen";

      if (verbose_mode)
        std::cout << "[crails-assets] outputing reference files to " << autogen_folder << std::endl;
      success = options.count("update")
        ? update_reference_files(files, autogen_folder, exclusion_pattern)
        : generate_reference_files(files, autogen_folder, exclusion_pattern);
      return success ? 0 : -1;
    }
  }
  else
    std::cerr << "inputs and output arguments are required" << std::endl;
  return -1;
}
