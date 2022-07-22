#include "helloprinter.h"
#include <iostream>

#ifndef MESSAGE
#define MESSAGE "No message specified."
#endif

void printHello()
{
    std::cout << MESSAGE << "\n";
}