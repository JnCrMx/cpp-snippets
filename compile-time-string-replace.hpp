#pragma once

#include <array>
#include <string_view>

template<std::array str, std::array from, std::array to>
constexpr auto replace_sub_str() {
    constexpr std::string_view sv{str.data(), str.size()};
    constexpr std::string_view from_sv{from.data(), from[from.size()-1] == '\0' ? from.size()-1 : from.size()};
    constexpr std::string_view to_sv{to.data(), to[to.size()-1] == '\0' ? to.size()-1 : to.size()};
    
    constexpr auto count = [](std::string_view sv, std::string_view needle) {
        unsigned int count = 0;
        std::size_t pos = 0;
        while((pos = sv.find(needle, pos)) != std::string_view::npos) {
            pos += needle.size();
            ++count;
        }
        return count;
    };
    constexpr unsigned int new_size = sv.size() + count(sv, from_sv) * (to_sv.size() - from_sv.size());
    std::array<char, new_size> result{};

    std::size_t pos = 0;
    std::size_t start = 0;
    std::size_t out_pos = 0;
    while((pos = sv.find(from_sv, start)) != std::string_view::npos) {
        std::copy(sv.begin() + start, sv.begin() + pos, result.begin() + out_pos);
        out_pos += pos - start;
        std::copy(to_sv.begin(), to_sv.end(), result.begin() + out_pos);
        out_pos += to_sv.size();
        start = pos + from_sv.size();
    }
    std::copy(sv.begin() + start, sv.end(), result.begin() + out_pos);

    return result;
}
