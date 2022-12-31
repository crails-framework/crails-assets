#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <crails/cli/process.hpp>
#include <crails/utils/split.hpp>
#include <crails/read_file.hpp>
#include <boost/process.hpp>
#include <chrono>

std::string filepath_to_varname(const std::string& filepath);

extern std::map<std::string,std::string> compression_strategies;
extern std::string tmp_path;
std::string xxd_tmp_path = "/tmp/xxd_output";

using namespace std;
using namespace std::chrono_literals;

// process.wait(), when called from this program, tends to
// hang undefinitely... hence the need to re-implement Crails::run_command...
// despite the fact that Crails::run_command itself was copied and pasted
// from the boost documentation:
// https://www.boost.org/doc/libs/1_67_0/doc/html/boost_process/tutorial.html
bool my_run_command(const string& command, string& result)
{
  boost::process::ipstream stream;
  boost::process::child process(command, boost::process::std_out > stream);
  string line;
  unsigned int i = 0;

  while (process.running() && getline(stream, line) && !line.empty())
  {
    result += line + '\n';
    i++;
  }
  stream.close();
  process.wait_for(5s);
  return process.exit_code() == 0;
}

static void compress_asset(const std::string& strategy, const std::string& filepath)
{
  std::string command = strategy + " -kc " + filepath;
  std::string result;
  std::ofstream output;

  //command = "cat " + filepath;
  std::cout << "+ " << command << std::endl;

  my_run_command(command, result);
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

    // the boost::process systematically truncates the output for some files,
    // so we're working around it by leaving the output redirection to 'sh'
    // instead, then calling Crails::read_file.
    command = "sh -c \"" + command + " > " + xxd_tmp_path + '"';
    my_run_command(command, output);
    Crails::read_file(xxd_tmp_path, output);
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
