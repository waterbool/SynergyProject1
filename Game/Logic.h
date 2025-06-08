#pragma once
#include <random>
#include <vector>
#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
public:
    Logic(Board* board, Config* config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine(
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);

        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    // Удалена реализация функции find_best_turns()
    // Находит все возможные ходы (и взятия) для заданного цвета на текущем состоянии доски.
    // color = true — для черных, false — для белых
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }
    // Находит все возможные продолжения взятия для фигуры, стоящей в клетке (x, y)
    // Используется при выполнении серии ударов
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

public:
    vector<move_pos> turns; // Список всех возможных ходов, найденных последним вызовом find_turns()
    bool have_beats; // true, если среди возможных ходов есть взятия (обязательные ходы)
    int Max_depth; // Максимальная глубина поиска в дереве ходов для ИИ (определяет уровень сложности)


private:
    // Удалена реализация функции find_first_best_turn()

    // Удалена реализация функции find_best_turns_rec()
    // Возвращает новое состояние доски после выполнения хода:
    // 1. Если было взятие — удаляет побитую фигуру;
    // 2. Проверяет, достиг ли игрок конца доски (становится дамкой);
    // 3. Перемещает фигуру в новую позицию
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }
    // Оценивает игровую позицию для ИИ (чем меньше значение, тем лучше позиция для first_bot_color).
    // Учитываются количество обычных и дамочных фигур, а также потенциал продвижения.
    // first_bot_color — цвет ИИ, которому нужно оценить позицию (true — черный)
    double calc_score(const vector<vector<POS_T>>& mtx, const bool first_bot_color) const
    {
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);

                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i);
                    b += 0.05 * (mtx[i][j] == 2) * (i);
                }
            }
        }

        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }

        if (w + wq == 0) return INF;
        if (b + bq == 0) return 0;

        int q_coef = (scoring_mode == "NumberAndPotential") ? 5 : 4;

        return (b + bq * q_coef) / (w + wq * q_coef);
    }
    // Вспомогательная функция: ищет ходы в конкретной позиции (mtx), для всех фигур заданного цвета
    void find_turns(const bool color, const vector<vector<POS_T>>& mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;

        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }

        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }
    // Вспомогательная функция: ищет ходы для одной фигуры в заданной клетке (x, y) в конкретной позиции (mtx)
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>>& mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];

        switch (type)
        {
        case 1: case 2:
            for (POS_T i = x - 2; i <= x + 2; i += 4)
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7) continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            break;

        default:
            for (POS_T i = -1; i <= 1; i += 2)
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (xb != -1))
                                break;
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                    }
                }
            break;
        }

        if (!turns.empty())
        {
            have_beats = true;
            return;
        }

        switch (type)
        {
        case 1: case 2:
        {
            POS_T i = ((type % 2) ? x - 1 : x + 1);
            for (POS_T j = y - 1; j <= y + 1; j += 2)
                if (i >= 0 && i <= 7 && j >= 0 && j <= 7 && !mtx[i][j])
                    turns.emplace_back(x, y, i, j);
            break;
        }
        default:
            for (POS_T i = -1; i <= 1; i += 2)
                for (POS_T j = -1; j <= 1; j += 2)
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2]) break;
                        turns.emplace_back(x, y, i2, j2);
                    }
            break;
        }
    }

private:
    default_random_engine rand_eng; // Генератор случайных чисел для перемешивания ходов (чтобы бот не действовал шаблонно)
    string scoring_mode; // Режим оценки позиции: например, "NumberAndPotential" учитывает не только количество, но и потенциальное продвижение
    string optimization; // Уровень или тип оптимизации (пока не используется, но предусмотрено для расширения функционала)

    // Используются для хранения лучшего хода или состояния на следующем уровне в дереве решений (для ИИ)
    vector<move_pos> next_move;
    vector<int> next_best_state;

    Board* board;   // Указатель на объект доски, предоставляет доступ к текущему положению фигур
    Config* config; // Указатель на конфигурацию игры, используется для чтения настроек ИИ, размера окна и т.д.

};
