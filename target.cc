#include <iostream>
#include <signal.h>
#include <fstream>
#include <vector>

int process_going(std::vector<char> &data)
{
    if (data.size() < 1)
        return 0;

    int sum = 0;
    for (char c : data)
        sum += (unsigned char)c;

    if (data.size() >= 3 && data[0] == 'A' && data[1] == 'B' && data[2] == 'C')
    {
        std::cout << "[process] CRASH TRIGGER DETECTED" << std::endl;
        int *p = nullptr;
        *p = 0;
    }

    return sum;
}

int main(int argc, char *argv[])
{
    // std::cout << "running" << std::endl;
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }
    std::ifstream file(argv[1], std::ios::binary);
    if (!file)
    {
        std::cerr << "[main] failed to open file" << std::endl;
        return 1;
    }

    std::vector<char> data((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

    while (1) {
        process_going(data);
    }

    return 0;
}


void test_hit() {
    raise(SIGABRT);
}