#include <thread>
#include <iostream>
#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

int A;
int flag;

void test_thread() {
    while (flag == 0)
        ;
    printf("%d", A);
}
int main() {
    std::thread t(test_thread);
    usleep(100000);
    A = 1;
    flag = 1;
    t.join();
    return 0;
}