#include "compression.hpp"
#include <sstream>

std::string compress_command(CompressionStrategy strategy, const boost::filesystem::path& source)
{
  std::stringstream stream;

  switch (strategy)
  {
  case Gzip:
    stream << "gzip -kf " << source.string();
    break;
  case Brotli:
    stream << "brotli -kf " << source.string();
    break ;
  default:
    break ;
  }
  return stream.str();
}
