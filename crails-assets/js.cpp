#include <filesystem>
#include <sstream>
#include <regex>
#include <crails/cli/process.hpp>
#include <crails/cli/filesystem.hpp>
#include <crails/read_file.hpp>
#include "file_mapper.hpp"

extern bool with_source_maps;

std::string public_path_for(const std::pair<std::string,std::string>& name_and_checksum);

static const std::vector<std::string> minify_candidates{"uglifyjs", "yuicompressor"};
static const std::map<std::string, std::string> minify_options{
  {"uglifyjs",      "\"$input\" -o \"$output\" --compress"},
  {"yuicompressor", "\"$input\" -o \"$output\""}
};

static std::pair<std::string, std::string> find_minify()
{
  for (const std::string& candidate : minify_candidates)
  {
    std::string result = Crails::which(candidate);

    if (result.length() > 0)
      return {candidate, result};
  }
  return {"", ""};
}

static std::string replace_options(const std::string& source, const std::map<std::string, std::string> vars)
{
  std::string result = source;

  for (auto it = vars.begin() ; it != vars.end() ; ++it)
  {
    auto position = result.find('$' + it->first);

    if (position != std::string::npos)
      result = result.substr(0, position) + it->second + result.substr(position + it->first.length() + 1);
  }
  return result;
}

static void replace_wasm_in_comet_javascript(const std::filesystem::path& input_path, const FileMapper& filemap, std::string& contents)
{
  auto wasm_filepath = std::filesystem::path(input_path).replace_extension("wasm");
  auto wasm_pattern = '\'' + wasm_filepath.filename().string() + '\'';
  std::string wasm_key;
  std::size_t pattern_position;

  if ((pattern_position = contents.find_last_of(wasm_pattern)) != std::string::npos)
  {
    if (filemap.get_key_from_alias(wasm_filepath, wasm_key))
    {
      std::string wasm_public_path = public_path_for({wasm_key, filemap.at(wasm_filepath)});
      contents.replace(pattern_position, wasm_pattern.length(), '\'' + wasm_public_path + '\'');
    }
  }
}

static void replace_sourcemaps(std::string& contents, const std::filesystem::path& output_path)
{
  std::regex regex("//#\\s*sourceMappingURL=([^\\s])");
  auto match = std::sregex_iterator(contents.begin(), contents.end(), regex);
  std::string replacement = "//# sourceMappingUrl=" + output_path.filename().string() + ".map";

  contents = std::regex_replace(contents, regex, replacement);
}

bool generate_js(const std::filesystem::path& input_path, const std::filesystem::path& output_path, const FileMapper& filemap)
{
  auto minifier = find_minify();
  std::string contents;

  Crails::read_file(input_path.string(), contents);
  replace_sourcemaps(contents, output_path);
  replace_wasm_in_comet_javascript(input_path.string(), filemap, contents);
  if (minifier.first.length() > 0)
  {
    std::stringstream command;
    std::string temporary_file = std::filesystem::temp_directory_path().string() + "/tmp.js";

    Crails::write_file("crails-assets", temporary_file, contents);
    command << minifier.second
      << ' ' << replace_options(minify_options.at(minifier.first), {{"input", temporary_file}, {"output", output_path.string()}});
    if (minifier.first == "uglifyjs" && with_source_maps)
      command << " --source-map \"" << output_path.string() << ".map\"";
    return Crails::run_command(command.str()) == 0;
  }
  return Crails::write_file("crails-assets", output_path.string(), contents);
}
