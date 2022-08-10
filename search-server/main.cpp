// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь: 271

// Закомитьте изменения и отправьте их в свой репозиторий.
#include <iostream>

using namespace std;

int main() {
    int count_of_three = 0;
    for(int i = 1; i <= 1000; ++i) {
        if((i%10 == 3) || (i%10/10 == 3) || (i/100 == 3) || (i%100/10 == 3)) {
            ++count_of_three;
        }
    }
    cout << "Answer: " << count_of_three << endl;
    return 0;
}