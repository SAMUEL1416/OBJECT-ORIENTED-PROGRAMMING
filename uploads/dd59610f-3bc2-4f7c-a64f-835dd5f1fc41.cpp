#include <iostream>
using namespace std;

int main() {

    int a = 10;

    cout << a / 0;

    int *p;
    cout << *p;

    int arr[5];
    cout << arr[10];

    while(true) {
        cout << "Loop";
    }

    return 0;
}