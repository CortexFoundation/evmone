// evmone: Fast Ethereum Virtual Machine implementation
// Copyright 2019-2020 The evmone Authors.
// SPDX-License-Identifier: Apache-2.0

#include "instructions.hpp"
#include "analysis.hpp"
#include <ethash/keccak.hpp>

namespace evmone
{
namespace
{
template <void InstrFn(evm_stack&)>
const instruction* op(const instruction* instr, execution_state& state) noexcept
{
    InstrFn(state.stack);
    return ++instr;
}

template <void InstrFn(execution_state&)>
const instruction* op(const instruction* instr, execution_state& state) noexcept
{
    InstrFn(state);
    return ++instr;
}

template <evmc_status_code InstrFn(execution_state&)>
const instruction* op(const instruction* instr, execution_state& state) noexcept
{
    const auto status_code = InstrFn(state);
    if (status_code != EVMC_SUCCESS)
        return state.exit(status_code);
    return ++instr;
}

const instruction* op_stop(const instruction*, execution_state& state) noexcept
{
    return state.exit(EVMC_SUCCESS);
}

const instruction* op_sha3(const instruction* instr, execution_state& state) noexcept
{
    const auto index = state.stack.pop();
    auto& size = state.stack.top();

    if (!check_memory(state, index, size))
        return nullptr;

    const auto i = static_cast<size_t>(index);
    const auto s = static_cast<size_t>(size);
    const auto w = num_words(s);
    const auto cost = w * 6;
    if ((state.gas_left -= cost) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    auto data = s != 0 ? &state.memory[i] : nullptr;
    size = intx::be::load<uint256>(ethash::keccak256(data, s));
    return ++instr;
}

const instruction* op_address(const instruction* instr, execution_state& state) noexcept
{
    // TODO: Might be generalized using pointers to class member.
    state.stack.push(intx::be::load<uint256>(state.msg->destination));
    return ++instr;
}

const instruction* op_balance(const instruction* instr, execution_state& state) noexcept
{
    auto& x = state.stack.top();
    x = intx::be::load<uint256>(state.host.get_balance(intx::be::trunc<evmc::address>(x)));
    return ++instr;
}

const instruction* op_chainid(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(intx::be::load<uint256>(state.host.get_tx_context().chain_id));
    return ++instr;
}

const instruction* op_selfbalance(const instruction* instr, execution_state& state) noexcept
{
    // TODO: introduce selfbalance in EVMC?
    state.stack.push(intx::be::load<uint256>(state.host.get_balance(state.msg->destination)));
    return ++instr;
}

const instruction* op_origin(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(intx::be::load<uint256>(state.host.get_tx_context().tx_origin));
    return ++instr;
}

const instruction* op_caller(const instruction* instr, execution_state& state) noexcept
{
    // TODO: Might be generalized using pointers to class member.
    state.stack.push(intx::be::load<uint256>(state.msg->sender));
    return ++instr;
}

const instruction* op_callvalue(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(intx::be::load<uint256>(state.msg->value));
    return ++instr;
}

const instruction* op_calldataload(const instruction* instr, execution_state& state) noexcept
{
    auto& index = state.stack.top();

    if (state.msg->input_size < index)
        index = 0;
    else
    {
        const auto begin = static_cast<size_t>(index);
        const auto end = std::min(begin + 32, state.msg->input_size);

        uint8_t data[32] = {};
        for (size_t i = 0; i < (end - begin); ++i)
            data[i] = state.msg->input_data[begin + i];

        index = intx::be::load<uint256>(data);
    }
    return ++instr;
}

const instruction* op_calldatasize(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(state.msg->input_size);
    return ++instr;
}

const instruction* op_calldatacopy(const instruction* instr, execution_state& state) noexcept
{
    const auto mem_index = state.stack.pop();
    const auto input_index = state.stack.pop();
    const auto size = state.stack.pop();

    if (!check_memory(state, mem_index, size))
        return nullptr;

    auto dst = static_cast<size_t>(mem_index);
    auto src = state.msg->input_size < input_index ? state.msg->input_size :
                                                     static_cast<size_t>(input_index);
    auto s = static_cast<size_t>(size);
    auto copy_size = std::min(s, state.msg->input_size - src);

    const auto copy_cost = num_words(s) * 3;
    if ((state.gas_left -= copy_cost) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    if (copy_size > 0)
        std::memcpy(&state.memory[dst], &state.msg->input_data[src], copy_size);

    if (s - copy_size > 0)
        std::memset(&state.memory[dst + copy_size], 0, s - copy_size);
    return ++instr;
}

const instruction* op_codesize(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(state.code_size);
    return ++instr;
}

const instruction* op_codecopy(const instruction* instr, execution_state& state) noexcept
{
    // TODO: Similar to op_calldatacopy().

    const auto mem_index = state.stack.pop();
    const auto input_index = state.stack.pop();
    const auto size = state.stack.pop();

    if (!check_memory(state, mem_index, size))
        return nullptr;

    auto dst = static_cast<size_t>(mem_index);
    auto src = state.code_size < input_index ? state.code_size : static_cast<size_t>(input_index);
    auto s = static_cast<size_t>(size);
    auto copy_size = std::min(s, state.code_size - src);

    const auto copy_cost = num_words(s) * 3;
    if ((state.gas_left -= copy_cost) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    // TODO: Add unit tests for each combination of conditions.
    if (copy_size > 0)
        std::memcpy(&state.memory[dst], &state.code[src], copy_size);

    if (s - copy_size > 0)
        std::memset(&state.memory[dst + copy_size], 0, s - copy_size);
    return ++instr;
}

const instruction* op_sload(const instruction* instr, execution_state& state) noexcept
{
    auto& x = state.stack.top();
    x = intx::be::load<uint256>(
        state.host.get_storage(state.msg->destination, intx::be::store<evmc::bytes32>(x)));
    return ++instr;
}

const instruction* op_sstore(const instruction* instr, execution_state& state) noexcept
{
    // TODO: Implement static mode violation in analysis.
    if (state.msg->flags & EVMC_STATIC)
        return state.exit(EVMC_STATIC_MODE_VIOLATION);

    if (state.rev >= EVMC_ISTANBUL)
    {
        const auto correction = state.current_block_cost - instr->arg.number;
        const auto gas_left = state.gas_left + correction;
        if (gas_left <= 2300)
            return state.exit(EVMC_OUT_OF_GAS);
    }

    const auto key = intx::be::store<evmc::bytes32>(state.stack.pop());
    const auto value = intx::be::store<evmc::bytes32>(state.stack.pop());
    auto status = state.host.set_storage(state.msg->destination, key, value);
    int cost = 0;
    switch (status)
    {
    case EVMC_STORAGE_UNCHANGED:
        if (state.rev >= EVMC_ISTANBUL)
            cost = 800;
        else if (state.rev == EVMC_CONSTANTINOPLE)
            cost = 200;
        else
            cost = 5000;
        break;
    case EVMC_STORAGE_MODIFIED:
        cost = 5000;
        break;
    case EVMC_STORAGE_MODIFIED_AGAIN:
        if (state.rev >= EVMC_ISTANBUL)
            cost = 800;
        else if (state.rev == EVMC_CONSTANTINOPLE)
            cost = 200;
        else
            cost = 5000;
        break;
    case EVMC_STORAGE_ADDED:
        cost = 20000;
        break;
    case EVMC_STORAGE_DELETED:
        cost = 5000;
        break;
    }
    if ((state.gas_left -= cost) < 0)
        return state.exit(EVMC_OUT_OF_GAS);
    return ++instr;
}

const instruction* op_jump(const instruction*, execution_state& state) noexcept
{
    const auto dst = state.stack.pop();
    auto pc = -1;
    if (std::numeric_limits<int>::max() < dst ||
        (pc = find_jumpdest(*state.analysis, static_cast<int>(dst))) < 0)
        return state.exit(EVMC_BAD_JUMP_DESTINATION);

    return &state.analysis->instrs[static_cast<size_t>(pc)];
}

const instruction* op_jumpi(const instruction* instr, execution_state& state) noexcept
{
    if (state.stack[1] != 0)
        instr = op_jump(instr, state);
    else
    {
        state.stack.pop();
        ++instr;
    }

    // OPT: The pc must be the BEGINBLOCK (even in fallback case),
    //      so we can execute it straight away.

    state.stack.pop();
    return instr;
}

const instruction* op_pc(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(instr->arg.number);
    return ++instr;
}

const instruction* op_gas(const instruction* instr, execution_state& state) noexcept
{
    const auto correction = state.current_block_cost - instr->arg.number;
    const auto gas = static_cast<uint64_t>(state.gas_left + correction);
    state.stack.push(gas);
    return ++instr;
}

const instruction* op_gasprice(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(intx::be::load<uint256>(state.host.get_tx_context().tx_gas_price));
    return ++instr;
}

const instruction* op_extcodesize(const instruction* instr, execution_state& state) noexcept
{
    auto& x = state.stack.top();
    x = state.host.get_code_size(intx::be::trunc<evmc::address>(x));
    return ++instr;
}

const instruction* op_extcodecopy(const instruction* instr, execution_state& state) noexcept
{
    const auto addr = intx::be::trunc<evmc::address>(state.stack.pop());
    const auto mem_index = state.stack.pop();
    const auto input_index = state.stack.pop();
    const auto size = state.stack.pop();

    if (!check_memory(state, mem_index, size))
        return nullptr;

    auto dst = static_cast<size_t>(mem_index);
    auto src = max_buffer_size < input_index ? max_buffer_size : static_cast<size_t>(input_index);
    auto s = static_cast<size_t>(size);

    const auto copy_cost = num_words(s) * 3;
    if ((state.gas_left -= copy_cost) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    auto data = s != 0 ? &state.memory[dst] : nullptr;
    auto num_bytes_copied = state.host.copy_code(addr, src, data, s);
    if (s - num_bytes_copied > 0)
        std::memset(&state.memory[dst + num_bytes_copied], 0, s - num_bytes_copied);
    return ++instr;
}

const instruction* op_returndatasize(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(state.return_data.size());
    return ++instr;
}

const instruction* op_returndatacopy(const instruction* instr, execution_state& state) noexcept
{
    const auto mem_index = state.stack.pop();
    const auto input_index = state.stack.pop();
    const auto size = state.stack.pop();

    if (!check_memory(state, mem_index, size))
        return nullptr;

    auto dst = static_cast<size_t>(mem_index);
    auto s = static_cast<size_t>(size);

    if (state.return_data.size() < input_index)
        return state.exit(EVMC_INVALID_MEMORY_ACCESS);
    auto src = static_cast<size_t>(input_index);

    if (src + s > state.return_data.size())
        return state.exit(EVMC_INVALID_MEMORY_ACCESS);

    const auto copy_cost = num_words(s) * 3;
    if ((state.gas_left -= copy_cost) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    if (s > 0)
        std::memcpy(&state.memory[dst], &state.return_data[src], s);
    return ++instr;
}

const instruction* op_extcodehash(const instruction* instr, execution_state& state) noexcept
{
    auto& x = state.stack.top();
    x = intx::be::load<uint256>(state.host.get_code_hash(intx::be::trunc<evmc::address>(x)));
    return ++instr;
}

const instruction* op_blockhash(const instruction* instr, execution_state& state) noexcept
{
    auto& number = state.stack.top();

    const auto upper_bound = state.host.get_tx_context().block_number;
    const auto lower_bound = std::max(upper_bound - 256, decltype(upper_bound){0});
    const auto n = static_cast<int64_t>(number);
    const auto header =
        (number < upper_bound && n >= lower_bound) ? state.host.get_block_hash(n) : evmc::bytes32{};
    number = intx::be::load<uint256>(header);
    return ++instr;
}

const instruction* op_coinbase(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(intx::be::load<uint256>(state.host.get_tx_context().block_coinbase));
    return ++instr;
}

const instruction* op_timestamp(const instruction* instr, execution_state& state) noexcept
{
    // TODO: Add tests for negative timestamp?
    const auto timestamp = static_cast<uint64_t>(state.host.get_tx_context().block_timestamp);
    state.stack.push(timestamp);
    return ++instr;
}

const instruction* op_number(const instruction* instr, execution_state& state) noexcept
{
    // TODO: Add tests for negative block number?
    const auto block_number = static_cast<uint64_t>(state.host.get_tx_context().block_number);
    state.stack.push(block_number);
    return ++instr;
}

const instruction* op_difficulty(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(intx::be::load<uint256>(state.host.get_tx_context().block_difficulty));
    return ++instr;
}

const instruction* op_gaslimit(const instruction* instr, execution_state& state) noexcept
{
    const auto block_gas_limit = static_cast<uint64_t>(state.host.get_tx_context().block_gas_limit);
    state.stack.push(block_gas_limit);
    return ++instr;
}

const instruction* op_push_small(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(instr->arg.small_push_value);
    return ++instr;
}

const instruction* op_push_full(const instruction* instr, execution_state& state) noexcept
{
    state.stack.push(*instr->arg.push_value);
    return ++instr;
}

const instruction* op_pop(const instruction* instr, execution_state& state) noexcept
{
    state.stack.pop();
    return ++instr;
}

template <evmc_opcode DupOp>
const instruction* op_dup(const instruction* instr, execution_state& state) noexcept
{
    constexpr auto index = DupOp - OP_DUP1;
    state.stack.push(state.stack[index]);
    return ++instr;
}

template <evmc_opcode SwapOp>
const instruction* op_swap(const instruction* instr, execution_state& state) noexcept
{
    constexpr auto index = SwapOp - OP_SWAP1 + 1;
    std::swap(state.stack.top(), state.stack[index]);
    return ++instr;
}

const instruction* op_log(
    const instruction* instr, execution_state& state, size_t num_topics) noexcept
{
    if (state.msg->flags & EVMC_STATIC)
        return state.exit(EVMC_STATIC_MODE_VIOLATION);

    const auto offset = state.stack.pop();
    const auto size = state.stack.pop();

    if (!check_memory(state, offset, size))
        return nullptr;

    const auto o = static_cast<size_t>(offset);
    const auto s = static_cast<size_t>(size);

    const auto cost = int64_t(s) * 8;
    if ((state.gas_left -= cost) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    auto topics = std::array<evmc::bytes32, 4>{};
    for (size_t i = 0; i < num_topics; ++i)
        topics[i] = intx::be::store<evmc::bytes32>(state.stack.pop());

    const auto data = s != 0 ? &state.memory[o] : nullptr;
    state.host.emit_log(state.msg->destination, data, s, topics.data(), num_topics);
    return ++instr;
}

template <evmc_opcode LogOp>
const instruction* op_log(const instruction* instr, execution_state& state) noexcept
{
    constexpr auto num_topics = LogOp - OP_LOG0;
    return op_log(instr, state, num_topics);
}

const instruction* op_invalid(const instruction*, execution_state& state) noexcept
{
    return state.exit(EVMC_INVALID_INSTRUCTION);
}

template <evmc_status_code status_code>
const instruction* op_return(const instruction*, execution_state& state) noexcept
{
    const auto offset = state.stack[0];
    const auto size = state.stack[1];

    if (!check_memory(state, offset, size))
        return nullptr;

    state.output_size = static_cast<size_t>(size);
    if (state.output_size != 0)
        state.output_offset = static_cast<size_t>(offset);
    return state.exit(status_code);
}

template <evmc_call_kind kind>
const instruction* op_call(const instruction* instr, execution_state& state) noexcept
{
    const auto arg = instr->arg;
    auto gas = state.stack[0];
    const auto dst = intx::be::trunc<evmc::address>(state.stack[1]);
    auto value = state.stack[2];
    auto input_offset = state.stack[3];
    auto input_size = state.stack[4];
    auto output_offset = state.stack[5];
    auto output_size = state.stack[6];

    state.stack.pop();
    state.stack.pop();
    state.stack.pop();
    state.stack.pop();
    state.stack.pop();
    state.stack.pop();
    state.stack[0] = 0;

    if (!check_memory(state, input_offset, input_size))
        return nullptr;

    if (!check_memory(state, output_offset, output_size))
        return nullptr;


    auto msg = evmc_message{};
    msg.kind = kind;
    msg.flags = state.msg->flags;
    msg.value = intx::be::store<evmc::uint256be>(value);

    auto correction = state.current_block_cost - arg.number;
    auto gas_left = state.gas_left + correction;

    auto cost = 0;
    auto has_value = value != 0;

    if (has_value)
        cost += 9000;

    if constexpr (kind == EVMC_CALL)
    {
        if (has_value && state.msg->flags & EVMC_STATIC)
            return state.exit(EVMC_STATIC_MODE_VIOLATION);

        if (has_value || state.rev < EVMC_SPURIOUS_DRAGON)
        {
            if (!state.host.account_exists(dst))
                cost += 25000;
        }
    }

    if ((gas_left -= cost) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    msg.gas = std::numeric_limits<int64_t>::max();
    if (gas < msg.gas)
        msg.gas = static_cast<int64_t>(gas);

    if (state.rev >= EVMC_TANGERINE_WHISTLE)
        msg.gas = std::min(msg.gas, gas_left - gas_left / 64);
    else if (msg.gas > gas_left)
        return state.exit(EVMC_OUT_OF_GAS);

    state.return_data.clear();

    state.gas_left -= cost;
    if (state.msg->depth >= 1024)
    {
        if (has_value)
            state.gas_left += 2300;  // Return unused stipend.
        if (state.gas_left < 0)
            return state.exit(EVMC_OUT_OF_GAS);
        return ++instr;
    }

    msg.destination = dst;
    msg.sender = state.msg->destination;
    msg.value = intx::be::store<evmc::uint256be>(value);

    if (size_t(input_size) > 0)
    {
        msg.input_data = &state.memory[size_t(input_offset)];
        msg.input_size = size_t(input_size);
    }

    msg.depth = state.msg->depth + 1;

    if (has_value)
    {
        const auto balance =
            intx::be::load<uint256>(state.host.get_balance(state.msg->destination));
        if (balance < value)
        {
            state.gas_left += 2300;  // Return unused stipend.
            if (state.gas_left < 0)
                return state.exit(EVMC_OUT_OF_GAS);
            return ++instr;
        }

        msg.gas += 2300;  // Add stipend.
    }

    auto result = state.host.call(msg);
    state.return_data.assign(result.output_data, result.output_size);


    state.stack[0] = result.status_code == EVMC_SUCCESS;

    if (auto copy_size = std::min(size_t(output_size), result.output_size); copy_size > 0)
        std::memcpy(&state.memory[size_t(output_offset)], result.output_data, copy_size);

    auto gas_used = msg.gas - result.gas_left;

    if (has_value)
        gas_used -= 2300;

    if ((state.gas_left -= gas_used) < 0)
        return state.exit(EVMC_OUT_OF_GAS);
    return ++instr;
}

const instruction* op_delegatecall(const instruction* instr, execution_state& state) noexcept
{
    const auto arg = instr->arg;
    auto gas = state.stack[0];
    const auto dst = intx::be::trunc<evmc::address>(state.stack[1]);
    auto input_offset = state.stack[2];
    auto input_size = state.stack[3];
    auto output_offset = state.stack[4];
    auto output_size = state.stack[5];

    state.stack.pop();
    state.stack.pop();
    state.stack.pop();
    state.stack.pop();
    state.stack.pop();
    state.stack[0] = 0;

    if (!check_memory(state, input_offset, input_size))
        return nullptr;

    if (!check_memory(state, output_offset, output_size))
        return nullptr;

    auto msg = evmc_message{};
    msg.kind = EVMC_DELEGATECALL;

    auto correction = state.current_block_cost - arg.number;
    auto gas_left = state.gas_left + correction;

    // TEST: Gas saturation for big gas values.
    msg.gas = std::numeric_limits<int64_t>::max();
    if (gas < msg.gas)
        msg.gas = static_cast<int64_t>(gas);

    if (state.rev >= EVMC_TANGERINE_WHISTLE)
        msg.gas = std::min(msg.gas, gas_left - gas_left / 64);
    else if (msg.gas > gas_left)  // TEST: gas_left vs state.gas_left.
        return state.exit(EVMC_OUT_OF_GAS);

    if (state.msg->depth >= 1024)
        return ++instr;

    msg.depth = state.msg->depth + 1;
    msg.flags = state.msg->flags;
    msg.destination = dst;
    msg.sender = state.msg->sender;
    msg.value = state.msg->value;

    if (size_t(input_size) > 0)
    {
        msg.input_data = &state.memory[size_t(input_offset)];
        msg.input_size = size_t(input_size);
    }

    auto result = state.host.call(msg);
    state.return_data.assign(result.output_data, result.output_size);

    state.stack[0] = result.status_code == EVMC_SUCCESS;

    if (const auto copy_size = std::min(size_t(output_size), result.output_size); copy_size > 0)
        std::memcpy(&state.memory[size_t(output_offset)], result.output_data, copy_size);

    auto gas_used = msg.gas - result.gas_left;

    if ((state.gas_left -= gas_used) < 0)
        return state.exit(EVMC_OUT_OF_GAS);
    return ++instr;
}

const instruction* op_staticcall(const instruction* instr, execution_state& state) noexcept
{
    const auto arg = instr->arg;
    auto gas = state.stack[0];
    const auto dst = intx::be::trunc<evmc::address>(state.stack[1]);
    auto input_offset = state.stack[2];
    auto input_size = state.stack[3];
    auto output_offset = state.stack[4];
    auto output_size = state.stack[5];

    state.stack.pop();
    state.stack.pop();
    state.stack.pop();
    state.stack.pop();
    state.stack.pop();
    state.stack[0] = 0;

    if (!check_memory(state, input_offset, input_size))
        return nullptr;

    if (!check_memory(state, output_offset, output_size))
        return nullptr;

    if (state.msg->depth >= 1024)
        return ++instr;

    auto msg = evmc_message{};
    msg.kind = EVMC_CALL;
    msg.flags |= EVMC_STATIC;

    msg.depth = state.msg->depth + 1;

    auto correction = state.current_block_cost - arg.number;
    auto gas_left = state.gas_left + correction;

    msg.gas = std::numeric_limits<int64_t>::max();
    if (gas < msg.gas)
        msg.gas = static_cast<int64_t>(gas);

    msg.gas = std::min(msg.gas, gas_left - gas_left / 64);

    msg.destination = dst;
    msg.sender = state.msg->destination;

    if (size_t(input_size) > 0)
    {
        msg.input_data = &state.memory[size_t(input_offset)];
        msg.input_size = size_t(input_size);
    }

    auto result = state.host.call(msg);
    state.return_data.assign(result.output_data, result.output_size);
    state.stack[0] = result.status_code == EVMC_SUCCESS;

    if (auto copy_size = std::min(size_t(output_size), result.output_size); copy_size > 0)
        std::memcpy(&state.memory[size_t(output_offset)], result.output_data, copy_size);

    auto gas_used = msg.gas - result.gas_left;

    if ((state.gas_left -= gas_used) < 0)
        return state.exit(EVMC_OUT_OF_GAS);
    return ++instr;
}

const instruction* op_create(const instruction* instr, execution_state& state) noexcept
{
    if (state.msg->flags & EVMC_STATIC)
        return state.exit(EVMC_STATIC_MODE_VIOLATION);

    const auto arg = instr->arg;
    auto endowment = state.stack[0];
    auto init_code_offset = state.stack[1];
    auto init_code_size = state.stack[2];

    state.stack.pop();
    state.stack.pop();
    state.stack[0] = 0;

    if (!check_memory(state, init_code_offset, init_code_size))
        return nullptr;

    state.return_data.clear();

    if (state.msg->depth >= 1024)
        return ++instr;

    if (endowment != 0)
    {
        const auto balance =
            intx::be::load<uint256>(state.host.get_balance(state.msg->destination));
        if (balance < endowment)
            return ++instr;
    }

    auto msg = evmc_message{};

    auto correction = state.current_block_cost - arg.number;
    msg.gas = state.gas_left + correction;
    if (state.rev >= EVMC_TANGERINE_WHISTLE)
        msg.gas = msg.gas - msg.gas / 64;

    msg.kind = EVMC_CREATE;

    if (size_t(init_code_size) > 0)
    {
        msg.input_data = &state.memory[size_t(init_code_offset)];
        msg.input_size = size_t(init_code_size);
    }

    msg.sender = state.msg->destination;
    msg.depth = state.msg->depth + 1;
    msg.value = intx::be::store<evmc::uint256be>(endowment);

    auto result = state.host.call(msg);
    state.return_data.assign(result.output_data, result.output_size);
    if (result.status_code == EVMC_SUCCESS)
        state.stack[0] = intx::be::load<uint256>(result.create_address);

    if ((state.gas_left -= msg.gas - result.gas_left) < 0)
        return state.exit(EVMC_OUT_OF_GAS);
    return ++instr;
}

const instruction* op_create2(const instruction* instr, execution_state& state) noexcept
{
    if (state.msg->flags & EVMC_STATIC)
        return state.exit(EVMC_STATIC_MODE_VIOLATION);

    const auto arg = instr->arg;
    auto endowment = state.stack[0];
    auto init_code_offset = state.stack[1];
    auto init_code_size = state.stack[2];
    auto salt = state.stack[3];

    state.stack.pop();
    state.stack.pop();
    state.stack.pop();
    state.stack[0] = 0;

    if (!check_memory(state, init_code_offset, init_code_size))
        return nullptr;

    auto salt_cost = num_words(static_cast<size_t>(init_code_size)) * 6;
    state.gas_left -= salt_cost;
    if (state.gas_left < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    state.return_data.clear();

    if (state.msg->depth >= 1024)
        return ++instr;

    if (endowment != 0)
    {
        const auto balance =
            intx::be::load<uint256>(state.host.get_balance(state.msg->destination));
        if (balance < endowment)
            return ++instr;
    }

    auto msg = evmc_message{};

    auto correction = state.current_block_cost - arg.number;
    auto gas = state.gas_left + correction;
    msg.gas = gas - gas / 64;

    msg.kind = EVMC_CREATE2;
    if (size_t(init_code_size) > 0)
    {
        msg.input_data = &state.memory[size_t(init_code_offset)];
        msg.input_size = size_t(init_code_size);
    }
    msg.sender = state.msg->destination;
    msg.depth = state.msg->depth + 1;
    msg.create2_salt = intx::be::store<evmc::bytes32>(salt);
    msg.value = intx::be::store<evmc::uint256be>(endowment);

    auto result = state.host.call(msg);
    state.return_data.assign(result.output_data, result.output_size);
    if (result.status_code == EVMC_SUCCESS)
        state.stack[0] = intx::be::load<uint256>(result.create_address);

    if ((state.gas_left -= msg.gas - result.gas_left) < 0)
        return state.exit(EVMC_OUT_OF_GAS);
    return ++instr;
}

const instruction* op_undefined(const instruction*, execution_state& state) noexcept
{
    return state.exit(EVMC_UNDEFINED_INSTRUCTION);
}

const instruction* op_selfdestruct(const instruction*, execution_state& state) noexcept
{
    if (state.msg->flags & EVMC_STATIC)
        return state.exit(EVMC_STATIC_MODE_VIOLATION);

    const auto addr = intx::be::trunc<evmc::address>(state.stack[0]);

    if (state.rev >= EVMC_TANGERINE_WHISTLE)
    {
        if (state.rev == EVMC_TANGERINE_WHISTLE || state.host.get_balance(state.msg->destination))
        {
            // After TANGERINE_WHISTLE apply additional cost of
            // sending value to a non-existing account.
            if (!state.host.account_exists(addr))
            {
                if ((state.gas_left -= 25000) < 0)
                    return state.exit(EVMC_OUT_OF_GAS);
            }
        }
    }

    state.host.selfdestruct(state.msg->destination, addr);
    return state.exit(EVMC_SUCCESS);
}

const instruction* opx_beginblock(const instruction* instr, execution_state& state) noexcept
{
    auto& block = instr->arg.block;

    if ((state.gas_left -= block.gas_cost) < 0)
        return state.exit(EVMC_OUT_OF_GAS);

    if (static_cast<int>(state.stack.size()) < block.stack_req)
        return state.exit(EVMC_STACK_UNDERFLOW);

    if (static_cast<int>(state.stack.size()) + block.stack_max_growth > evm_stack::limit)
        return state.exit(EVMC_STACK_OVERFLOW);

    state.current_block_cost = block.gas_cost;
    return ++instr;
}

constexpr op_table create_op_table_frontier() noexcept
{
    auto table = op_table{};

    // First, mark all opcodes as undefined.
    for (auto& t : table)
        t = {op_undefined, 0, 0, 0};

    table[OP_STOP] = {op_stop, 0, 0, 0};
    table[OP_ADD] = {op<add>, 3, 2, -1};
    table[OP_MUL] = {op<mul>, 5, 2, -1};
    table[OP_SUB] = {op<sub>, 3, 2, -1};
    table[OP_DIV] = {op<div>, 5, 2, -1};
    table[OP_SDIV] = {op<sdiv>, 5, 2, -1};
    table[OP_MOD] = {op<mod>, 5, 2, -1};
    table[OP_SMOD] = {op<smod>, 5, 2, -1};
    table[OP_ADDMOD] = {op<addmod>, 8, 3, -2};
    table[OP_MULMOD] = {op<mulmod>, 8, 3, -2};
    table[OP_EXP] = {op<exp>, 10, 2, -1};
    table[OP_SIGNEXTEND] = {op<signextend>, 5, 2, -1};
    table[OP_LT] = {op<lt>, 3, 2, -1};
    table[OP_GT] = {op<gt>, 3, 2, -1};
    table[OP_SLT] = {op<slt>, 3, 2, -1};
    table[OP_SGT] = {op<sgt>, 3, 2, -1};
    table[OP_EQ] = {op<eq>, 3, 2, -1};
    table[OP_ISZERO] = {op<iszero>, 3, 1, 0};
    table[OP_AND] = {op<and_>, 3, 2, -1};
    table[OP_OR] = {op<or_>, 3, 2, -1};
    table[OP_XOR] = {op<xor_>, 3, 2, -1};
    table[OP_NOT] = {op<not_>, 3, 1, 0};
    table[OP_BYTE] = {op<byte>, 3, 2, -1};
    table[OP_SHA3] = {op_sha3, 30, 2, -1};
    table[OP_ADDRESS] = {op_address, 2, 0, 1};
    table[OP_BALANCE] = {op_balance, 20, 1, 0};
    table[OP_ORIGIN] = {op_origin, 2, 0, 1};
    table[OP_CALLER] = {op_caller, 2, 0, 1};
    table[OP_CALLVALUE] = {op_callvalue, 2, 0, 1};
    table[OP_CALLDATALOAD] = {op_calldataload, 3, 1, 0};
    table[OP_CALLDATASIZE] = {op_calldatasize, 2, 0, 1};
    table[OP_CALLDATACOPY] = {op_calldatacopy, 3, 3, -3};
    table[OP_CODESIZE] = {op_codesize, 2, 0, 1};
    table[OP_CODECOPY] = {op_codecopy, 3, 3, -3};
    table[OP_GASPRICE] = {op_gasprice, 2, 0, 1};
    table[OP_EXTCODESIZE] = {op_extcodesize, 20, 1, 0};
    table[OP_EXTCODECOPY] = {op_extcodecopy, 20, 4, -4};
    table[OP_BLOCKHASH] = {op_blockhash, 20, 1, 0};
    table[OP_COINBASE] = {op_coinbase, 2, 0, 1};
    table[OP_TIMESTAMP] = {op_timestamp, 2, 0, 1};
    table[OP_NUMBER] = {op_number, 2, 0, 1};
    table[OP_DIFFICULTY] = {op_difficulty, 2, 0, 1};
    table[OP_GASLIMIT] = {op_gaslimit, 2, 0, 1};
    table[OP_POP] = {op_pop, 2, 1, -1};
    table[OP_MLOAD] = {op<mload>, 3, 1, 0};
    table[OP_MSTORE] = {op<mstore>, 3, 2, -2};
    table[OP_MSTORE8] = {op<mstore8>, 3, 2, -2};
    table[OP_SLOAD] = {op_sload, 50, 1, 0};
    table[OP_SSTORE] = {op_sstore, 0, 2, -2};
    table[OP_JUMP] = {op_jump, 8, 1, -1};
    table[OP_JUMPI] = {op_jumpi, 10, 2, -2};
    table[OP_PC] = {op_pc, 2, 0, 1};
    table[OP_MSIZE] = {op<msize>, 2, 0, 1};
    table[OP_GAS] = {op_gas, 2, 0, 1};
    table[OPX_BEGINBLOCK] = {opx_beginblock, 1, 0, 0};  // Replaces JUMPDEST.

    for (auto op = size_t{OP_PUSH1}; op <= OP_PUSH8; ++op)
        table[op] = {op_push_small, 3, 0, 1};
    for (auto op = size_t{OP_PUSH9}; op <= OP_PUSH32; ++op)
        table[op] = {op_push_full, 3, 0, 1};

    table[OP_DUP1] = {op_dup<OP_DUP1>, 3, 1, 1};
    table[OP_DUP2] = {op_dup<OP_DUP2>, 3, 2, 1};
    table[OP_DUP3] = {op_dup<OP_DUP3>, 3, 3, 1};
    table[OP_DUP4] = {op_dup<OP_DUP4>, 3, 4, 1};
    table[OP_DUP5] = {op_dup<OP_DUP5>, 3, 5, 1};
    table[OP_DUP6] = {op_dup<OP_DUP6>, 3, 6, 1};
    table[OP_DUP7] = {op_dup<OP_DUP7>, 3, 7, 1};
    table[OP_DUP8] = {op_dup<OP_DUP8>, 3, 8, 1};
    table[OP_DUP9] = {op_dup<OP_DUP9>, 3, 9, 1};
    table[OP_DUP10] = {op_dup<OP_DUP10>, 3, 10, 1};
    table[OP_DUP11] = {op_dup<OP_DUP11>, 3, 11, 1};
    table[OP_DUP12] = {op_dup<OP_DUP12>, 3, 12, 1};
    table[OP_DUP13] = {op_dup<OP_DUP13>, 3, 13, 1};
    table[OP_DUP14] = {op_dup<OP_DUP14>, 3, 14, 1};
    table[OP_DUP15] = {op_dup<OP_DUP15>, 3, 15, 1};
    table[OP_DUP16] = {op_dup<OP_DUP16>, 3, 16, 1};

    table[OP_SWAP1] = {op_swap<OP_SWAP1>, 3, 2, 0};
    table[OP_SWAP2] = {op_swap<OP_SWAP2>, 3, 3, 0};
    table[OP_SWAP3] = {op_swap<OP_SWAP3>, 3, 4, 0};
    table[OP_SWAP4] = {op_swap<OP_SWAP4>, 3, 5, 0};
    table[OP_SWAP5] = {op_swap<OP_SWAP5>, 3, 6, 0};
    table[OP_SWAP6] = {op_swap<OP_SWAP6>, 3, 7, 0};
    table[OP_SWAP7] = {op_swap<OP_SWAP7>, 3, 8, 0};
    table[OP_SWAP8] = {op_swap<OP_SWAP8>, 3, 9, 0};
    table[OP_SWAP9] = {op_swap<OP_SWAP9>, 3, 10, 0};
    table[OP_SWAP10] = {op_swap<OP_SWAP10>, 3, 11, 0};
    table[OP_SWAP11] = {op_swap<OP_SWAP11>, 3, 12, 0};
    table[OP_SWAP12] = {op_swap<OP_SWAP12>, 3, 13, 0};
    table[OP_SWAP13] = {op_swap<OP_SWAP13>, 3, 14, 0};
    table[OP_SWAP14] = {op_swap<OP_SWAP14>, 3, 15, 0};
    table[OP_SWAP15] = {op_swap<OP_SWAP15>, 3, 16, 0};
    table[OP_SWAP16] = {op_swap<OP_SWAP16>, 3, 17, 0};

    table[OP_LOG0] = {op_log<OP_LOG0>, 1 * 375, 2, -2};
    table[OP_LOG1] = {op_log<OP_LOG1>, 2 * 375, 3, -3};
    table[OP_LOG2] = {op_log<OP_LOG2>, 3 * 375, 4, -4};
    table[OP_LOG3] = {op_log<OP_LOG3>, 4 * 375, 5, -5};
    table[OP_LOG4] = {op_log<OP_LOG4>, 5 * 375, 6, -6};

    table[OP_CREATE] = {op_create, 32000, 3, -2};
    table[OP_CALL] = {op_call<EVMC_CALL>, 40, 7, -6};
    table[OP_CALLCODE] = {op_call<EVMC_CALLCODE>, 40, 7, -6};
    table[OP_RETURN] = {op_return<EVMC_SUCCESS>, 0, 2, -2};
    table[OP_INVALID] = {op_invalid, 0, 0, 0};
    table[OP_SELFDESTRUCT] = {op_selfdestruct, 0, 1, -1};
    return table;
}

constexpr op_table create_op_table_homestead() noexcept
{
    auto table = create_op_table_frontier();
    table[OP_DELEGATECALL] = {op_delegatecall, 40, 6, -5};
    return table;
}

constexpr op_table create_op_table_tangerine_whistle() noexcept
{
    auto table = create_op_table_homestead();
    table[OP_BALANCE].gas_cost = 400;
    table[OP_EXTCODESIZE].gas_cost = 700;
    table[OP_EXTCODECOPY].gas_cost = 700;
    table[OP_SLOAD].gas_cost = 200;
    table[OP_CALL].gas_cost = 700;
    table[OP_CALLCODE].gas_cost = 700;
    table[OP_DELEGATECALL].gas_cost = 700;
    table[OP_SELFDESTRUCT].gas_cost = 5000;
    return table;
}

constexpr op_table create_op_table_byzantium() noexcept
{
    auto table = create_op_table_tangerine_whistle();
    table[OP_RETURNDATASIZE] = {op_returndatasize, 2, 0, 1};
    table[OP_RETURNDATACOPY] = {op_returndatacopy, 3, 3, -3};
    table[OP_STATICCALL] = {op_staticcall, 700, 6, -5};
    table[OP_REVERT] = {op_return<EVMC_REVERT>, 0, 2, -2};
    return table;
}

constexpr op_table create_op_table_constantinople() noexcept
{
    auto table = create_op_table_byzantium();
    table[OP_SHL] = {op<shl>, 3, 2, -1};
    table[OP_SHR] = {op<shr>, 3, 2, -1};
    table[OP_SAR] = {op<sar>, 3, 2, -1};
    table[OP_EXTCODEHASH] = {op_extcodehash, 400, 1, 0};
    table[OP_CREATE2] = {op_create2, 32000, 4, -3};
    return table;
}

constexpr op_table create_op_table_istanbul() noexcept
{
    auto table = create_op_table_constantinople();
    table[OP_BALANCE] = {op_balance, 700, 1, 0};
    table[OP_CHAINID] = {op_chainid, 2, 0, 1};
    table[OP_EXTCODEHASH] = {op_extcodehash, 700, 1, 0};
    table[OP_SELFBALANCE] = {op_selfbalance, 5, 0, 1};
    table[OP_SLOAD] = {op_sload, 800, 1, 0};
    return table;
}

constexpr op_table op_tables[] = {
    create_op_table_frontier(),           // Frontier
    create_op_table_homestead(),          // Homestead
    create_op_table_tangerine_whistle(),  // Tangerine Whistle
    create_op_table_tangerine_whistle(),  // Spurious Dragon
    create_op_table_byzantium(),          // Byzantium
    create_op_table_constantinople(),     // Constantinople
    create_op_table_constantinople(),     // Petersburg
    create_op_table_istanbul(),           // Istanbul
    create_op_table_istanbul(),           // Berlin
};
static_assert(sizeof(op_tables) / sizeof(op_tables[0]) > EVMC_MAX_REVISION,
    "op table entry missing for an EVMC revision");
}  // namespace

EVMC_EXPORT const op_table& get_op_table(evmc_revision rev) noexcept
{
    return op_tables[rev];
}
}  // namespace evmone
