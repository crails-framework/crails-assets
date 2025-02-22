#include <string>
#include <map>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <crails/cli/process.hpp>
#include <crails/utils/split.hpp>
#include <crails/read_file.hpp>
//#include <boost/process.hpp>
#include <chrono>

std::string filepath_to_varname(const std::string& filepath);

extern std::map<std::string,std::string> compression_strategies;
extern std::string tmp_path;
std::string xxd_tmp_path = "/tmp/xxd_output";

using namespace std;
using namespace std::chrono_literals;

static string replace_all(std::string str, const std::string& from, const std::string& to)
{
  size_t start_pos = 0;
  while((start_pos = str.find(from, start_pos)) != std::string::npos)
  {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
  return str;
}

static void compress_asset(const std::string& strategy, const std::string& filepath)
{
  std::string command = strategy + " -kc " + filepath;
  std::string result;

  std::cout << "+ " << command << std::endl;
  std::system((command + " > " + tmp_path).c_str());
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
  // Use xxd to append the files as binary in the C++ file
  for (const auto& file : files)
  {
    std::string command, output;
    std::ofstream source;

    source.open(source_path, std::ios_base::app);
    source << "const char* " << classname << "::" << filepath_to_varname(file.second) << " = \""
           << uri_root << file.second << "\";" << std::endl
           << "static const ";
    compress_asset(compression_strategy, file.first);
    // Older versions of xxd don't support the option -n
    //command = "xxd -n " + filepath_to_varname(file.second) + " -i " + tmp_path;
    // PATCH for older versions of xxd
    command = "xxd -i " + tmp_path;
    // END PATCH for older versions of xxd
    std::cout << "+ " << command << std::endl;

    // the boost::process systematically truncates the output for some files,
    // so we're working around it by using std::system and shell redirections
    // instead, then calling Crails::read_file.
    std::system((command + " > " + xxd_tmp_path).c_str());
    Crails::read_file(xxd_tmp_path, output);

    // PATCH for older versions of xxd
    filepath_to_varname(tmp_path);
    output = replace_all(output, filepath_to_varname(tmp_path), filepath_to_varname(file.second));
    // END PATCH for older versions of xxd

    source << output;
    source.close();
  }
  // Makes the length variables static and const
  {
    std::string command, output;
   
    command =  std::string("awk ")
      + '\''
      + "{ if ($0 ~ /^unsigned/) { gsub(\"unsigned\", \"static const unsigned\"); print $0 } else { print $0 } }"
      + '\''
      + ' ' + source_path
      + " > /tmp/tmpfile";
    std::cout << command << std::endl;
    std::system(command.c_str());
    Crails::read_file("/tmp/tmpfile", output);
    {
      std::ofstream source;
      source.open(source_path);
      source << output;
      source.close();
    }
  }
  // Append the constructor for BuiltinAssets
  {
    std::ofstream source;
    source.open(source_path, std::ios_base::app);
    source << classname << "::" << classname << "() : Crails::BuiltinAssets(\""
           << uri_root << "\", "
           << '"' << compression_strategy << "\")" << std::endl;
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
