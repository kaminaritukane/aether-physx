#pragma once
#include <array>
#include <cstddef>
#include <cassert>
#include <vector>
#include <zstd.h>
#include <aether/common/io/io.hh>

namespace aether {

/// Compress data before writing into the inferior writer
/// @param buffer_size the size of the internal buffer for copmression
template<typename Writer>
struct zstd_writer final : public aether::writer {
  private:
    using writer_type = Writer;
    size_t offset = 0;
    std::vector<char> buffer;
    writer_type &inferior;
    ZSTD_CCtx *ctx;

  public:
    ssize_t write(const void *in, size_t len) override final {
        ZSTD_inBuffer inb;
        inb.pos = 0;
        inb.size = len;
        inb.src = in;

        ZSTD_outBuffer outb;
        outb.pos = offset;
        outb.size = buffer.size();
        outb.dst = buffer.data();

        while (inb.pos < inb.size) {
            auto old_outpos = outb.pos, old_inpos = inb.pos;
            if (ZSTD_compressStream2(ctx, &outb, &inb, ZSTD_e_continue) < 0) {
                return -1;
            }
            if (old_outpos == outb.pos && old_inpos == inb.pos) {
                // We made no progress, flush output buffer
                if (write_all(inferior, buffer.data(), outb.pos) != 0) {
                    return -1;
                }
                outb.pos = 0;
            }
        }

        offset = outb.pos;
        return len;
    }

    int flush() override final {
        ZSTD_inBuffer inb;
        inb.pos = 0;
        inb.size = 0;
        inb.src = NULL;
        ZSTD_outBuffer outb;
        outb.pos = offset;
        outb.size = buffer.size();
        outb.dst = buffer.data();
        int ret = 0;
        do {
            ret = ZSTD_compressStream2(ctx, &outb, &inb, ZSTD_e_flush);
            if (ret < 0) {
                return -1;
            }
            if (write_all(inferior, buffer.data(), outb.pos) != 0) {
                return -1;
            }
            outb.pos = 0;
        } while (ret);

        offset = 0;
        return 0;
    }

    zstd_writer(writer_type &w, const size_t buffer_size = 0) : inferior(w) {
        ctx = ZSTD_createCCtx();
        ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, 0);
        ZSTD_CCtx_setParameter(ctx, ZSTD_c_contentSizeFlag, 0);
        buffer.resize(std::max(ZSTD_CStreamOutSize(), buffer_size), 0);
    }

    ~zstd_writer() {
        do {
            if (flush() < 0) {
                break;
            }

            // End the stream
            ZSTD_inBuffer inb;
            inb.pos = 0;
            inb.size = 0;
            inb.src = NULL;
            ZSTD_outBuffer outb;
            outb.pos = offset;
            outb.size = buffer.size();
            outb.dst = buffer.data();
            if (ZSTD_compressStream2(ctx, &outb, &inb, ZSTD_e_end) < 0) {
                break;
            }
            offset = outb.pos;

            flush();
        } while (false);

        ZSTD_freeCCtx(ctx);
    }
};

template<typename Reader>
struct zstd_reader final : public aether::reader {
private:
    using reader_type = Reader;
    reader_type &inferior;
    std::vector<char> buffer;
    ZSTD_DStream *ctx;
    ZSTD_inBuffer in_buf;

public:
    ssize_t read(void *out, const size_t len) override final {
        ZSTD_outBuffer out_buf;
        out_buf.pos = 0;
        out_buf.size = len;
        out_buf.dst = out;

        do {
            const int ret = ZSTD_decompressStream(ctx, &out_buf, &in_buf);
            if (ZSTD_isError(ret)) {
                return -1;
            }
            if (out_buf.pos == 0 && in_buf.pos == in_buf.size) {
                // No progress made and we have exhausted input, read more
                const int ret = inferior.read(buffer.data(), buffer.size());
                if (ret < 0) {
                    return -1;
                }
                if (ret == 0 && out_buf.pos == 0) {
                    // EOF
                    return 0;
                }
                in_buf.size = ret;
                in_buf.pos = 0;
            }
        } while(out_buf.pos == 0);

        return out_buf.pos;
    }

    zstd_reader(reader_type &r, const size_t buffer_size = 0) : inferior(r) {
        ctx = ZSTD_createDStream();
        assert(ctx != nullptr && "Failed to create zstd decompressor");
        buffer.resize(std::max(ZSTD_CStreamInSize(), buffer_size), 0);
        in_buf.src = buffer.data();
        in_buf.size = 0;
        in_buf.pos = 0;
    }

    ~zstd_reader() {
        ZSTD_freeDCtx(ctx);
    }
};

}
