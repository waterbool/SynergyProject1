﻿#pragma once

#include <iostream>     // Для вывода ошибок
#include <fstream>      // Для логирования
#include <vector>       // Для хранения состояния доски и истории

#include "../Models/Move.h"         // Структура хода
#include "../Models/Project_path.h" // Путь к ресурсам

// Подключение SDL с учётом платформы
#ifdef __APPLE__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#else
    #include <SDL.h>
    #include <SDL_image.h>
#endif

using namespace std;

// Класс Board — управляет графикой, состоянием доски и логикой визуализации
class Board
{
public:
    Board() = default;

    // Конструктор с установкой размеров окна
    Board(const unsigned int W, const unsigned int H) : W(W), H(H) {}

    // Инициализация SDL, создание окна и загрузка всех текстур
    int start_draw()
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1;
        }

        // Если размеры окна не заданы, устанавливаем их автоматически
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desktop display mode");
                return 1;
            }
            W = min(dm.w, dm.h);
            W -= W / 15;
            H = W;
        }

        // Создание окна и рендерера
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (!win)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }

        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!ren)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }

        // Загрузка текстур фона, шашек, дамок и кнопок
        board = IMG_LoadTexture(ren, board_path.c_str());
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());
        back = IMG_LoadTexture(ren, back_path.c_str());
        replay = IMG_LoadTexture(ren, replay_path.c_str());

        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }

        SDL_GetRendererOutputSize(ren, &W, &H);
        make_start_mtx();
        rerender();
        return 0;
    }

    // Сброс состояния доски (перезапуск игры)
    void redraw()
    {
        game_results = -1;
        history_mtx.clear();
        history_beat_series.clear();
        make_start_mtx();
        clear_active();
        clear_highlight();
    }

    // Ход с возможным взятием
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
    }

    // Ход по координатам
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        if (mtx[i2][j2])
            throw runtime_error("final position is not empty, can't move");
        if (!mtx[i][j])
            throw runtime_error("begin position is empty, can't move");

        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2; // Превращение в дамку

        mtx[i2][j2] = mtx[i][j];
        drop_piece(i, j);
        add_history(beat_series);
    }

    // Удаление шашки
    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0;
        rerender();
    }

    // Превращение в дамку
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        if (mtx[i][j] == 0 || mtx[i][j] > 2)
            throw runtime_error("can't turn into queen in this position");
        mtx[i][j] += 2;
        rerender();
    }

    // Получить состояние доски
    vector<vector<POS_T>> get_board() const
    {
        return mtx;
    }

    // Подсветка возможных ходов
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (auto pos : cells)
            is_highlighted_[pos.first][pos.second] = 1;
        rerender();
    }

    // Сброс подсветки
    void clear_highlight()
    {
        for (POS_T i = 0; i < 8; ++i)
            is_highlighted_[i].assign(8, 0);
        rerender();
    }

    // Установка активной (выделенной) клетки
    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x;
        active_y = y;
        rerender();
    }

    // Сброс выделенной клетки
    void clear_active()
    {
        active_x = -1;
        active_y = -1;
        rerender();
    }

    // Проверка на подсветку клетки
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y];
    }

    // Откат последнего (или нескольких) ходов
    void rollback()
    {
        auto beat_series = max(1, *(history_beat_series.rbegin()));
        while (beat_series-- && history_mtx.size() > 1)
        {
            history_mtx.pop_back();
            history_beat_series.pop_back();
        }
        mtx = *(history_mtx.rbegin());
        clear_highlight();
        clear_active();
    }

    // Отображение результата игры
    void show_final(const int res)
    {
        game_results = res;
        rerender();
    }

    // Обработка изменения размера окна
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H);
        rerender();
    }

    // Очистка всех ресурсов SDL
    void quit()
    {
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Деструктор — вызывается при удалении объекта
    ~Board()
    {
        if (win)
            quit();
    }

private:
    // Добавление текущего состояния доски в историю
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx);
        history_beat_series.push_back(beat_series);
    }

    // Начальная расстановка фигур
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0;
                if (i < 3 && (i + j) % 2 == 1)
                    mtx[i][j] = 2; // Чёрные
                if (i > 4 && (i + j) % 2 == 1)
                    mtx[i][j] = 1; // Белые
            }
        }
        add_history();
    }

    // Полная перерисовка поля и всех элементов
    void rerender()
    {
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, board, NULL, NULL);

        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!mtx[i][j]) continue;
                int wpos = W * (j + 1) / 10 + W / 120;
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 };

                SDL_Texture* piece_texture;
                if (mtx[i][j] == 1) piece_texture = w_piece;
                else if (mtx[i][j] == 2) piece_texture = b_piece;
                else if (mtx[i][j] == 3) piece_texture = w_queen;
                else piece_texture = b_queen;

                SDL_RenderCopy(ren, piece_texture, NULL, &rect);
            }
        }

        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);
        const double scale = 2.5;
        SDL_RenderSetScale(ren, scale, scale);
        for (POS_T i = 0; i < 8; ++i)
            for (POS_T j = 0; j < 8; ++j)
                if (is_highlighted_[i][j])
                {
                    SDL_Rect cell{ int(W * (j + 1) / 10 / scale), int(H * (i + 1) / 10 / scale), int(W / 10 / scale), int(H / 10 / scale) };
                    SDL_RenderDrawRect(ren, &cell);
                }

        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
            SDL_Rect active_cell{ int(W * (active_y + 1) / 10 / scale), int(H * (active_x + 1) / 10 / scale), int(W / 10 / scale), int(H / 10 / scale) };
            SDL_RenderDrawRect(ren, &active_cell);
        }
        SDL_RenderSetScale(ren, 1, 1);

        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, back, NULL, &rect_left);
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);

        if (game_results != -1)
        {
            string result_path = draw_path;
            if (game_results == 1) result_path = white_path;
            else if (game_results == 2) result_path = black_path;

            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (!result_texture)
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }
            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            SDL_DestroyTexture(result_texture);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    // Логирование ошибок в файл
    void print_exception(const string& text) {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". " << SDL_GetError() << endl;
        fout.close();
    }

public:
    int W = 0, H = 0; // Размеры окна
    vector<vector<vector<POS_T>>> history_mtx; // История ходов

private:
    SDL_Window *win = nullptr;
    SDL_Renderer *ren = nullptr;
    SDL_Texture *board = nullptr, *w_piece = nullptr, *b_piece = nullptr;
    SDL_Texture *w_queen = nullptr, *b_queen = nullptr, *back = nullptr, *replay = nullptr;

    const string textures_path = project_path + "Textures/";
    const string board_path = textures_path + "board.png";
    const string piece_white_path = textures_path + "piece_white.png";
    const string piece_black_path = textures_path + "piece_black.png";
    const string queen_white_path = textures_path + "queen_white.png";
    const string queen_black_path = textures_path + "queen_black.png";
    const string white_path = textures_path + "white_wins.png";
    const string black_path = textures_path + "black_wins.png";
    const string draw_path = textures_path + "draw.png";
    const string back_path = textures_path + "back.png";
    const string replay_path = textures_path + "replay.png";

    int active_x = -1, active_y = -1;
    int game_results = -1;

    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));
    vector<int> history_beat_series;
};
