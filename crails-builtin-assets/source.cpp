#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <crails/cli/process.hpp>
#include <crails/utils/split.hpp>

std::string filepath_to_varname(const std::string& filepath);

extern std::map<std::string,std::string> compression_strategies;
extern std::string tmp_path;

static void compress_asset(const std::string& strategy, const std::string& filepath)
{
  std::string command = strategy + " -kc " + filepath;
  std::string result;
  std::ofstream output;

  command = "cat " + filepath;
  std::cout << "+ " << command << std::endl;
  Crails::run_command(command, result);
  output.open(tmp_path);
  output << result;
  output.close();
}

void generate_source(const std::string& path, const std::string& classname, const std::string& compression_strategy, const std::string& uri_root, const std::map<std::string, std::string>& files)
{
  std::string header_path = *Crails::split(path, '/').rbegin() + ".hpp";
  std::string source_path = path + ".cpp";
  {
    std::ofstream source;
    source.open(source_path);
    source << "#include \"" << header_path << "\"" << std::endl << std::endl;
    source.close();
  }
  for (const auto& file : files)
  {
    std::string command, output;
    std::ofstream source;

    source.open(source_path, std::ios_base::app);
    source << "const char* " << classname << "::" << filepath_to_varname(file.second) << " = \""
           << uri_root << file.second << "\";" << std::endl
           << "static const ";
    compress_asset(compression_strategy, file.first);
    command = "xxd -n " + filepath_to_varname(file.second) + " -i " + tmp_path;
    std::cout << "+ " << command << std::endl;
    Crails::run_command(command, output);
    source << output;
    source.close();
  }
  {
    std::ofstream source;
    source.open(source_path, std::ios_base::app);
    source << classname << "::" << classname << "() : Crails::BuiltinAssets(\""
           << uri_root << "\", "
           << '"' << compression_strategies.at(compression_strategy) << "\")" << std::endl;
    source << '{' << std::endl;
    for (const auto& file : files)
    {
      source << "  add(\"" << file.second << "\", "
	     << "reinterpret_cast<const char*>(::" << filepath_to_varname(file.second) << "), "
	     << filepath_to_varname(file.second) << "_len);" << std::endl;
    }
    source << '}' << std::endl;
  }
}
