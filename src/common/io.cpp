#include "common/io.h"

#include "3rd_party/cnpy/cnpy.h"
#include "common/shape.h"
#include "common/types.h"

#include "common/binary.h"
#include "common/io_item.h"
#include "common/regex.h"
#include "common/file_stream.h"

namespace marian {
namespace io {

bool isNpz(const std::string& fileName) {
  return fileName.size() >= 4
         && fileName.substr(fileName.length() - 4) == ".npz";
}

bool isBin(const std::string& fileName) {
  return fileName.size() >= 4
         && fileName.substr(fileName.length() - 4) == ".bin";
}

void getYamlFromNpz(YAML::Node& yaml,
                    const std::string& varName,
                    const std::string& fileName) {
  auto item = cnpy::npz_load(fileName, varName);
  if(item->size() > 0)
    yaml = YAML::Load(item->data());
}

void getYamlFromBin(YAML::Node& yaml,
                    const std::string& varName,
                    const std::string& fileName) {
  auto item = binary::getItem(fileName, varName);
  if(item.size() > 0)
    yaml = YAML::Load(item.data());
}

void getYamlFromModel(YAML::Node& yaml,
                      const std::string& varName,
                      const std::string& fileName) {
  if(io::isNpz(fileName)) {
    io::getYamlFromNpz(yaml, varName, fileName);
  } else if(io::isBin(fileName)) {
    io::getYamlFromBin(yaml, varName, fileName);
  } else {
    ABORT("Unknown model file format for file {}", fileName);
  }
}

void getYamlFromModel(YAML::Node& yaml,
                      const std::string& varName,
                      const void* ptr) {
  auto item = binary::getItem(ptr, varName);
  if(item.size() > 0)
    yaml = YAML::Load(item.data());
}

void addMetaToItems(const std::string& meta,
                    const std::string& varName,
                    std::vector<io::Item>& items) {
  Item item;
  item.name = varName;

  // increase size by 1 to add \0
  item.shape = Shape({(int)meta.size() + 1});

  item.bytes.resize(item.shape.elements());
  std::copy(meta.begin(), meta.end(), item.bytes.begin());
  // set string terminator
  item.bytes.back() = '\0';

  item.type = Type::int8;

  items.push_back(item);
}

void loadItemsFromNpz(const std::string& fileName, std::vector<Item>& items) {
  auto numpy = cnpy::npz_load(fileName);
  for(auto it : numpy) {
    Shape shape;
    if(it.second->shape.size() == 1) {
      shape.resize(2);
      shape.set(0, 1);
      shape.set(1, (size_t)it.second->shape[0]);
    } else {
      shape.resize(it.second->shape.size());
      for(size_t i = 0; i < it.second->shape.size(); ++i)
        shape.set(i, (size_t)it.second->shape[i]);
    }

    Item item;
    item.name = it.first;
    item.shape = shape;
    item.bytes.swap(it.second->bytes);
    items.emplace_back(std::move(item));
  }
}

std::vector<Item> loadItems(const std::string& fileName) {
  std::vector<Item> items;
  if(isNpz(fileName)) {
    loadItemsFromNpz(fileName, items);
  } else if(isBin(fileName)) {
    binary::loadItems(fileName, items);
  } else {
    ABORT("Unknown model file format for file {}", fileName);
  }

  return items;
}

std::vector<Item> loadItems(const void* ptr) {
  std::vector<Item> items;
  binary::loadItems(ptr, items, false);
  return items;
}

std::vector<Item> mmapItems(const void* ptr) {
  std::vector<Item> items;
  binary::loadItems(ptr, items, true);
  return items;
}

// @TODO: make cnpy and our wrapper talk to each other in terms of types
// or implement our own saving routines for npz based on npy, probably better.
void saveItemsNpz(const std::string& fileName, const std::vector<Item>& items) {
  std::vector<cnpy::NpzItem> npzItems;
  for(auto& item : items) {
    std::vector<unsigned int> shape(item.shape.begin(), item.shape.end());
    char type;

    if(item.type == Type::float32)
      type = cnpy::map_type(typeid(float));
    else if(item.type == Type::float64)
      type = cnpy::map_type(typeid(double));
    else if(item.type == Type::int8)
      type = cnpy::map_type(typeid(char));
    else if(item.type == Type::int32)
      type = cnpy::map_type(typeid(int32_t));
    else if (item.type == Type::uint32)
        type = cnpy::map_type(typeid(uint32_t));
    else
      ABORT("Other types not supported yet");

    npzItems.emplace_back(item.name, item.bytes, shape, type, sizeOf(item.type));
  }
  cnpy::npz_save(fileName, npzItems);
}

void saveItems(const std::string& fileName, const std::vector<Item>& items) {
  if(isNpz(fileName)) {
    saveItemsNpz(fileName, items);
  } else if(isBin(fileName)) {
    binary::saveItems(fileName, items);
  } else {
    ABORT("Unknown file format for file {}", fileName);
  }
}

template<typename T>
void matTranspose(const T *src, int src_row, int src_col, T *dest) {
  for (int r = 0; r < src_row; r++) {
    for (int c = 0; c < src_col; c++) {
      dest[c * src_row + r] = *src++;
    }
  }
}

void saveInt16Items(const std::string& fileName, const std::vector<Item>& items) {
  std::vector<Item> int16_items;
  Item int16_item;
  const float int16_max = 32767;

  for (auto& item : items) {
    if (item.name.find("special") == item.name.npos) {
      int16_item.bytes.resize(item.bytes.size() / 2);
      int16_item.type = Type::int16;
      int16_item.shape = item.shape;
      int16_item.name = item.name;
      float scale;
      float min = std::numeric_limits<float>::max();
      float max = std::numeric_limits<float>::min();

      for (int i = 0; i < item.shape.elements(); i++) {
        float e = ((float*)item.bytes.data())[i];

        if (e > max) {
          max = e;
        }
        if (e < min) {
          min = e;
        }
      }

      float max_val = std::max(std::fabs(min), std::fabs(max));
      scale = int16_max / max_val;

      for (int i = 0; i < item.shape.elements(); i++) {
        float e = ((float*)item.bytes.data())[i];
        int16_t d = e * scale;

        ((int16_t*)int16_item.bytes.data())[i] = d;
      }

      bool transpose = false;
      if (item.name[item.name.size() - 2] == 'W') {
        transpose = true;
        std::vector<char> dest;
        dest.resize(int16_item.bytes.size());
        matTranspose((int16_t*)int16_item.bytes.data(), int16_item.shape.dim(0), int16_item.shape.dim(1),
                      (int16_t*)dest.data());

        std::swap(int16_item.shape.dim(0), int16_item.shape.dim(1));
        std::swap(int16_item.bytes, dest);
      }

      LOG(info, "[data] Save parameter {}, shape {}, {}M, max {}, min {}, transpose {}, scale {}", int16_item.name, int16_item.shape.toString(),
          int16_item.bytes.size() / 1024 / 1024, max, min, transpose, scale);

      int16_items.emplace_back(std::move(int16_item));

      Item int16_para;
      int16_para.bytes.resize(4);
      memcpy(&int16_para.bytes[0], &scale, 4);
      int16_para.type = Type::float32;
      int16_para.shape = {1};
      int16_para.name = item.name + "_fscale";
      int16_items.emplace_back(std::move(int16_para));
    } else {
      int16_items.emplace_back(item);
    }
  }

  if(isNpz(fileName)) {
    saveItemsNpz(fileName, items);
  } else if(isBin(fileName)) {
    binary::saveItems(fileName, int16_items);
  } else {
    ABORT("Unknown file format for file {}", fileName);
  }
}

std::vector<std::string> loadVocab(const std::string& vocabPath, size_t maxSize) {
  bool isJson = regex::regex_search(vocabPath, regex::regex("\\.(json|yaml|yml)$"));
  LOG(info,
      "[data] Loading vocabulary from {} file {}",
      isJson ? "JSON/Yaml" : "text",
      vocabPath);
  ABORT_IF(!filesystem::exists(vocabPath),
          "DefaultVocabulary file {} does not exist",
          vocabPath);
 
  std::vector<std::string> vocabs;
  // read from JSON (or Yaml) file
  if(isJson) {
    io::InputFileStream strm(vocabPath);
    YAML::Node vocabNode = YAML::Load(strm);
    for(auto&& pair : vocabNode)
      if(!maxSize || vocabs.size() < maxSize) {
        vocabs.push_back(pair.first.as<std::string>());
      }
  }
  // read from flat text file
  else {
    io::InputFileStream in(vocabPath);
    std::string line;
    while(io::getline(in, line)) {
      ABORT_IF(line.empty(),
              "DefaultVocabulary file {} must not contain empty lines",
              vocabPath);
       if(!maxSize || vocabs.size() < maxSize) {
        vocabs.push_back(line);
      }
    }
    ABORT_IF(in.bad(), "DefaultVocabulary file {} could not be read", vocabPath);
  }

  ABORT_IF(vocabs.empty(), "Empty vocabulary: ", vocabPath);

  return vocabs;
}


void saveTxtVocab(const std::string& vocabPath, std::vector<std::string>& vocab, size_t maxSize) {
  size_t count = 0;
  io::OutputFileStream out(vocabPath);
  for (std::string& word : vocab) {
    // LOG(info, "{}", word);
    out << word << std::endl;
    count++;
  }
  ABORT_IF(count != maxSize, "Vocabulary size error: {} / {} ", count, maxSize);
  LOG(info, "[data] Transforming vocabulary size for input 0 to {}", count);
}

}  // namespace io
}  // namespace marian
