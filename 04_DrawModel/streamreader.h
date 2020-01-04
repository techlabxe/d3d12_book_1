#pragma once
#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/Deserialize.h>

#include <iostream>
#include <fstream>
#if _MSC_VER > 1922 && !defined(_SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING)
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#endif

#include <experimental/filesystem>

class StreamReader : public Microsoft::glTF::IStreamReader
{
public:
  StreamReader(std::experimental::filesystem::path pathBase) : m_pathBase(std::move(pathBase)){ }

  std::shared_ptr<std::istream> GetInputStream(const std::string& filename) const override
  {
    auto streamPath = m_pathBase / std::experimental::filesystem::u8path(filename);
    auto stream = std::make_shared<std::ifstream>(streamPath, std::ios_base::binary);
    if (!stream || !(*stream))
    {
      throw std::runtime_error("Unable to create valid input stream.");
    }
    return stream;
  }

private:
  std::experimental::filesystem::path m_pathBase;
};

