/*
 * @Author: Zhao Shiyu
 * @Date: 2021-01-19 20:48:39
 */
#pragma once

#include <vector>

#include "common/definitions.h"
#include "common/utils.h"
#include "common/io.h"

#include "models/model_task.h"

#include "json/json.hpp"

namespace marian {

class TransformModel : public ModelTask {
private:
  Ptr<Options> options_;

public:
  TransformModel(Ptr<Options> options)
    : options_(New<Options>(options->clone())) { // @TODO: clone should return Ptr<Options> same as "with"?
    // This is currently safe as the translator is either created stand-alone or
    // or config is created anew from Options in the validator

    options_->set("inference", true,
                  "shuffle", "none");
  }

  void run() override {

    auto models = options_->get<std::vector<std::string>>("models");
    std::string npz_model_path = models.front();
    std::vector<std::string> path_item = utils::split(npz_model_path, "/");
    path_item[path_item.size() - 1] = "model.bin";
    std::string bin_model_path = utils::join(path_item, "/");
    LOG(info, "[model] Loading model from {}", npz_model_path);
    auto items = io::loadItems(npz_model_path);
    io::saveInt16Items(bin_model_path, items);


    auto vocabPaths = options_->get<std::vector<std::string>>("vocabs");
    std::vector<int> maxVocabs = options_->get<std::vector<int>>("dim-vocabs");

    path_item[path_item.size() - 1] = "vocab_src.txt";
    std::string src_vocab_path = utils::join(path_item, "/");
    std::vector<std::string> srcVocab = io::loadVocab(vocabPaths[0], maxVocabs[0]);
    io::saveTxtVocab(src_vocab_path, srcVocab, maxVocabs[0]);

    path_item[path_item.size() - 1] = "vocab_trg.txt";
    std::string trg_vocab_path = utils::join(path_item, "/");
    std::vector<std::string> trgVocab = io::loadVocab(vocabPaths[1], maxVocabs[1]);
    io::saveTxtVocab(trg_vocab_path, trgVocab, maxVocabs[1]);

    nlohmann::json config;
    config["pepo-config"] = "pepo_config.json";
    config["model"] = "model.bin";
    config["vocabs"] = {"vocab_src.txt", "vocab_trg.txt"};
    config["dim-vocabs"] = options_->get<std::vector<size_t>>("dim-vocabs");
    config["dim-emb"] = options_->get<int>("dim-emb");
    config["enc-depth"] = options_->get<int>("enc-depth");
    config["dec-depth"] = options_->get<int>("dec-depth");
    config["transformer-heads"] = options_->get<int>("transformer-heads");
    config["transformer-dim-ffn"] = options_->get<int>("transformer-dim-ffn");
    config["transformer-ffn-depth"] = options_->get<int>("transformer-ffn-depth");
    config["transformer-ffn-activation"] = options_->get<std::string>("transformer-ffn-activation");
    config["transformer-dim-aan"] = options_->get<int>("transformer-dim-aan");
    config["transformer-aan-depth"] = options_->get<int>("transformer-aan-depth");
    config["transformer-aan-activation"] = options_->get<std::string>("transformer-aan-activation");
    config["transformer-aan-nogate"] = options_->get<bool>("transformer-aan-nogate");
    config["beam-size"] = options_->get<size_t>("beam-size");
    config["normalize"] = options_->get<float>("normalize");
    config["word-penalty"] = options_->get<float>("word-penalty");
    config["max-length-factor"] = options_->get<float>("max-length-factor");
    config["allow-unk"] = options_->get<bool>("allow-unk");
    config["workspace"] = options_->get<size_t>("workspace");
   
    path_item[path_item.size() - 1] = "model_config.json";
    std::string config_path = utils::join(path_item, "/");
    io::OutputFileStream out(config_path);
    out << config.dump(4) << std::endl;
    LOG(info, "[info] Dump json config success");
  }
};
}  // namespace marian