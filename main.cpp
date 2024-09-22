#include <iostream>
#include <cstdlib>
#include <complex>
#include <thread>
#include <mutex>
#include <random>
#include <vector>
#include "ThreadSafeQueue.h"

#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

/*
    Константы
*/

const ssize_t MAX_UDP_SIZE = 65535;
const size_t N_DEFAULT = 10;

/*
    Вспомогательные функции
*/

size_t calculateMaxElements() { //вернет 7 при 64-битной архитектуре
    size_t elementSize = sizeof(std::complex<int16_t>*); // размер одного указателя
    return MAX_UDP_SIZE / 1024 / elementSize; // максимальное количество отправляемых элементов
}

/*
    Основные функции программы
*/

void GetDmaBuff(ThreadSafeQueue<std::complex<int16_t>*>& queue, size_t N, bool& done); //Получение танных с АЦП
void sendDataViaDpa(ThreadSafeQueue<std::complex<int16_t>*>& queue, bool& done, bool& hasError); //Отправка пакетов данных

/*
    Главная функция
*/

int main(int, char**){
    //флаги
    bool done = false;
    bool hasError = false;

    //очередь, которая будет управляться потоками
    ThreadSafeQueue<std::complex<int16_t>*> queue;

    //создание потоков
    std::thread th1(GetDmaBuff, std::ref(queue), N_DEFAULT, std::ref(done));
    std::thread th2(sendDataViaDpa, std::ref(queue), std::ref(done), std::ref(hasError));

    //синхронизация потоков
    th1.join();
    th2.join();

    if (!hasError) {
        std::cout << "Программа завершена успешно и без ошибок за время ее выполнения" << std::endl;
    } else {
        std::cout << "Программа завершена с ошибками" << std::endl;
    }

    return 0;
}

/*
    Реализация объявленных функций
*/

void GetDmaBuff(ThreadSafeQueue<std::complex<int16_t>*>& queue, size_t N, bool& done) {    
    //генерация случайных комплексных чисел int16_t
    int16_t min_val = -32768;
    int16_t max_val = 32767;
    std::uniform_int_distribution<int16_t> dist(min_val, max_val);
    std::random_device rd;
    std::mt19937 gen(rd());

    //опрос АЦП
    std::complex<int16_t>* area;
    for (int i = 0; i < 100; i++) {
        area = new std::complex<int16_t>[N];
        for (int j = 0; j < N; j++) {
            int16_t real = dist(gen);
            int16_t imag = dist(gen);
            area[j] = std::complex<int16_t>(real, imag);
        }
        queue.push(area);
        delete[] area;
    }
    done = true; 
}

void sendDataViaDpa(ThreadSafeQueue<std::complex<int16_t>*>& queue, bool& done, bool& hasError) {
    // создание сокета для udp
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Ошибка создания сокета\n";
        hasError = true;
        return;
        //либо кинуть исключение: throw std::runtime_error("Ошибка создания сокета");
    }
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8081);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    //ограничения UDP позволяют нам отправлять только 7 элементов очереди за раз
    size_t maxCount = calculateMaxElements();
    std::complex<int16_t>** data;
    while(true) {
        //ожидание новыхх данных (чтобы точно успевала заполняться очередь)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //выбираем длину массива под отправляемый блок данных (макс 7 элементов)
        size_t len = queue.size();
        size_t arrLength = (len > maxCount) ? maxCount : len;
        //если очередь не пуста, то отправляем пакет, не превышающий допустимые размеры в соответствии с ограничениями
        if (len != 0) {
            data = new std::complex<int16_t>*[arrLength];
            for (int i = 0; i < arrLength; i++) {
                data[i] = queue.pop();
            }
            ssize_t sentBytes = sendto(sockfd, data, arrLength * sizeof(std::complex<int16_t>*), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
            if (sentBytes == -1) {
                std::cerr << "Ошибка при отправке данных\n";
                hasError = true;
            } else {
                std::cout << "Отправлено " << sentBytes << " байт" << std::endl;
            }
            delete[] data;
        } else if (done) { //иначе отправлять нечего, выходим из цикла
            break;
        }        
    }
    //закрытие сокета
    close(sockfd);
    return;
}