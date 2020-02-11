[![Build Status](https://travis-ci.org/andrey-terekhov/RuC-VM.svg?branch=master)](https://travis-ci.org/andrey-terekhov/RuC-VM)
# ![Logo](https://raw.githubusercontent.com/Victor-Y-Fadeev/RuC-WPF/master/RuC.WPF/Images/Repository.png) Russian C VM

Этот репозиторий содержит виртуальную машину языка [RuC](https://github.com/andrey-terekhov/RuC).
Авторство принадлежит [Андрею Николаевичу Терехову](https://github.com/andrey-terekhov),
заведующему кафедрой "Системного программирования" СПбГУ.

## Сборка

При первоначальном использовании необходимо установить некоторые утилиты:
```
$ sudo apt-get -y install cmake clang-tidy clang-format-9
```

Для конфигурирования и сборки репозитория воспользуйтесь следующими командами:
```
$ mkdir build && cd build && cmake .. && cd ..
$ cmake --build build --config Release
```

Так как в сборке используется CMake, имеется возможность генерации проекта для IDE, например Xcode:
```
cmake . -G Xcode
```
