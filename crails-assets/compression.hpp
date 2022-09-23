#pragma once
#include <filesystem>

enum CompressionStrategy
{
  Gzip,
  Brotli,
  AllCompressions,
  NoCompression
};

std::string compress_command(CompressionStrategy strategy, const std::filesystem::path& source);
