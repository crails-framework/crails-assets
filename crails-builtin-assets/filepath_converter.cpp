#include <string>

std::string filepath_to_varname(const std::string& filepath)
{
  std::string result;

  for (size_t i = 0 ; i < filepath.length() ; ++i)
  {
    if (filepath[i] == '/' || filepath[i] == ':' || filepath[i] == '-' || filepath[i] == '.')
      result += '_';
    else
      result += filepath[i];
  }
  return result;
}
