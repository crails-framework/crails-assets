#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <string_view>
#include <regex>
#include <crails/render_file.hpp>

extern bool with_source_maps;

const std::vector<std::string> sass_candidates{"scss", "node-sass"};
const std::map<std::string, std::string> sass_options{
  {"scss",      "--style compressed"},
  {"node-sass", "--output-style compressed"}
};

const std::map<std::string, std::string> sass_sourcemap_options{
  {"scss",      "--sourcemap=inline"},
  {"node-sass", "--source-map-embed"}
};

static std::pair<std::string, std::string> find_sass()
{
  for (const std::string& candidate : sass_candidates)
  {
    boost::process::ipstream pipe_stream;
    boost::process::child which("which " + candidate, boost::process::std_out > pipe_stream);
    std::string result;

    std::getline(pipe_stream, result);
    which.wait();
    if (which.exit_code() == 0)
      return {candidate, result};
  }
  return {"", ""};
}

static std::string sass_command(const std::pair<std::string, std::string>& sass_impl, const boost::filesystem::path& input)
{
  std::stringstream stream;

  stream << sass_impl.second << ' ' << sass_options.at(sass_impl.first) << ' ';
  if (with_source_maps)
    stream << sass_sourcemap_options.at(sass_impl.first) << ' ';
  stream << input.string();
  return stream.str();
}

bool generate_sass(const boost::filesystem::path& input_path, const boost::filesystem::path& output_path, std::function<std::string(const std::string&)> post_filter)
{
  auto sass_impl = find_sass();

  if (input_path.filename().string()[0] == '_')
    return true;
  if (sass_impl.first.length() > 0)
  {
    Crails::RenderFile render_file;
    boost::process::ipstream pipe_stream;
    boost::process::child sass(
      sass_command(sass_impl, input_path),
      boost::process::std_out > pipe_stream
    );
    std::stringstream stream;
    std::string line, injected_source;

    while (pipe_stream && std::getline(pipe_stream, line))
      stream << line << '\n';
    sass.wait();
    if (sass.exit_code() != 0)
      return false;
    injected_source = post_filter(stream.str());
    if (injected_source.length() == 0)
      return false;
    render_file.open(output_path.string());
    render_file.set_body(injected_source.c_str(), injected_source.length());
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

