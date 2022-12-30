#include "file_mapper.hpp"

void FileMapper::collect_files(std::filesystem::path directory)
{
  directory = std::filesystem::canonical(directory);
  collect_files(directory, directory);
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
