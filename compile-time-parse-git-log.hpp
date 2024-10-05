#pragma once

#include <array>
#include <string_view>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
constexpr char git_log[] = {
#embed ".git/logs/HEAD"
};
#pragma clang diagnostic pop

namespace detail {
    template<std::array src, std::size_t start, std::size_t length>
    constexpr auto substr() {
        std::array<char, length> result{};
        std::copy(src.begin() + start, src.begin() + start + length, result.begin());
        return result;
    }
};

constexpr auto git_commit_hash = [](){    
    constexpr std::string_view sv{git_log, sizeof(git_log)};
    constexpr auto pos1 = sv.find_last_of('\n');
    constexpr auto pos2 = sv.find_last_of('\n', pos1-1);
    constexpr auto pos3 = pos2 == std::string_view::npos ? 0 : pos2; // the file might have only one line
    constexpr auto pos4 = sv.find(' ', pos3);

    return detail::substr<std::to_array(git_log), pos4+1, 40>();
}();
constexpr auto git_commit_hash_short = detail::substr<git_commit_hash, 0, 7>();
