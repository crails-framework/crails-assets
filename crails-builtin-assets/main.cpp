#include "file_mapper.hpp"
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>

void generate_header(const std::string& path, const std::string& classname, const std::map<std::string, std::string>& files);
void generate_source(const std::string& path, const std::string& classname, const std::string& compression_strategy, const std::string& uri_root, const std::map<std::string, std::string>& files);

std::map<std::string,std::string> compression_strategies{
  {"gzip","gz"},{"brotli","br"}
};

std::string tmp_path("/tmp/crails-builtin-asset");

int main(int argc, const char** argv)
{
  boost::program_options::options_description desc("Options");
  boost::program_options::variables_map options;

  desc.add_options()
    ("inputs,i", boost::program_options::value<std::vector<std::string>>(), "list of inputs folders")
    ("output,o", boost::program_options::value<std::string>(), "output source and header filename, without extension")
    ("classname,c", boost::program_options::value<std::string>(), "classname for the builtin asset library")
    ("compression,z", boost::program_options::value<std::string>(), "compression strategy (gzip or brotli)")
    ("uri-root,u", boost::program_options::value<std::string>(), "uri root")
    ("help,h", "display this help message");
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), options);
  boost::program_options::notify(options);
  if (options.count("help"))
  {
    std::cout << desc << std::endl;
    return 0;
  }
  else if (!options.count("compression") || compression_strategies.find(options["compression"].as<std::string>()) == compression_strategies.end())
    std::cout << "invalid compression strategies (supported values are gzip or brotli)" << std::endl;
  else if (!options.count("uri-root"))
    std::cout << "missing uri-root" << std::endl;
  else if (options.count("inputs") && options.count("output"))
  {
    FileMapper files;
    auto output = options["output"].as<std::string>();
    auto classname = options["classname"].as<std::string>();
    auto compression = options["compression"].as<std::string>();
    auto uri_root = options["uri-root"].as<std::string>();

    for (const std::string& path : options["inputs"].as<std::vector<std::string>>())
      files.collect_files(path);
    generate_header(output, classname, files);
    generate_source(output, classname, compression, uri_root, files);
    return 0;
  }
  else
    std::cerr << "invalid options" << std::endl;
  return -1;
}
