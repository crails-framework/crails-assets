#include "file_mapper.hpp"
#include <iostream>

void FileMapper::collect_files(std::filesystem::path directory)
{
  std::error_code error_code;

  directory = std::filesystem::canonical(directory, error_code);
  if (!error_code)
    collect_files(directory, directory);
  else
    std::cerr << "/!\\ no such directory '" << directory.string() << '\'' << std::endl;
}

void FileMapper::collect_files(std::filesystem::path root, std::filesystem::path path)
{
  if (std::filesystem::is_directory(path))
  {
    std::filesystem::recursive_directory_iterator dir(path);

    for (auto& entry : dir)
      collect_files(root, entry.path());
  }
  else if (std::filesystem::is_regular_file(path))
  {
    emplace(path.string(), path.string().substr(root.string().length() + 1));
  }
}
