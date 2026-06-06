#ifndef LOGGER_H_

#ifdef __cplusplus
#include <initializer_list>
#include <iostream>
#include <string> 
inline void FATAL(std::string list, bool to_exit = true) {
    if (list.size() == 0) return;
    std::cerr << list << std::endl;
    if (to_exit) {
        exit(1);
    }
}

inline void SAY(std::string list) {
    if (list.size() == 0) return;
    std::cout << list << std::endl;
}

inline void SAY(std::initializer_list<std::string> list) {
    if (list.size() == 0) return;
    for (auto begin = list.begin(); begin != list.end(); ++begin) {
        if (begin == (list.end()-1)) {
            std::cout << *begin;
        } else {
            std::cout << *begin;
            std::cout << " ";
        }
    }
    std::cout << std::endl;
}

inline void FATAL(std::initializer_list<std::string> list, bool to_exit = true) {
    if (list.size() == 0) return;
    for (auto begin = list.begin(); begin != list.end(); ++begin) {
        if (begin == (list.end()-1)) {
            std::cerr << *begin;
        } else {
            std::cerr << *begin;
            std::cerr << " ";
        }
    }
    std::cerr << std::endl;
    if (to_exit) {
        exit(1);
    }
}
#endif
#endif