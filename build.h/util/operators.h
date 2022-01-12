#pragma once

#include <vector>

template<typename T>
std::vector<T>& operator +=(std::vector<T>& s, T other) {
    s.push_back(std::move(other));
    return s;
}
template<typename T>
std::vector<T>& operator +=(std::vector<T>& s, std::initializer_list<T> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}
template<typename T>
std::vector<T>& operator +=(std::vector<T>& s, std::vector<T> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}

template<typename T, typename U>
std::vector<T>& operator +=(std::vector<T>& s, U other) {
    s.push_back(std::move(other));
    return s;
}
template<typename T, typename U>
std::vector<T>& operator +=(std::vector<T>& s, std::initializer_list<U> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}
template<typename T, typename U>
std::vector<T>& operator +=(std::vector<T>& s, std::vector<U> other) {
    s.insert(s.end(), std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
    return s;
}