#pragma once
#include <stdlib.h>

typedef int8_t POS_T;

struct move_pos
{
    POS_T x, y;             // начальная позиция (откуда двигаем фигуру)
    POS_T x2, y2;           // конечная позиция (куда перемещаем)
    POS_T xb = -1, yb = -1; // координаты побитой фигуры (если был удар), -1 если нет удара


    // Конструктор для обычного хода (без указания побитой фигуры)
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2) : x(x), y(y), x2(x2), y2(y2)
    {
    }

    // Конструктор для хода с указанием побитой фигуры (удар)
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }
    // Проверка на равенство двух ходов: сравниваются только начальные и конечные координаты
    bool operator==(const move_pos& other) const
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }
    // Проверка на неравенство
    bool operator!=(const move_pos& other) const
    {
        return !(*this == other);
    }
};

