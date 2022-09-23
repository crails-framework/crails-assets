#include <filesystem>
#include <sstream>
#include <crails/cli/process.hpp>
#include <crails/cli/filesystem.hpp>
#include <crails/read_file.hpp>
#include "file_mapper.hpp"

extern bool with_source_maps;

std::string public_path_for(const std::pair<std::string,std::string>& name_and_checksum);

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

bool generate_js(const boost::filesystem::path& input_path, const boost::filesystem::path& output_path, const FileMapper& filemap)
{
  auto uglify_impl = Crails::which("uglifyjs");
  std::string contents;

  Crails::read_file(input_path.string(), contents);
  replace_wasm_in_comet_javascript(input_path.string(), filemap, contents);
  if (uglify_impl.length() > 0)
  {
    std::stringstream command;
    std::string temporary_file = std::filesystem::temp_directory_path().string() + "/tmp.js";

    Crails::write_file("crails-assets", temporary_file, contents);
    command << uglify_impl << ' ' << temporary_file
      << " -o \"" << output_path.string() << '"'
      << " --compress";
    if (with_source_maps)
      command << " --source-map \"" << output_path.string() << ".map\"";
    return Crails::run_command(command.str()) == 0;
  }
  return Crails::write_file("crails-assets", output_path.string(), contents);
}
