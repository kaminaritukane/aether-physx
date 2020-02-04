#include <aether/common/compression.hh>
#include <vector>
#include <algorithm>
#include <iterator>
#include <cstring>
#include <assert.h>
#include <aether/common/vector.hh>
#include <aether/common/net.hh>

namespace aether {

namespace compression {

compression_config::compression_config() {
    quat_step = (quat_max_size - quat_min_size) / pow(2, 9);
    vel_step = (2*vel_size) / (float)pow(2, compressed_vel_precision);
    compressed_o_size = quat_index_precision + 3 * quat_component_precision;
}

size_t get_bit_size(uint32_t context) {
    size_t cellBitSize = 32;
    for (uint32_t i = 0x80000000; i > 0; i /= 2) {
        if ((i & context) > 0) {
            break;
        }
        cellBitSize--;
    }

    return cellBitSize;
}

uint8_t set_N_bits_left(uint8_t var, uint8_t n, bool set)
{
    uint8_t new_var;
    set_N_bits_left_ref(new_var, n, set);
    return new_var;
}

uint8_t set_N_bits_right(uint8_t var, uint8_t n, bool set)
{
    uint8_t new_var;
    set_N_bits_right_ref(new_var, n, set);
    return new_var;
}

void set_N_bits_left_ref(uint8_t& var, uint8_t n, bool set) {
    if (set) {
        var = ( var | (((uint8_t)pow(2, 8) - 1) - ((uint8_t)pow(2, 8 - n) - 1)));
    }
    else {
        var = ( var & ((uint8_t)pow(2, 8 - n) - 1));
    }
}

void set_N_bits_right_ref(uint8_t& var, uint8_t n, bool set) {
    if (set) {
        var = (var | ((uint8_t)pow(2, n) - 1));
    }
    else {
        var = (var & (((uint8_t)pow(2, 8) - 1) - ((uint8_t)pow(2, n) - 1)));
    }
}

packed_writer::packed_writer(std::unique_ptr<compression_config> _config) :
    cursor(0), size(0), config(std::move(_config)) {
}

packed_reader::packed_reader(const uint8_t* blob, size_t bit_size, std::unique_ptr<compression_config> _config) :
    cursor(bit_size), size(bit_size), bulk_data(blob), config(std::move(_config)) {
}

void packed_writer::append(const uint8_t* _bulk_data, size_t size, uint64_t starting_bit) {
    const size_t new_size = this->size + size;
    this->size = new_size;
    bulk_data.resize((new_size + 8 - 1) / 8, 0);
    //append to first missaligned byte
    uint8_t missaligned_cursor_bits = 8 - (cursor % 8);
    if (missaligned_cursor_bits == 8) { missaligned_cursor_bits = 0; }

    if (missaligned_cursor_bits > 0) {
        uint8_t miassaligned_byte = 0;
        assign_byte<uint8_t>(miassaligned_byte, &_bulk_data[0], starting_bit);
        bulk_data[cursor / 8] |= (miassaligned_byte >> (8 - missaligned_cursor_bits));
        if (size < missaligned_cursor_bits) {
            set_N_bits_right_ref(bulk_data[cursor / 8], missaligned_cursor_bits - size, false);
            cursor += size;
            return;
        }
        cursor += missaligned_cursor_bits;
    }

    //append aligned bytes
    uint32_t bits_left = size - missaligned_cursor_bits;
    uint32_t byte_num = bits_left / 8;
    for (uint32_t byte_idx = 0; byte_idx < byte_num; byte_idx++) {
        assign_byte<uint8_t>(bulk_data[cursor / 8], &_bulk_data[0], starting_bit + missaligned_cursor_bits + byte_idx * 8);
        cursor += 8;
    }

    //append right missaligned byte
    uint8_t missaligned_bits = bits_left % 8;
    if (missaligned_bits > 0) {
        uint8_t last_byte = 0;
        assign_byte<uint8_t>(last_byte, &_bulk_data[0], starting_bit + byte_num * 8 + missaligned_cursor_bits);
        //set_N_bits_right_ref(last_byte, 8 - missaligned_bits, false);
        bulk_data[cursor / 8] = last_byte;
        set_N_bits_right_ref(bulk_data[cursor / 8], 8 - missaligned_bits, false);
        cursor += missaligned_bits;
    }
}

uint32_t packed_writer::get_precision(int32_t max_val, int32_t min_val) {
    uint32_t val_range = max_val - min_val;
    return get_bit_size(val_range);
}

uint32_t packed_writer::get_float_precision(float max_val, float min_val, uint32_t precision) {
    uint32_t val_range = (uint32_t)((max_val + 1) - (min_val - 1));
    return get_bit_size(val_range) + precision;
}

void packed_writer::append_quat(const net_quat &quat) {
    float A[4] = { quat.x, quat.y, quat.z, quat.w };

    //pick the biggest component to ignore
    uint8_t _idx;
    float _val = 0;
    for (uint8_t i = 0; i < 4; i++) {
        if (fabs(A[i]) > _val) {
            _idx = i;
            _val = fabs(A[i]);
        }
    }

    //Switch sign if the biggest component is negative
    if (A[_idx] < 0) {
        for (int i = 0; i < 4; i++) {
            A[i] *= -1;
        }
    }

    uint8_t _idx_flag = _idx << 6;
    append(&_idx_flag, config->quat_index_precision);
    for (int i = 0; i < 4; i++) {
        if (i != (int)_idx) {
            uint16_t int_range = ((A[i] + config->quat_max_size )/ config->quat_step);
            append_N_bits<uint16_t>(&int_range, config->quat_component_precision);
        }
    }
}

void packed_writer::append_velocity(const vec3f &vel) {
    uint32_t x_range = ((vel.x + config->vel_size) / config->vel_step);
    uint32_t y_range = ((vel.y + config->vel_size) / config->vel_step);
    uint32_t z_range = ((vel.z + config->vel_size) / config->vel_step);

    append_N_bits<uint32_t>(&x_range, config->compressed_vel_precision);
    append_N_bits<uint32_t>(&y_range, config->compressed_vel_precision);
    append_N_bits<uint32_t>(&z_range, config->compressed_vel_precision);
}

void packed_writer::append_4_b(const uint32_t* bytes) {
    append(reinterpret_cast<const uint8_t*>(bytes), sizeof(uint32_t) * 8);
}

void packed_writer::append_1_b(const uint8_t* byte) {
    append(byte, sizeof(uint8_t) * 8);
}

void packed_writer::append_4_b_packed(const uint32_t* bytess, int32_t max_val, int32_t min_val) {
    uint32_t range_bit_size = get_precision(max_val, min_val);
    uint32_t out = (uint32_t)((*bytess - min_val));
    append_N_bits<uint32_t>(&out, range_bit_size);
}

void packed_writer::append_float_packed(const float* bytes, float max_val, float min_val, uint32_t precision) {
    float val_range = max_val - min_val;
    uint32_t full_range_bit_size = get_float_precision(max_val, min_val, precision);
    float step = (val_range) / pow(2, full_range_bit_size);
    uint32_t out = (*bytes - min_val) / step;
    append_N_bits<uint32_t>(&out, full_range_bit_size);
}

void packed_writer::reserve_bits(const size_t n) {
    bulk_data.reserve((n + 7) / 8);
}
bool packed_reader::pop_float_packed(float& bytes, float max_val, float min_val, uint32_t precision) {
    uint32_t out;
    float val_range = max_val - min_val;
    uint32_t full_range_bit_size = packed_writer::get_float_precision(max_val, min_val, precision);
    float step = (val_range) / pow(2, full_range_bit_size);
    bool result = pop<uint32_t>(out, full_range_bit_size);
    bytes = out * step + min_val;
    return result;
}

const compression_config &packed_writer::get_config() const {
    return *config.get();
}

const compression_config &packed_reader::get_config() const {
    return *config.get();
}

bool packed_reader::pop_quat(net_quat& quat) {
    auto parse_component_index = [&quat](uint32_t idx, float inVal) {
        switch (idx) {
            case 0:    quat.x = inVal; break;
            case 1: quat.y = inVal; break;
            case 2: quat.z = inVal; break;
            case 3: quat.w = inVal; break;
        }
    };

    uint16_t components[3];
    for (uint8_t idx = 0; idx < 3; idx++) {
        bool result = pop<uint16_t>(components[2 - idx], config->quat_component_precision);
	if(!result) return false;
    }
    uint8_t max_comp_idx;
    bool result = pop<uint8_t>(max_comp_idx, config->quat_index_precision);
    if(!result) return false;
    float modulo = 0.0;
    for (uint8_t idx = 0, comp_idx = 0; idx < 4; idx++, comp_idx++) {
        if (max_comp_idx == idx) {
            comp_idx--;
            continue;
        }

        float value = config->quat_step * components[comp_idx] - config->quat_max_size;
        modulo += powf(value, 2);

        parse_component_index(idx, value);
    }
    parse_component_index(max_comp_idx, sqrt(1 - modulo));
    return true;
}

bool packed_reader::pop_velocity(vec3f& vel) {
    uint32_t x_range_int = 0;
    uint32_t y_range_int = 0;
    uint32_t z_range_int = 0;
    bool result = pop<uint32_t>(z_range_int, config->compressed_vel_precision);
    if(!result) return false;
    result = pop<uint32_t>(y_range_int, config->compressed_vel_precision);
    if(!result) return false;
    result = pop<uint32_t>(x_range_int, config->compressed_vel_precision);
    if(!result) return false;
    vel.x = (x_range_int * config->vel_step) - config->vel_size;
    vel.y = (y_range_int * config->vel_step) - config->vel_size;
    vel.z = (z_range_int * config->vel_step) - config->vel_size;
    return true;
}

bool packed_reader::pop_4_b(uint32_t& bytes) {
    return pop<uint32_t>(bytes, 32);
}

bool packed_reader::pop_1_b(uint8_t& byte) {
    return pop<uint8_t>(byte, 8);
}

bool packed_reader::pop_4_b_packed(uint32_t& bytes, int32_t max_val, int32_t min_val) {
    uint32_t range_bit_size = packed_writer::get_precision(max_val, min_val);
    uint32_t out;
    bool result = pop<uint32_t>(out, range_bit_size);
    bytes = out + min_val;
    return result;
}

}

}
