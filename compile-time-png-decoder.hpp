#include <array>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <numeric>
#include <string_view>
#include <tuple>

namespace detail {
    template<std::array input, unsigned int pos>
    consteval uint32_t read_int() {
        uint32_t v = 0;
        v |= input[pos] << 24;
        v |= input[pos+1] << 16;
        v |= input[pos+2] << 8;
        v |= input[pos+3] << 0;
        return v;
    }

    template<std::array src, size_t start, size_t length, typename T = typename decltype(src)::value_type>
    constexpr auto substr() {
        std::array<T, length> result{};
        std::copy(src.begin() + start, src.begin() + start + length, result.begin());
        return result;
    }

    template<typename T, T pre, std::array array>
    constexpr auto prepend() {
        std::array<T, array.size()+1> result{};
        result[0] = pre;
        std::copy(array.begin(), array.end(), result.begin()+1);
        return result;
    }

    template<std::array a, std::array b>
    constexpr auto concat() {
        std::array<typename decltype(a)::value_type, a.size() + b.size()> result{};
        std::copy(a.begin(), a.end(), result.begin());
        std::copy(b.begin(), b.end(), result.begin() + a.size());
        return result;
    }

    struct png_chunk {
        uint32_t begin;
        uint32_t len;
        std::array<char, 4> type;
        uint32_t crc;
    };

    template<std::array input, unsigned int offset>
    constexpr unsigned int count_chunks() {
        if constexpr (offset >= input.size()) {
            return 0;
        } else {
            constexpr unsigned int chunk_len = read_int<input, offset>();
            return 1 + count_chunks<input, offset+chunk_len+12>();
        }
    }
    template<std::array input, unsigned int offset, unsigned int count>
    constexpr std::array<png_chunk, count> get_chunks() {
        if constexpr (count == 0) {
            return {};
        } else {
            constexpr unsigned int chunk_len = read_int<input, offset>();
            constexpr auto rest = get_chunks<input, offset+chunk_len+12, count-1>();
            constexpr png_chunk chunk = {
                .begin = offset,
                .len = chunk_len,
                .type = substr<input, offset+4, 4, char>(),
                .crc = read_int<input, offset+chunk_len+8>()
            };
            return prepend<png_chunk, chunk, rest>();
        }
    }

    struct ihdr {
        uint32_t width;
        uint32_t height;
        unsigned char bit_depth;
        unsigned char color_type;
        unsigned char compression_method;
        unsigned char filter_method;
        unsigned char interlace_method;
    };

    template<std::array input, png_chunk chunk>
    constexpr ihdr read_ihdr() {
        static_assert(std::string_view{chunk.type} == "IHDR", "Chunk must be IHDR");
        static_assert(chunk.len == 13, "IHDR chunk must be 13 bytes long");
        return {
            .width = read_int<input, chunk.begin+8>(),
            .height = read_int<input, chunk.begin+12>(),
            .bit_depth = input[chunk.begin+16],
            .color_type = input[chunk.begin+17],
            .compression_method = input[chunk.begin+18],
            .filter_method = input[chunk.begin+19],
            .interlace_method = input[chunk.begin+20]
        };
    }

    template<std::array input, std::array chunks>
    constexpr auto collect_idata() {
        static_assert(std::is_same_v<typename decltype(input)::value_type, unsigned char>, "Input must be unsigned char");

        constexpr size_t total_size = std::accumulate(chunks.begin(), chunks.end(), 0, [](size_t acc, const png_chunk& chunk) {
            if(std::string_view{chunk.type} == "IDAT")
                return acc + chunk.len;
            return acc;
        });

        std::array<unsigned char, total_size> result{};
        size_t pos = 0;
        for(const png_chunk& chunk : chunks) {
            if(std::string_view{chunk.type} == "IDAT") {
                std::copy(input.begin() + chunk.begin + 8, input.begin() + chunk.begin + 8 + chunk.len, result.begin() + pos);
                pos += chunk.len;
            }
        }
        return result;
    }

    template<std::array input>
    constexpr auto to_bitset() {
        std::bitset<input.size()*8> result{};
        for(unsigned int i = 0; i < input.size(); i++) {
            for(unsigned int j = 0; j < 8; j++) {
                result[i*8+j] = (input[i] >> j) & 1;
            }
        }
        return result;
    }

    template<size_t N>
    struct read_deflate_block_result {
        std::array<unsigned char, N> data;
        bool bfinal;
        unsigned int bits_read;
    };

    template<std::array input, unsigned int pos>
    constexpr auto read_deflate_block() {
        static_assert(std::is_same_v<typename decltype(input)::value_type, unsigned char>, "Input must be unsigned char");

        constexpr auto bitset = to_bitset<input>();

        constexpr bool bfinal = bitset[pos];
        constexpr unsigned char btype = (bitset[pos+2] << 1) | bitset[pos+1];

        if constexpr (btype == 0b00) { // no compression
            constexpr unsigned int ppos = pos + 3;
            constexpr unsigned int byte = ppos/8 + (ppos%8 == 0 ? 0 : 1);
            constexpr uint16_t len = input[byte] + (input[byte+1] << 8);
            constexpr int16_t nlen = static_cast<int16_t>(input[byte+2] + (input[byte+3] << 8));

            static_assert(len == ~(nlen), "Invalid length");

            std::array<unsigned char, len> result{};
            std::copy(input.begin() + byte + 4, input.begin() + byte + 4 + len, result.begin());
            return read_deflate_block_result{
                .data = result,
                .bfinal = bfinal,
                .bits_read = (byte + 4 + len)*8
            };
        } else if constexpr (btype == 0b01) { // fixed huffman codes
            static_assert(false, "Fixed huffman codes are not supported");
        } else if constexpr (btype == 0b10) { // dynamic huffman codes
            static_assert(false, "Dynamic huffman codes are not supported");
        } else if constexpr (btype == 0b11) { // reserved
            static_assert(false, "Reserved block type");
        } else {
            static_assert(false, "Invalid block type");
        }
    }

    template<std::array input, unsigned int pos = 0>
    constexpr auto read_deflate_blocks() {
        constexpr auto res = read_deflate_block<input, pos>();
        if constexpr (res.bfinal) {
            return res.data;
        } else {
            return concat<res.data, read_deflate_blocks<input, pos+res.bits_read>>();
        }
    }

    template<std::array input>
    consteval auto zlib_inflate() {
        static_assert(std::is_same_v<typename decltype(input)::value_type, unsigned char>, "Input must be unsigned char");

        constexpr unsigned char compression_method_flags = input[0];
        constexpr unsigned char flags = input[1];

        constexpr unsigned char compression_method = compression_method_flags & 0b00001111;
        constexpr unsigned char compression_info = compression_method_flags >> 4;
        static_assert(compression_method == 8, "Only deflate compression is supported");
        static_assert(compression_info <= 7, "Invalid compression info");

        constexpr bool fdict = flags & (1<<5);
        static_assert(((static_cast<uint16_t>(compression_method_flags)*256) + flags) % 31 == 0, "Invalid fcheck");
        static_assert(!fdict, "fdict is not supported");

        constexpr unsigned int window_size = 1 << (compression_info + 8);

        constexpr std::array compressed_data = substr<input, 2, input.size()-2-4>();
        constexpr auto decompressed_data = read_deflate_blocks<compressed_data>();

        static_assert(decompressed_data.size() == 3220);

        return decompressed_data;
    }

    template<typename T>
    constexpr std::make_unsigned_t<T> my_abs(T a) {
        return a < 0 ? -a : a;
    }
}

template<unsigned int width, unsigned int height>
struct image {
    std::array<unsigned char, width*height*4> data;
    static constexpr unsigned int get_width() { return width; }
    static constexpr unsigned int get_height() { return height; }

    using pixel = std::tuple<unsigned char, unsigned char, unsigned, unsigned char>;

    constexpr pixel get_pixel(unsigned int x, unsigned int y) const {
        return std::make_tuple(
            data[y*width*4 + x*4 + 0],
            data[y*width*4 + x*4 + 1],
            data[y*width*4 + x*4 + 2],
            data[y*width*4 + x*4 + 3]
        );
    }
};

template<std::array input>
consteval auto decode_png() {
    constexpr std::array<char, input.size()> signed_input = std::bit_cast<std::array<char, input.size()>>(input);
    constexpr std::array<unsigned char, input.size()> unsigned_input = std::bit_cast<std::array<unsigned char, input.size()>>(input);

    static_assert(unsigned_input[0] == 0x89, "Invalid png file");
    static_assert(std::string_view{signed_input.begin()+1,3} == "PNG", "Invalid png file");

    constexpr unsigned int chunk_count = count_chunks<unsigned_input, 8>();
    constexpr auto chunk_array = get_chunks<unsigned_input, 8, chunk_count>();

    static_assert(chunk_array.size() == chunk_count, "Something went wrong with reading chunks");
    static_assert(std::string_view{chunk_array[0].type} == "IHDR", "First chunk must be IHDR");

    constexpr detail::ihdr header = read_ihdr<unsigned_input, chunk_array[0]>();
    std::array<unsigned char, header.width*header.height*4> image_data{};

    constexpr auto idata = collect_idata<unsigned_input, chunk_array>();
    static_assert(idata.size() == 3231);
    constexpr auto inflated = zlib_inflate<idata>();

    for(unsigned int y = 0; y < header.height; y++) {
        unsigned int i = y * (header.width*4 + 1);
        unsigned int filter = inflated[i];

        for(unsigned int x = 0; x < header.width; x++) {
            for(unsigned int q = 0; q < 4; q++) {
                unsigned char offset;
                if(filter == 0) {
                    offset = 0;
                } else if(filter == 1) {
                    offset = x == 0 ? 0 : image_data[y*header.width*4 + 4*(x-1) + q];
                } else if(filter == 2) {
                    offset = y == 0 ? 0 : image_data[(y-1)*header.width*4 + 4*x + q];
                } else if(filter == 3) {
                    unsigned char a = x == 0 ? 0 : image_data[y*header.width*4 + 4*(x-1) + q];
                    unsigned char b = y == 0 ? 0 : image_data[(y-1)*header.width*4 + 4*x + q];
                    offset = (a+b)/2;
                } else if(filter == 4) {
                    unsigned char a = x == 0 ? 0 : image_data[y*header.width*4 + 4*(x-1) + q];
                    unsigned char b = y == 0 ? 0 : image_data[(y-1)*header.width*4 + 4*x + q];
                    unsigned char c = x == 0 || y == 0 ? 0 : image_data[(y-1)*header.width*4 + 4*(x-1) + q];
                    unsigned char p = a+b-c;
                    unsigned char pa = detail::my_abs(p-a);
                    unsigned char pb = detail::my_abs(p-b);
                    unsigned char pc = detail::my_abs(p-c);
                    offset = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
                }
                image_data[y*header.width*4 + 4*x + q] = inflated[i+4*x+q+1] + offset;
            }
        }
    }

    static_assert(std::string_view{chunk_array[chunk_array.size()-1].type} == "IEND", "Last chunk must be IEND");

    return image<header.width, header.height>{.data = image_data};
}
