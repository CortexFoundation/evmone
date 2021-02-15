#ifndef EVMONE_INFER_H
#define EVMONE_INFER_H

#include <string>
#include <vector>
#include "evmc/evmc.h"
#include "instructions.hpp"
#include "analysis.hpp"

const std::string DATA_PATH    = "/data";
const std::string SYMBOL_PATH  = "/data/symbol";
const std::string PARAM_PATH   = "/data/params";

const uint8_t SUCCESS = 1;
const uint8_t ErrorCodeTypeModelMeta = 1;
const uint8_t ErrorCodeTypeInputMeta = 2;
const uint8_t ErrorDecodeModelMeta = 3;
const uint8_t ErrorDecodeInputMeta = 4;
const uint8_t ErrorNotMature = 5;
const uint8_t ErrorExpired = 6;
const uint8_t ErrorInvalidBlockNum = 6;

const uint8_t CVM_VERSION_ONE = 0;
const uint8_t CVM_VERSION_TWO = 1;

class ModelMeta
{
public:
    std::string comment;
    evmc_address hash;
    uint64_t raw_size;
    std::vector<uint64_t> input_shape;
    std::vector<uint64_t> output_shape;
    uint64_t gas;
    evmc_address author_address;
    uint64_t block_num;
};

class InputMeta
{
public:
    std::string comment;
    evmc_address hash;
    uint64_t raw_size;
    std::vector<uint64_t> shape;

    uint64_t block_num;
};

class LibCVM {
    std::string path;
    void* lib;
};

class Model
{
public:
    void * model;
    LibCVM* lib;
    uint64_t ops;
    uint64_t size;

    uint64_t input_size;
    uint64_t input_byte;

    uint64_t output_byte;
};

ModelMeta check_model(evmone::execution_state& state, evmc::address model_addr, uint8_t& err);
InputMeta check_input_meta(evmone::execution_state& state, evmc::address input_addr, uint8_t& err);
uint8_t* infer(const std::string& model_hash, const std::string& input_hash, uint64_t model_raw_size, uint64_t input_raw_size, uint8_t& err);
Model* new_model(char* model_bytes, char* param_bytes, int device_type, int device_id);
#endif  // EVMONE_INFER_H
