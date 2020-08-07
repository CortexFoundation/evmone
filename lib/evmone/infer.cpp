#include "infer.h"
#include "../cvm-runtime/include/cvm/c_api.h"
#include <string>
#include <fstream>
#include <sstream>

uint8_t decode_model_rlp(ModelMeta* mm, uint8_t* model_raw, uint64_t model_size) {
    if (model_size < 2) {
        return ErrorCodeTypeModelMeta;
    }
    if (!(model_raw[0] == 0x0 && model_raw[1] == 0x1)) {
        return ErrorCodeTypeModelMeta;
    }
    return SUCCESS;
}

uint8_t decode_input_rlp(InputMeta* im, uint8_t* input_raw, uint64_t input_size) {
    if (input_size < 2) {
        return ErrorCodeTypeInputMeta;
    }
    if (!(input_raw[0] == 0x0 && input_raw[1] == 0x1)) {
        return ErrorCodeTypeInputMeta;
    }
    return SUCCESS;
}

ModelMeta check_model(evmone::execution_state& state, evmc::address model_addr, uint8_t& err) {
    auto model_size = state.host.get_code_size(model_addr);
    uint8_t* model_buffer = new uint8_t[model_size];
    auto copy_size = state.host.copy_code(model_addr, 0, model_buffer, model_size);
    auto model_meta = ModelMeta();
    if (copy_size < model_size) {
        err = ErrorCodeTypeModelMeta;
        return model_meta;
    }
    err = decode_model_rlp(&model_meta, model_buffer, model_size);
    return model_meta;
}

InputMeta check_input_meta(evmone::execution_state& state, evmc::address input_addr, uint8_t& err) {
    auto input_size = state.host.get_code_size(input_addr);
    uint8_t* input_buffer = new uint8_t[input_size];
    auto copy_size = state.host.copy_code(input_addr, 0, input_buffer, input_size);
    auto input_meta = InputMeta();
    if (copy_size < input_size) {
        err = ErrorCodeTypeInputMeta;
        return input_meta;
    }
    err = decode_input_rlp(&input_meta, input_buffer, input_size);
    return input_meta;
}

char* get_file(std::string input_hash, std::string sub_path) {
    std::string path = input_hash + sub_path;
    std::ifstream fs(path);
    fs.seekg(0, std::ios::end);    // go to the end
    auto length = fs.tellg();           // report location (this is the length)
    fs.seekg(0, std::ios::beg);    // go back to the beginning
    auto buffer = new char [(unsigned long)length];    // allocate memory for a buffer of appropriate dimension
    fs.read(buffer, length);       // read the whole file into the buffer
    fs.close();                    // close file handle
    return (char *)buffer;
}

Model* new_model(char* model_bytes, char* param_bytes, int device_type, int device_id) {
    auto model = new Model();
    int status = CVMAPILoadModel(model_bytes, int(std::strlen(model_bytes)), param_bytes, int(std::strlen(param_bytes)), &model->model, device_type, device_id);
    if (status != SUCCEED) {
        return nullptr;
    }
    status = CVMAPIGetStorageSize(model->model, &model->size);
    if (status != SUCCEED) {
        return nullptr;
    }
    status = CVMAPIGetGasFromModel(model->model, &model->ops);
    if (status != SUCCEED) {
        return nullptr;
    }
    status = CVMAPIGetInputLength(model->model, &model->input_size);
    if (status != SUCCEED) {
        return nullptr;
    }
    status = CVMAPIGetInputTypeSize(model->model, &model->input_byte);
    if (status != SUCCEED) {
        return nullptr;
    }
    status = CVMAPIGetOutputTypeSize(model->model, &model->output_byte);
    if (status != SUCCEED) {
        return nullptr;
    }
    return model;
}

uint32_t* infer(std::string model_hash, std::string input_hash, uint64_t model_raw_size, uint64_t input_raw_size, uint8_t& err) {
    auto input_bytes = get_file(input_hash, DATA_PATH);
    auto model_bytes = get_file(model_hash, SYMBOL_PATH);
    auto param_bytes = get_file(model_hash, PARAM_PATH);
    int device_type = 0, device_id = 0;
    auto model = new_model(model_bytes, param_bytes, device_type, device_id);
    uint32_t* output;
    auto input_length = strlen(input_bytes);
    auto status = CVMAPIInference(model->model, input_bytes, int(input_length) ,(char*)output);
    if (status != SUCCEED) {
        return nullptr;
    }
    return output;
}
