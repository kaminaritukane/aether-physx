#pragma once

#include <optional>
#include <type_traits>
#include <cassert>
#include <algorithm>
#include <cstring>
#include <array>
#include <limits>
#include <array>
#include <climits>
#include <vector>
#include <aether/common/io/io.hh>

namespace aether {
namespace netcode {
namespace transcode {

/// Densely pack a stream of bits into a byte array
struct bit_appender {
  private:
    std::vector<uint8_t> &output;
    size_t total_bits = 0;

    void push_bits_aligned(const uint8_t *input, size_t nbits) {
        // Precondition: total_bits % CHAR_BIT == 0
        assert((total_bits & (CHAR_BIT - 1)) == 0 && "Output buffer not aligned");
        const size_t input_length_ceil_bytes = (nbits + CHAR_BIT - 1) / CHAR_BIT;
        output.insert(output.end(), input, input + input_length_ceil_bytes);
        total_bits += nbits;
        zero_trailing_bits();
    }

    void zero_trailing_bits() {
        const size_t final_bits = total_bits & (CHAR_BIT - 1);
        if (final_bits != 0) {
            const auto keep = ((static_cast<uint8_t>(1) << final_bits) - 1);
            output.back() &= keep;
        }
    }

    void push_bits_unaligned(const uint8_t *input, const size_t nbits) {
        // Precondition: total_bits % CHAR_BIT != 0
        const auto offset = total_bits & (CHAR_BIT - 1);
        assert(offset != 0 && "Output buffer unexpectedly is aligned");
        output.resize((total_bits + nbits + CHAR_BIT - 1) / CHAR_BIT);
        const size_t num_input_bytes = (nbits + CHAR_BIT - 1) / CHAR_BIT;
        uint8_t *output_start = &output[total_bits / CHAR_BIT];
        uint8_t carried_over = 0;
        for (size_t i = 0; i < num_input_bytes; ++i) {
            const auto input_char = input[i];
            const auto left_char = input_char << offset;
            output_start[i] |= carried_over | left_char;
            carried_over = input_char >> (CHAR_BIT - offset);
        }
        // We have this since the number of input bytes and the number of modified
        // bytes may differ.
        if (offset + (nbits & (CHAR_BIT - 1)) != 0) {
            output.back() |= carried_over;
        }
        total_bits += nbits;
        zero_trailing_bits();
    }

  public:
    /// Pack `nbits` bits from `input` into the output array
    /// @param input the input bits
    /// @param nbits the number of bits
    void push_bits(const uint8_t *input, const size_t nbits) {
        const size_t new_size = total_bits + nbits;
        if ((total_bits & (CHAR_BIT - 1)) != 0) {
            push_bits_unaligned(input, nbits);
        } else {
            push_bits_aligned(input, nbits);
        }
        assert(total_bits == new_size);
        assert(output.size() == (total_bits + CHAR_BIT - 1) / CHAR_BIT);
    }

    /// @return number of bits in the output array
    size_t size_bits() const {
        return total_bits;
    }

    /// Create an appender that appends into `v`, assuming there is already `nbits` written there
    bit_appender(std::vector<uint8_t> &_output, const size_t _total_bits)
        : output(_output), total_bits(_total_bits) {
        assert((total_bits + CHAR_BIT - 1) / CHAR_BIT == output.size());
        zero_trailing_bits();
    }
};

/// Wraps an array of bytes and provides an interface for reading arbitrary number of bits
/// from the array
struct bit_stream {
  private:
    const std::vector<uint8_t> &input;
    int padding_bits;
    size_t offset;

    size_t get_bits_aligned(uint8_t *output, size_t nbits) {
        if (nbits == 0) {
            return 0;
        }
        size_t total_bits = input.size() * 8 - offset - padding_bits;
        assert(nbits <= total_bits);
        std::memcpy(output, input.data() + (offset / 8), (nbits + 7) / 8);
        if (nbits % 8 != 0) {
            // clear the higher bits outside of 0~nbits
            output[(nbits - 1) / 8] &= uint8_t((1 << (nbits % 8)) - 1);
        }
        offset += nbits;
        return nbits;
    }

    size_t get_bits_unaligned(uint8_t *output, size_t nbits) {
        if (nbits == 0) {
            return 0;
        }
        std::size_t eight{8};
        assert(nbits <= input.size() * 8 - offset - padding_bits);
        for (size_t i = 0; i * 8 < nbits; i++) {
            // how many bits is still left to be written for this byte
            size_t bits_left = std::min(nbits - i * 8, eight);
            uint8_t this_byte = input[offset / 8 + i] >> (offset % 8);
            this_byte &= uint8_t((1 << std::min(bits_left, 8ul - offset % 8ul)) - 1);
            if (bits_left + offset % 8 > 8) {
                // needs to read from next byte as well
                bits_left -= 8 - offset % 8;
                this_byte |= (input[offset / 8 + i + 1] & uint8_t((1 << bits_left) - 1))
                             << (8 - offset % 8);
            }
            output[i] = this_byte;
        }
        offset += nbits;
        return nbits;
    }

  public:
    /// Construct a bit stream from an array of bytes.
    /// Assuming there are `padding_bits_` trailing padding bits in `v`
    bit_stream(const std::vector<uint8_t> &v, size_t total_bits, size_t offset_ = 0)
        : input(v), padding_bits(v.size() * 8 - total_bits), offset(offset_) {
    }

    /// same as get_bits(vector, size_t), but assuming the buffer is already allocated
    size_t get_bits(uint8_t *output, size_t nbits) {
        size_t total_bits = input.size() * 8 - offset - padding_bits;
        nbits = std::min(nbits, total_bits);
        if (offset % 8 == 0) {
            return get_bits_aligned(output, nbits);
        } else {
            return get_bits_unaligned(output, nbits);
        }
    }

    // get nbits into `output`, padded to byte boundary with 0s
    size_t get_bits(std::vector<uint8_t> &output, size_t nbits) {
        size_t total_bits = input.size() * 8 - offset - padding_bits;
        nbits = std::min(nbits, total_bits);
        output.resize((nbits + 7) / 8);
        return get_bits(output.data(), nbits);
    }
};

/// Base interface for Transcoders
template <typename T>
struct transcode_base {
  public:
    using Item = T;
    virtual bool encode(const T &input, bit_appender &w) = 0;
    virtual bool encode_stream(reader &r, bit_appender &w) {
        T data;
        if (read_exact(r, &data, sizeof(data)) != 0) {
            return false;
        }
        return encode(data, w);
    }
    virtual bool decode(bit_stream &r, T &out) = 0;
    virtual bool decode_stream(bit_stream &r, writer &w) {
        T data;
        if (!decode(r, data)) {
            return false;
        }
        return write_all(w, &data, sizeof(data)) == 0;
    }
    virtual ~transcode_base() {
    }
};

/// Base for transformers. Transcoders are basically semi-inversible functions that
/// takes an `S` and returns a `T`;
template <typename S, typename T>
struct transform_base {
  public:
    using Output = T;
    using Input = S;
    /// transform `input` into `output`
    /// @return whether the input is valid
    virtual bool apply(const S &input, T &output) = 0;
    /// invert the tranformation
    /// @return whehter input is valid
    virtual bool invert(const T &input, S &output) = 0;
    virtual ~transform_base() {
    }
};

template <
    typename T1,
    typename T2,
    std::enable_if_t<std::is_same<typename T1::Output, typename T2::Input>::value, int> =
        0>
struct transform_compose
    : public transform_base<typename T1::Input, typename T2::Output> {
    T1 t1;
    T2 t2;

    bool
    apply(const typename T1::Input &input, typename T2::Output &output) override final {
        typename T1::Output intermediate;
        const auto valid = t1.apply(input, intermediate);
        if (!valid) {
            return false;
        }
        return t2.apply(intermediate, output);
    }

    bool
    invert(const typename T2::Output &input, typename T1::Input &output) override final {
        typename T1::Output intermediate;
        const auto valid = t2.invert(input, intermediate);
        if (!valid) {
            return false;
        }
        return t1.invert(intermediate, output);
    }
};

template <typename T>
struct as_uint64 : public transform_base<T, uint64_t> {
  public:
    bool apply(const T &input, uint64_t &output) override final {
        output = static_cast<uint64_t>(input);
        assert(
            input == static_cast<T>(output) &&
            "Conversion to uint64_t will not invert correctly");
        if (std::is_signed<T>::value) {
            output = (output << 1) | (output >> 63);
        }
        return true;
    }

    bool invert(const uint64_t &_input, T &output) override final {
        auto input = _input;
        if (std::is_signed<T>::value) {
            input = (input >> 1) | (input << 63);
        }
        output = static_cast<T>(input);
        return true;
    }
};

/// Stateful transformer that XORs values
template <typename T>
struct integer_delta_transform : public transform_base<T, typename std::make_signed<T>::type> {
    using value_type = T;
    using unsigned_type = typename std::make_unsigned<value_type>::type;
    using transformed_type = typename std::make_signed<value_type>::type;

    unsigned_type last_input = 0;
    unsigned_type last_output = 0;

    bool apply(const T &input, transformed_type &output) override final {
        const auto u_input = static_cast<unsigned_type>(input);
        const auto u_output = u_input - last_input;
        last_input = u_input;
        output = static_cast<transformed_type>(u_output);
        return true;
    }

    bool invert(const transformed_type &input, T &output) override final {
        const auto u_input = static_cast<unsigned_type>(input);
        const auto u_output = u_input + last_output;
        last_output = u_output;
        output = static_cast<value_type>(u_output);
        return true;
    }
};

/// Cast a float point number to integer
template <
    typename F,
    typename I,
    std::
        enable_if_t<std::is_integral<I>::value && std::is_floating_point<F>::value, int> =
            0>
struct to_integer : public transform_base<F, I> {
  public:
    bool apply(const F &input, I &output) override final {
        output = I(input);
        return true;
    }
    // Lossy inversion
    bool invert(const I &input, F &output) override final {
        output = F(input);
        return true;
    }
};

/// scale a number up by `scale`
template <typename T, int64_t Scale>
struct scale : public transform_base<T, T> {
    static constexpr int64_t value = Scale;

    bool apply(const T &input, T &output) override final {
        output = input * T(value);
        return true;
    }
    /// Potentially lossy
    bool invert(const T &input, T &output) override final {
        output = input / T(value);
        return true;
    }
};

/// clamp a number to range [lower, upper]
template <typename T, int64_t lower, int64_t upper>
struct clamp : public transform_base<T, T> {
  public:
    bool apply(const T &input, T &output) override final {
        if (input > T(upper)) {
            output = T(upper);
        } else if (input < T(lower)) {
            output = T(lower);
        } else {
            output = input;
        }
        return true;
    }
    bool invert(const T &input, T &output) override final {
        if (input > T(upper) || input < T(lower)) {
            return false;
        }
        output = input;
        return true;
    }
};

/// A coder that transforms the input with `Transformer`, then encodes it with `Coder`
template <typename T, typename Transformer, typename Coder>
struct transform_coder final : public transcode_base<T> {
  private:
    static_assert(
        std::is_base_of<transcode_base<typename Coder::Item>, Coder>::value,
        "Coder is not a transcoder");
    static_assert(
        std::is_base_of<transform_base<T, typename Coder::Item>, Transformer>::value,
        "Transformer argument is not a valid Transformer");
    Transformer t{};
    Coder c{};

  public:
    static constexpr size_t bit_size = Coder::bit_size;
    bool encode(const T &input, bit_appender &w) override final {
        typename Transformer::Output tmp;
        if (!t.apply(input, tmp)) {
            return false;
        }
        return c.encode(tmp, w);
    }

    bool decode(bit_stream &r, T &out) override final {
        typename Transformer::Output tmp;
        if (!c.decode(r, tmp)) {
            return false;
        }
        return t.invert(tmp, out);
    }
};

/// Transform a number from [old_base, +inf) to [0, +inf). By subtracting `old_base`
template <typename T, T old_base>
struct Rebase : public transform_base<T, T> {
  public:
    bool apply(const T &input, T &output) override final {
        if (input < old_base) {
            return false;
        }
        output = input - old_base;
        return true;
    }
    bool invert(const T &input, T &output) override final {
        if (input < 0) {
            return false;
        }
        output = input + old_base;
        return true;
    }
};

/// Copy `T` verbatim
template <typename T>
struct identity : public transcode_base<T> {
  public:
    static constexpr size_t bit_size = sizeof(T) * CHAR_BIT;
    bool encode(const T &input, bit_appender &w) override final {
        w.push_bits(reinterpret_cast<const uint8_t *>(&input), bit_size);
        return true;
    }
    bool decode(bit_stream &r, T &out) override final {
        if (r.get_bits(reinterpret_cast<uint8_t *>(&out), bit_size) != bit_size) {
            return false;
        }
        return true;
    }
};

/// Encodes a boolean value using 1 bit
struct boolean : public transcode_base<bool> {
  public:
    static constexpr size_t bit_size = 1;
    bool encode(const bool &input, bit_appender &w) override final {
        int tmp = input;
        w.push_bits(reinterpret_cast<const uint8_t *>(&tmp), 1);
        return true;
    }
    bool decode(bit_stream &r, bool &out) override final {
        int tmp = 0;
        if (r.get_bits(reinterpret_cast<uint8_t *>(&tmp), 1) != 1) {
            return false;
        }
        out = tmp;
        return true;
    }
};

/// Encodes a integer within [0, limit) using as little bits as possible
template <typename T, T limit, std::enable_if_t<std::is_integral<T>::value, int> = 0>
struct finite_int final : public transcode_base<T> {
  private:
    static_assert(limit > 0, "limit must be greater than 0");
    static constexpr int log2(int64_t n) {
        int ans = 0, tmp = 1;
        while (tmp < n) {
            ans++;
            tmp *= 2;
        }
        return ans;
    }

  public:
    static constexpr int bit_size = log2(limit);
    bool encode(const T &input, bit_appender &w) override final {
        if (input >= limit) {
            return false;
        }
        w.push_bits(reinterpret_cast<const uint8_t *>(&input), bit_size);
        return true;
    }

    bool decode(bit_stream &r, T &out) override final {
        std::array<uint8_t, sizeof(T)> buf{};
        int nbits = r.get_bits(buf.data(), bit_size);
        if (nbits != bit_size) {
            return false;
        }
        auto tmp = *reinterpret_cast<T *>(buf.data());
        if (tmp >= limit) {
            return false;
        }
        out = tmp;
        return true;
    }
};

template <typename T, T ptr, typename Coder>
struct struct_member;

/// Represents a member in the struct, and how it should be encoded
template <typename T, typename S, T S::*ptr, typename Coder>
struct struct_member<T S::*, ptr, Coder> {
    static_assert(
        std::is_base_of<transcode_base<typename Coder::Item>, Coder>::value,
        "Coder is not a transcoder");
    static_assert(
        std::is_same<T, typename Coder::Item>::value,
        "Coder is incompatible with member");
};

#define MAKE_STRUCT_MEMBER(ptr, ...) struct_member<decltype(ptr), ptr, __VA_ARGS__>

/// Encode a C++ struct according to `Spec`. `Spec` must be a list of struct_members.
/// struct members will be encoded into the bit_appender in the order they appear in the `Spec`.
template <typename T, typename... Spec>
struct struct_coder;

/// ditto
template <typename T>
struct struct_coder<T> : public transcode_base<T> {
  public:
    static constexpr size_t bit_size = 0;
    bool encode(const T &input, bit_appender &w) override final {
        return true;
    }
    bool decode(bit_stream &r, T &output) override final {
        return true;
    }
};

/// ditto
template <typename T, typename S, typename Coder, S T::*ptr, typename... Rest>
struct struct_coder<T, struct_member<S T::*, ptr, Coder>, Rest...> final
    : public transcode_base<T> {
  private:
    Coder c;
    struct_coder<T, Rest...> rest;

  public:
    static constexpr size_t bit_size = Coder::bit_size + decltype(rest)::bit_size;
    bool encode(const T &input, bit_appender &w) override final {
        if (!c.encode(input.*ptr, w)) {
            return false;
        }
        return rest.encode(input, w);
    }
    bool decode(bit_stream &r, T &out) override final {
        if (!c.decode(r, out.*ptr)) {
            return false;
        }
        return rest.decode(r, out);
    }
};

template <typename T, int index, typename BitMaskMember, typename... Spec>
struct optional_struct_coder_impl;

template <typename T, int index, typename BitMaskMember>
struct optional_struct_coder_impl<T, index, BitMaskMember> final {
  public:
    bool encode(const T &input, bit_appender &w) override final {
        return true;
    }
    bool decode(bit_stream &r, T &out) override final {
        return true;
    }
};

template <
    typename T,
    typename S,
    typename BM,
    int index,
    typename BitMaskCoder,
    BM T::*bit_mask_ptr,
    S T::*ptr,
    typename Coder,
    typename... Rest>
struct optional_struct_coder_impl<
    T,
    index,
    struct_member<BM T::*, bit_mask_ptr, BitMaskCoder>,
    struct_member<S T::*, ptr, Coder>,
    Rest...>
    final {
  private:
    Coder c;
    optional_struct_coder_impl<
        T,
        index + 1,
        struct_member<BM T::*, bit_mask_ptr, BitMaskCoder>,
        Rest...>
        rest;
    static_assert(
        std::is_base_of<transcode_base<S>, Coder>::value,
        "Coder is incompatible with member");

  public:
    bool encode(const T &input, bit_appender &w) {
        if (input.*bit_mask_ptr & (1 << index)) {
            return c.encode(input.*ptr, w);
        }
        return rest.encode(input, w);
    }
    bool decode(BM bit_mask, bit_stream &r, T &out) {
        if (bit_mask_ptr & (1 << index)) {
            return c.decode(r, out.*ptr);
        }
        return rest.decode(bit_mask, r, out);
    }
};

/// A struct coder that can skip fields based on a bit mask. The bit mask itself
/// has to be part of the struct you want to transcode.
/// @param BitMaskMember a struct_member<> describing how the bit mask field itself
//                       should be transcoded
//  @param Spec          a series of struct_member<> describing the fields in the
//                       struct
template <typename T, typename BitMaskMember, typename... Spec>
struct optional_struct_coder;

template <
    typename T,
    typename BM,
    typename BitMaskCoder,
    BM T::*bit_mask_ptr,
    typename... Spec>
struct optional_struct_coder<
    T,
    struct_member<BM T::*, bit_mask_ptr, BitMaskCoder>,
    Spec...> : public transcode_base<T> {
  private:
    BitMaskCoder bit_mask_coder;
    optional_struct_coder_impl<
        T,
        0,
        struct_member<BM T::*, bit_mask_ptr, BitMaskCoder>,
        Spec...>
        inferior;

  public:
    bool encode(const T &input, bit_appender &w) override final {
        if (!bit_mask_coder(input.*bit_mask_ptr, w)) {
            return false;
        }
        return inferior.encode(input, w);
    }
    bool decode(bit_stream &r, T &out) {
        if (!bit_mask_coder(r, out.*bit_mask_ptr)) {
            return false;
        }
        return inferior.decode(out.*bit_mask_ptr, r, out);
    }
};

// clang-format off
/// Encode an integer value within the range of [lower, upper)
template <
    typename T,
    T lower,
    T upper,
    std::enable_if_t<lower<upper, int> = 0>
using bounded_int =
        transform_coder<T, Rebase<T, lower>, finite_int<T, upper - lower>>;
// clang-format on

/// Encode an integer value by clamping it to range [lower, upper)
template <
    typename T,
    T lower,
    T upper,
    std::enable_if_t<lower<upper, int> = 0> using clamped_int = transform_coder<
        T,
        clamp<T, lower, upper - 1>,
        transform_coder<T, Rebase<T, lower>, finite_int<T, upper - lower>>>;

/// Encode a floating point in the range of [ceil(lower / scale), floor(upper / scale)], as a fixed point number
template <
    typename T,
    int64_t Scale,
    int64_t Lower,
    int64_t Upper,
    std::enable_if_t<std::is_floating_point<T>::value, int> = 0>
using BoundedFixedPoint = transform_coder<
    T,
    scale<T, Scale>,
    transform_coder<T, to_integer<T, int64_t>, bounded_int<int64_t, Lower, Upper>>>;

/// Encode a floating point by scaling it up by `scale`, then clamped it to [lower, upper)
template <
    typename T,
    int64_t Scale,
    int64_t Lower,
    int64_t Upper,
    std::enable_if_t<std::is_floating_point<T>::value, int> = 0>
using clamped_fixed_point = transform_coder<
    T,
    scale<T, Scale>,
    transform_coder<T, to_integer<T, int64_t>, clamped_int<int64_t, Lower, Upper>>>;

/// Encodes a integer using a variable length encoding
template <
    typename T,
    std::enable_if_t<std::is_integral<T>::value, int> = 0>
struct variable_int final : public transcode_base<T> {
    // This is invalid since the compile-time size of the encoding is unknown
    static const size_t bit_size = 0;

    bool encode(const T &_input, bit_appender &w) override final {
        uint64_t input = to_u64(_input);
        std::array<unsigned char, 10> encoded;
        size_t length = 0;
        do {
            encoded[length] = input & 127;
            input >>= 7;
            if (input != 0) {
                encoded[length] |= 128;
            }
            ++length;
        } while (input != 0);

        w.push_bits(reinterpret_cast<const uint8_t *>(&encoded[0]), length * CHAR_BIT);
        return true;
    }

    bool decode(bit_stream &r, T &out) override final {
        uint64_t output = 0;
        unsigned char current;
        size_t offset = 0;
        do {
            const auto nbits =
                r.get_bits(reinterpret_cast<uint8_t *>(&current), CHAR_BIT);
            if (nbits != CHAR_BIT) {
                return false;
            }
            output |= (static_cast<uint64_t>(current & 127) << offset);
            offset += 7;
        } while ((current & 128) != 0);
        out = from_u64(output);
        return true;
    }

private:
    static uint64_t to_u64(const T& v) {
        if (std::is_unsigned<T>::value) {
            return static_cast<uint64_t>(v);
        } else {
            const auto v_i64 = static_cast<int64_t>(v);
            const uint64_t sign = v_i64 < 0 ? 1 : 0;
            auto v_u64 = static_cast<uint64_t>(v);
            if (sign > 0) {
                v_u64 = 0 - v_u64 - 1;
            }
            v_u64 = (v_u64 << 1) | sign;
            return v_u64;
        }
    }

    static T from_u64(const uint64_t v) {
        if (std::is_unsigned<T>::value) {
            return static_cast<T>(v);
        } else {
            uint64_t v_u64 = v;
            const auto sign = v_u64 & 1;
            v_u64 = v_u64 >> 1;
            if (sign) {
                v_u64 = 0 - v_u64 - 1;
            }
            auto v_i64 = static_cast<int64_t>(v_u64);
            return static_cast<T>(v_i64);
        }
    }
};

template <typename T>
using unbounded_integer_delta = transform_coder<
    T,
    transform_compose<as_uint64<T>, integer_delta_transform<uint64_t>>,
    variable_int<int64_t>>;

template <typename T, int64_t Scale>
using scaled_fixed_point_delta = transform_coder<
    T,
    scale<T, Scale>,
    transform_coder<
        T,
        transform_compose<
            transform_compose<to_integer<T, int64_t>, as_uint64<int64_t>>,
            integer_delta_transform<uint64_t>>,
        variable_int<int64_t>>>;

} // namespace transcode
} // namespace netcode
} // namespace aether
