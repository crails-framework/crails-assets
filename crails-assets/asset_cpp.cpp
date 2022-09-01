#include <crails/render_file.hpp>
#include <sstream>
#include <iostream>
#include "file_mapper.hpp"
#include "exclusion_pattern.hpp"

std::string public_path_for(const std::pair<std::string, std::string>& name_and_checksum);

const unsigned short max_characters_in_variable_name = 255;
const std::vector<std::string> reserved_keywords{
  "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto",
  "bitand", "bitor", "bool", "break",
  "case", "catch", "char", "char16_t", "char32_t", "class", "compl", "concept", "const", "constexpr", "const_cast", "continue",
  "co_await", "co_return", "co_yield",
  "decltype", "default", "delete", "do", "double", "dynamic_cast",
  "else", "enum", "explicit", "export", "extern",
  "false", "float", "for", "friend",
  "goto",
  "if", "import", "inline", "int",
  "long",
  "module", "mutable",
  "namespace", "new", "noexcept", "not", "not_eq", "nullptr",
  "operator", "or", "or_eq",
  "private", "protected", "public",
  "register", "reinterpret_cast", "requires", "return",
  "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized",
  "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
  "union", "unsigned", "using",
  "virtual", "void", "volatile",
  "wchar_t", "while",
  "xor", "xor_eq"
};

static bool is_alphanumerical(char c)
{
  return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
}

static std::string filepath_to_varname(const std::string& filepath)
{
  std::string output;

  output.reserve(filepath.length() + 1);
  if (filepath[0] >= '0' && filepath[0] <= '9')
    output += '_';
  for (unsigned short i = 0 ; i < filepath.length() ; ++i)
    output += is_alphanumerical(filepath[i]) ? filepath[i] : '_';
  if (std::find(reserved_keywords.begin(), reserved_keywords.end(), output) != reserved_keywords.end())
    output += '_';
  return output;
}

bool generate_reference_files(const FileMapper& file_map, std::string_view output_path, const ExclusionPattern& exclusion_pattern)
{
  std::stringstream stream_hpp, stream_cpp, stream_js;
  std::string_view assets_ns = "Assets";
  Crails::RenderFile header, source, javascript;
  std::map<std::string, std::string> varname_map;

  stream_hpp << "#ifndef APPLICATION_ASSETS_HPP" << std::endl;
  stream_hpp << "#define APPLICATION_ASSETS_HPP" << std::endl;
  stream_hpp << "namespace " << assets_ns << std::endl << '{' << std::endl;
  stream_cpp << "#include \"assets.hpp\"" << std::endl;
  stream_cpp << "namespace " << assets_ns << std::endl << '{' << std::endl;
  stream_js << "export const Assets = {";
  for (auto it = file_map.begin() ; it != file_map.end() ; ++it)
  {
    std::string alias = file_map.get_alias(it->first);
    std::string varname = filepath_to_varname(alias);

    if (varname_map.find(varname) != varname_map.end())
    {
      std::cerr << "Cannot generate a variable name for `" << it->first << "`: duplicate with `" << varname_map.at(varname) << '`' << std::endl;
      return false;
    }
    varname_map.emplace(varname, it->first);
    if (varname.length() > max_characters_in_variable_name)
    {
      std::cerr << "Cannot generate a variable name for `" << it->first << "`: path is too long." << std::endl;
      return false;
    }
    exclusion_pattern.protect(it->first, stream_hpp, [&]()
    { stream_hpp << "  extern const char* " << varname << ';' << std::endl; });
    exclusion_pattern.protect(it->first, stream_cpp, [&]()
    { stream_cpp << "  const char* " << varname << " = \"" << public_path_for({it->first, it->second}) << "\";" << std::endl; });
    if (it != file_map.begin()) stream_js << ',' << std::endl;
    stream_js << "  \"" << alias << "\": \"" << public_path_for({it->first, it->second});
  }
  stream_js << std::endl << '}' << std::endl;
  stream_cpp << '}' << std::endl;
  stream_hpp << '}' << std::endl << "#endif" << std::endl;
  header.open(output_path.data() + std::string("/assets.hpp"));
  source.open(output_path.data() + std::string("/assets.cpp"));
  javascript.open(output_path.data() + std::string("/assets.js"));
  header.set_body(stream_hpp.str().c_str(), stream_hpp.str().length());
  source.set_body(stream_cpp.str().c_str(), stream_cpp.str().length());
  javascript.set_body(stream_js.str().c_str(), stream_js.str().length());
  return true;
}
