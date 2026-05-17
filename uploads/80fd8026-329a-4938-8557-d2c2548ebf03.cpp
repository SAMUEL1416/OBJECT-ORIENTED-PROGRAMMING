#include <iostream>
using namespace std;

class Student {
private:
    int marks;

public:
    Student() {
        marks = 0;
    }

    virtual void show() {
        cout << marks;
    }
};

int main() {

    int arr[5] = {1,2,3,4,5};

    cout << arr[2];

    return 0;
}