#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс Hand — отвечает за обработку ввода от пользователя (мышь, выход, кнопки)
class Hand
{
public:
    // Конструктор: получает указатель на игровое поле
    Hand(Board* board) : board(board)
    {
    }

    // Метод get_cell() — возвращает кортеж:
    // (ответ пользователя, координата X, координата Y)
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;  // Начальное состояние
        int x = -1, y = -1;            // Координаты в пикселях
        int xc = -1, yc = -1;          // Преобразованные координаты на поле

        while (true)
        {
            if (SDL_PollEvent(&windowEvent)) // Ожидаем событие от SDL
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT; // Пользователь закрыл окно
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    // Получаем координаты нажатия мыши
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;

                    // Переводим координаты в ячейки поля 8x8
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    // Проверка: нажата кнопка "Назад"
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;
                    }
                    // Проверка: нажата кнопка "Сыграть заново"
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;
                    }
                    // Проверка: клик по игровому полю
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;
                    }
                    // Если клик вне зоны — обнуляем координаты
                    else
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;

                case SDL_WINDOWEVENT:
                    // Обработка изменения размеров окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();
                        break;
                    }
                }

                // Если получен какой-либо ответ — выходим из цикла
                if (resp != Response::OK)
                    break;
            }
        }

        // Возвращаем тип ответа + координаты
        return { resp, xc, yc };
    }


    // Метод wait() — используется после завершения игры для ожидания
    // действия игрока: выйти или сыграть заново
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;

        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT;
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    board->reset_window_size();
                    break;

                case SDL_MOUSEBUTTONDOWN: {
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;

                    // Перевод координат мыши в индексы ячеек
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);

                    // Нажата кнопка "Сыграть заново"
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                                        break;
                }

                if (resp != Response::OK)
                    break;
            }
        }

        return resp;
    }


   private:
    Board* board; // Указатель на игровое поле для взаимодействия (подсветка, размеры и т.д.)
};

