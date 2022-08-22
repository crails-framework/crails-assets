#pragma once
#include <boost/filesystem.hpp>

enum CompressionStrategy
{
  Gzip,
  Brotli,
  AllCompressions,
  NoCompression
};

std::string compress_command(CompressionStrategy strategy, const boost::filesystem::path& source);
