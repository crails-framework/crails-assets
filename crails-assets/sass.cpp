#include <iostream>
#include <string_view>
#include <regex>
#include <filesystem>
#include <map>
#include <sstream>
#include <functional>
#include <crails/cli/filesystem.hpp>
#include <crails/cli/process.hpp>
#include "file_mapper.hpp"

using namespace std;

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

static void aggregate(std::vector<std::string>& deps, const std::map<std::string,std::string>& index, const std::filesystem::path& candidate)
{
  auto it = index.find(candidate.lexically_normal().string());
  if (it != index.end())
    deps.push_back(it->second);
}

std::vector<std::string> FileMapper::sass_dependencies_of(const std::string& key, const std::map<std::string,std::string>& index) const
{
  using namespace std;
  static const std::regex import_stmt(R"(@(?:import|use|forward)\s+([^;\n]+))");
  static const std::regex quoted(R"(["']([^"']+)["'])");
  static const std::regex asset_call{R"re(asset_path\("([^"]+)"\))re"};

  filesystem::path path(key);
  vector<string>   deps;
  auto             add = bind(&aggregate, ref(deps), ref(index), placeholders::_1);
  string           source;

  Crails::read_file(key, source);
  for (auto m = sregex_iterator(source.begin(), source.end(), import_stmt) ; m != sregex_iterator() ; ++m)
  {
    string args = (*m)[1].str();
    for (auto q = sregex_iterator(args.begin(), args.end(), quoted) ; q != sregex_iterator() ; ++q)
    {
      filesystem::path name((*q)[1].str());
      if (name.string().rfind("sass:", 0) == 0 || name.string().rfind("http", 0) == 0)
        continue;
      vector<filesystem::path> bases{path.parent_path()};
      bases.insert(bases.end(), roots.begin(), roots.end());
      for (const filesystem::path& base : bases)
      {
        for (const char* suffix : {"", ".scss", ".sass", ".css"})
        {
          add(base / name.parent_path() / (name.filename().string() + suffix));
          add(base / name.parent_path() / ('_' + name.filename().string() + suffix));
        }
      }
    }
  }
  for (auto m = sregex_iterator(source.begin(), source.end(), asset_call) ; m != sregex_iterator() ; ++m)
  {
    string dep_key;
    if (get_key_from_alias((*m)[1].str(), dep_key))
      deps.push_back(dep_key);
  }
  std::sort(deps.begin(), deps.end());
  deps.erase(std::unique(deps.begin(), deps.end()), deps.end());
  return deps;
}
