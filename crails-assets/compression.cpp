#include "compression.hpp"
#include <sstream>

std::string compress_command(CompressionStrategy strategy, const std::filesystem::path& source)
{
  std::stringstream stream;

  switch (strategy)
  {
  case Gzip:
    stream << "gzip -knf " << source.string();
    break;
  case Brotli:
    stream << "brotli -kf " << source.string();
    break ;
  default:
    break ;
  }
  return stream.str();
}
