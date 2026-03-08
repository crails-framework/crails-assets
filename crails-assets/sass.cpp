#include <iostream>
#include <string_view>
#include <regex>
#include <filesystem>
#include <crails/cli/filesystem.hpp>
#include <crails/cli/process.hpp>

extern bool with_source_maps;

static const std::vector<std::string> sass_candidates{"scss", "sass", "node-sass"};
static const std::map<std::string, std::string> sass_options{
  {"scss",      "--style compressed"},
  {"sass",      "--style compressed"},
  {"node-sass", "--output-style compressed"}
};

static const std::map<std::string, std::string> sass_sourcemap_options{
  {"scss",      "--sourcemap=inline"},
  {"sass",      "--source-map --embed-source-map"},
  {"node-sass", "--source-map-embed"}
};

static std::pair<std::string, std::string> find_sass()
{
  for (const std::string& candidate : sass_candidates)
  {
    std::string result = Crails::which(candidate);

    if (result.length() > 0)
      return {candidate, result};
  }
  return {"", ""};
}

static std::string sass_command(const std::pair<std::string, std::string>& sass_impl, const std::filesystem::path& input)
{
  std::stringstream stream;

  stream << sass_impl.second << ' ' << sass_options.at(sass_impl.first) << ' ';
  if (with_source_maps)
    stream << sass_sourcemap_options.at(sass_impl.first) << ' ';
  stream << input.string();
  return stream.str();
}

bool generate_sass(const std::filesystem::path& input_path, const std::filesystem::path& output_path, std::function<std::string(const std::string&)> post_filter)
{
  auto sass_impl = find_sass();

  if (input_path.filename().string()[0] == '_')
    return true;
  if (sass_impl.first.length() > 0)
  {
    std::string output, injected_source;
    std::string cmd = sass_command(sass_impl, input_path);

    std::cout << "[crails-assets] sass command: " << cmd << std::endl;
    if (!Crails::run_command(cmd, output))
      return false;
    injected_source = post_filter(output);
    if (injected_source.length() == 0)
      return false;
    Crails::write_file("crails-assets", output_path.string(), injected_source);
    std::cout << "[crails-sass] generated css for `" << input_path.string() << "` at `" << output_path.string() << '`' << std::endl;
    return true;
  }
  else
  {
    std::cerr << "Cannot convert `" << input_path.string() << "` to css. Sass not found." << std::endl;
    std::cerr << "Looked for:" << std::endl;
    for (const std::string& candidate : sass_candidates) std::cerr << "- " << candidate << std::endl;
  }
  return false;
}

