[![Build](https://github.com/andrey-terekhov/RuC-VM/actions/workflows/build.yml/badge.svg)](https://github.com/andrey-terekhov/RuC-VM/actions/workflows/build.yml)
# ![Logo](https://raw.githubusercontent.com/Victor-Y-Fadeev/RuC-WPF/master/RuC.WPF/Images/Repository.png) Russian C VM

Этот репозиторий содержит виртуальную машину языка [RuC](https://github.com/andrey-terekhov/RuC).
Авторство принадлежит [Андрею Николаевичу Терехову](https://github.com/andrey-terekhov),
заведующему кафедрой "Системного программирования" СПбГУ.

## Сборка

При первоначальном использовании необходимо установить некоторые утилиты:
```
$ sudo apt-get -y install git gcc cmake clang-tidy clang-format-9
```

Для конфигурирования и сборки репозитория воспользуйтесь:
```
$ mkdir build && cd build && cmake .. && cd ..
$ cmake --build build --config Release
```

P.s. Если вы собирали Debug версию, не забудьте вернуть `-DCMAKE_BUILD_TYPE=Release`

## Использование

Установить сборку в систему можно одной из следующих команд:
```
$ cmake --install build --config Release
$ cmake --install build --prefix path/to/install --config Release
```

Так как в сборке используется CMake, имеется возможность генерации проекта для IDE, например Xcode:
```
$ cmake . -G Xcode
```
