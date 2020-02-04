#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <cstring>
#include <type_traits>
#include "net.hh"
#include "vector.hh"

namespace aether {

namespace compression {

size_t get_bit_size(uint32_t context);
uint8_t set_N_bits_left(uint8_t var, uint8_t n, bool set);
uint8_t set_N_bits_right(uint8_t var, uint8_t n, bool set);
void set_N_bits_left_ref(uint8_t& var, uint8_t n, bool set);
void set_N_bits_right_ref(uint8_t& var, uint8_t n, bool set);

/**
 *    Compression rules for given data context
 */
struct compression_config {
    compression_config();

    size_t compressed_vel_precision = 16;        //velocity = [x:compressed_vel_precision][y:compressed_vel_precision][z:compressed_vel_precision]

    size_t compressed_o_size;                    //orientation = [quat_index_precision][x:quat_compression_precision][y:quat_compression_precision][z:quat_compression_precision][w:0]
                                                //              = [quat_index_precision] + [quat_compression_precision] * 3

    float quat_max_size = 0.707107;                //max value for orientation
    float quat_min_size = -0.707107;            //min value for orientation
    size_t quat_index_precision = 2;            //<max num of all quat components>
    size_t quat_component_precision = 9;        //<size of 1 component>
    float quat_step;                            //min discrete step done between two values for orientation: this value describe compression loss for given property

    float size_max_size = 20.0;                    //max size of "size" in bits
    float size_min_size = 0.0;                    //min size of "size" in bits
    size_t size_precision = 3;                    //num of bits for "size" after decimal point

    float vel_size = 12;                        //max value for velocity
    float vel_step;                                //min discrete step done between two values for velocity: this value describe compression loss for given property

    uint32_t color_min_val = 0;                    //min val for color - note, currently excluded
    uint32_t color_max_val = 0;                    //max val for color - note, currently excluded

    uint32_t species_min_val = 0;                //min val for species
    uint32_t species_max_val = 3;                //max val for species

    uint32_t type_min_val = 0;                    //min val for type
    uint32_t type_max_val = 7;                    //max val for type

    uint32_t faction_min_val = 0;
    uint32_t faction_max_val = 2;
};

/**
 * Representation of compressed data:
 * - compressed data should have preallocated size via bitSize param in constructor.
 *     its preallocated size is defined in "size" property
 * - current size of compressed data is stored in "cursor" property ( its value defines amount of used bits )
 * - compressed data is not affected by memory alignment, and is stored in LIFO order
 * - it should be mid layer used for serialization in the future
 */
class packed_reader {
public:
    packed_reader(const uint8_t* blob, size_t bitSize, std::unique_ptr<compression_config> config);

    const uint8_t& operator[](size_t i) const { return bulk_data[i]; }

    /** Set of methods used to decompress specific properties from bytearray */

    //get from stack any type of aligned, not compressed data
    template<typename T> bool pop(T& dst, size_t pop_bit_size);

    //get from stack compressed quaternion
    bool pop_quat(net_quat& quat);

    //get from stack compressed velocity
    bool pop_velocity(vec3f& vel);

    //get from stack n bits of data based on max_val & min_val scope
    bool pop_4_b(uint32_t& bytes);

    //get from stack 1 byte of data ( memory must be aligned )
    bool pop_1_b(uint8_t& byte);

    //get from stack n bits of data based on max_val & min_val scope
    bool pop_4_b_packed(uint32_t& bytes, int32_t max_val, int32_t min_val);

    //get from stack n bits of data based on max_val & min_val scope, and number of bits after decimal point described by @precision input param
    bool pop_float_packed(float& bytes, float max_val, float min_val, uint32_t precision);

    //getter for "size"
    size_t get_size_bits() const { return size; }

    const compression_config &get_config() const;

    size_t get_cursor() const { return cursor; }

protected:
    //Current occupied size ( in bits ) of compressed data
    size_t cursor;

    //Allocated size ( in bits ) for compressed data
    size_t size;

    //Compressed data
    const uint8_t* bulk_data;

    //Configuration that describes compression rules for given packed_reader
    std::unique_ptr<compression_config> config;
};

class packed_writer {
public:
    packed_writer(std::unique_ptr<compression_config> config);

    uint8_t& operator[] (size_t i) { return bulk_data[i]; }
    const uint8_t& operator[] (size_t i) const { return bulk_data[i]; }

    /** Set of methods used to compress specific properties into bytearray */
    //Let define "compression stack" as order of compressed properties, not affected by memory alignment and affected by LIFO rules,
    //from set of method described below:

    //put on stack any type of data, with custom number of bits
    template<typename T> void append_N_bits(const T* data, size_t size);

    //put on stack any type of aligned, not compressed data
    void append(const uint8_t* _bulk_data, size_t size, uint64_t starting_bit=0);

    //put on stack compressed quaternion
    void append_quat(const net_quat &quat);

    //put on stack compressed velocity
    void append_velocity(const vec3f &vel);

    //put on stack 4 bytes of data ( memory must be aligned )
    void append_4_b(const uint32_t* bytes);

    //put on stack 1 byte of data ( memory must be aligned )
    void append_1_b(const uint8_t* byte);

    //put on stack n bits of data based on max_val & min_val scope
    void append_4_b_packed(const uint32_t* bytess, int32_t max_val, int32_t min_val);

    //put on stack n bits of data based on max_val & min_val scope, and number of bits after decimal point described by @precision input param
    void append_float_packed(const float* bytes, float max_val, float min_val, uint32_t precision);

    // Reserve specified number of bits for output buffer
    void reserve_bits(size_t n);

    /** Set of methods used to decompress specific properties from bytearray */

    //getter for "bulk_data"
    const uint8_t* get_data() const { return &bulk_data[0]; };

    //getter for "size" in bits
    size_t get_size_bits() const { return size; }

    //getter for "size" in bytes
    size_t get_size_bytes() const { return (size + 7) / 8; }

    static uint32_t get_precision(int32_t max_val, int32_t min_val);
    static uint32_t get_float_precision(float max_val, float min_val, uint32_t precision);

    const compression_config &get_config() const;

protected:

    //Current occupied size ( in bits ) of compressed data
    size_t cursor;

    //Allocated size ( in bits ) for compressed data
    size_t size;

    //Compressed data
    std::vector<uint8_t> bulk_data;

    //Configuration that describes compression rules for given packed_writer
    std::unique_ptr<compression_config> config;
};

template<typename T> void assign_byte(T& dst, const uint8_t* src, uint64_t bitStart);

template<typename T> void packed_writer::append_N_bits(const T *data, size_t size) {
    for (uint32_t byte_idx = 0; byte_idx < size; byte_idx += 8) {
        uint8_t append_size = byte_idx + 8 > size ? size % 8 : 8;
        uint8_t next_byte = ((*data) >> byte_idx);
        append(&next_byte, append_size, 8 - append_size);
    }
}

template<typename T> bool packed_reader::pop(T& dst, size_t pop_bit_size){
    static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value && std::is_fundamental<T>::value);
    if(((int32_t)cursor - (int32_t)pop_bit_size) < 0 ) return false;
    //pop right missaligned byte
    dst = static_cast<T>(0);

    bool alignment_flag = ((pop_bit_size % 8) > 0 && (pop_bit_size % 8)!= 8);
    int32_t first_index = alignment_flag ? pop_bit_size - (pop_bit_size % 8) : pop_bit_size - 8;
    for (int32_t byte_idx = first_index; byte_idx >=0; byte_idx-=8) {
        uint8_t next_byte = 0;
        assign_byte<uint8_t>(next_byte, &bulk_data[0], cursor - pop_bit_size + byte_idx);
        if (alignment_flag) {
            next_byte >>= (8 - pop_bit_size % 8);
            alignment_flag = false;
            dst = next_byte;
        }
        else {
            dst = (dst << 8);
            dst = (dst | next_byte);
        }
    }
    cursor -= pop_bit_size;
    return true;
}

template<typename T> void assign_byte(T& dst, const uint8_t* src, uint64_t bitStart) {
    memset(&dst, 0, sizeof(uint8_t));
    uint8_t byte1 = src[bitStart / 8];
    if (bitStart % 8 == 0) {
        dst = (byte1 & 0xFF);
        return;
    }
    uint8_t byte2_size = bitStart % 8;
    uint8_t byte1_size = 8 - byte2_size;
    uint8_t byte2 = src[bitStart / 8 + 1];
    dst = (T)((byte1 << byte2_size) | (byte2 >> byte1_size));
}

}

}
