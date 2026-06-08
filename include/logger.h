#ifndef LOGGER_H_
#define LOGGER_H_ 

#include <stdio.h>  
#include <stdlib.h> 

#define FATAL_C(msg)                         \
    do                                       \
    {                                        \
        fprintf(stderr, "FATAL: %s\n", msg); \
        exit(EXIT_FAILURE);                  \
    } while (0)

#ifdef __cplusplus
#include <initializer_list>
#include <iostream>
#include <string>

inline void FATAL(const std::string &list, bool to_exit = true)
{
    if (list.empty())
        return;
    std::cerr << list << std::endl;
    if (to_exit)
    {
        exit(1);
    }
}

// Changed to const std::string&
inline void SAY(const std::string &list)
{
    if (list.empty())
        return;
    std::cout << list << std::endl;
}

inline void SAY(std::initializer_list<std::string> list)
{
    if (list.size() == 0)
        return;
    for (auto begin = list.begin(); begin != list.end(); ++begin)
    {
        if (begin == (list.end() - 1))
        {
            std::cout << *begin;
        }
        else
        {
            std::cout << *begin << " ";
        }
    }
    std::cout << std::endl;
}

inline void FATAL(std::initializer_list<std::string> list, bool to_exit = true)
{
    if (list.size() == 0)
        return;
    for (auto begin = list.begin(); begin != list.end(); ++begin)
    {
        if (begin == (list.end() - 1))
        {
            std::cerr << *begin;
        }
        else
        {
            std::cerr << *begin << " ";
        }
    }
    std::cerr << std::endl;
    if (to_exit)
    {
        exit(1);
    }
}
#endif // __cplusplus

#endif // LOGGER_H_