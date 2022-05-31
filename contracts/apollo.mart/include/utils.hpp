#pragma once

#include <string>
#include <algorithm>
#include <iterator>
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>


using namespace std;


template<typename T>
int128_t multiply(int128_t a, int128_t b) {
    int128_t ret = a * b;
    check(ret >= std::numeric_limits<T>::min() && ret <= std::numeric_limits<T>::max(),
          "overflow exception of multiply");
    return ret;
}

template<typename T>
int128_t divide_decimal(int128_t a, int128_t b, int128_t precision) {
    // with rounding-off method
    int128_t tmp = 10 * a * precision  / b;
    check(tmp >= std::numeric_limits<T>::min() && tmp <= std::numeric_limits<T>::max(),
          "overflow exception of divide_decimal");
    return (tmp + 5) / 10;
}

template<typename T>
int128_t multiply_decimal(int128_t a, int128_t b, int128_t precision) {
    // with rounding-off method
    int128_t tmp = 10 * a * b / precision;
    check(tmp >= std::numeric_limits<T>::min() && tmp <= std::numeric_limits<T>::max(),
          "overflow exception of multiply_decimal");
    return (tmp + 5) / 10;
}

#define divide_decimal64(a, b, precision) divide_decimal<int64_t>(a, b, precision)
#define multiply_decimal64(a, b, precision) multiply_decimal<int64_t>(a, b, precision)
#define multiply_i64(a, b) multiply<int64_t>(a, b)


string_view trim(string_view sv) {
    sv.remove_prefix(std::min(sv.find_first_not_of(" "), sv.size())); // left trim
    sv.remove_suffix(std::min(sv.size()-sv.find_last_not_of(" ")-1, sv.size())); // right trim
    return sv;
}
vector<string_view> split(string_view str, string_view delims = " ")
{
    vector<string_view> res;
    std::size_t current, previous = 0;
    current = str.find_first_of(delims);
    while (current != std::string::npos) {
        res.push_back(trim(str.substr(previous, current - previous)));
        previous = current + 1;
        current = str.find_first_of(delims, previous);
    }
    res.push_back(trim(str.substr(previous, current - previous)));
    return res;
}

int64_t to_int64(string_view s, const char* err_title) {
    errno = 0;
    uint64_t ret = std::strtoll(s.data(), nullptr, 10);
    check(errno == 0, string(err_title) + ": convert str to int64 error: " + std::strerror(errno));
    return ret;
}

uint64_t to_uint64(string_view s, const char* err_title) {
    errno = 0;
    uint64_t ret = std::strtoul(s.data(), nullptr, 10);
    check(errno == 0, string(err_title) + ": convert str to uint64 error: " + std::strerror(errno));
    return ret;
}
