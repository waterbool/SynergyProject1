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

    // Находит лучший набор ходов с использованием минимакс-алгоритма
    vector<move_pos> find_best_turns(const bool color)
    {
        int depth = Max_depth;
        vector<vector<POS_T>> current = board->get_board();

        vector<vector<POS_T>> best_mtx;
        double best_score = INF;
        vector<move_pos> best_turns;

        find_best_turns_rec(current, color, depth, 0, best_score, best_turns);

        return best_turns;
    }

    // Находит лучший первый ход (используется, когда не нужно искать всю серию)
    move_pos find_first_best_turn(const bool color)
    {
        find_turns(color);
        vector<move_pos> best;
        double best_score = INF;

        for (const auto& turn : turns)
        {
            vector<vector<POS_T>> mtx = make_turn(board->get_board(), turn);
            double score = calc_score(mtx, color);

            if (score < best_score)
            {
                best_score = score;
                best.clear();
                best.push_back(turn);
            }
            else if (score == best_score)
            {
                best.push_back(turn);
            }
        }

        shuffle(best.begin(), best.end(), rand_eng);
        return best[0];
    }

    // Рекурсивный минимакс-алгоритм для выбора лучшей последовательности ходов
    void find_best_turns_rec(
        vector<vector<POS_T>> mtx,
        bool color,
        int depth,
        int beat_series,
        double& best_score,
        vector<move_pos>& best_turns)
    {
        if (depth == 0)
        {
            double score = calc_score(mtx, color);
            if (score < best_score)
            {
                best_score = score;
                best_turns.clear();
            }
            return;
        }

        vector<move_pos> local_turns;
        bool have_beats_local = false;

        find_turns(color, mtx);
        local_turns = turns;
        have_beats_local = have_beats;

        for (const auto& turn : local_turns)
        {
            vector<vector<POS_T>> new_mtx = make_turn(mtx, turn);
            double local_score = INF;
            vector<move_pos> local_sequence;

            find_best_turns_rec(new_mtx, !color, depth - 1, 0, local_score, local_sequence);

            if (local_score < best_score)
            {
                best_score = local_score;
                best_turns.clear();
                best_turns.push_back(turn);
                best_turns.insert(best_turns.end(), local_sequence.begin(), local_sequence.end());
            }
        }
    }

    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

public:
    vector<move_pos> turns; // Список возможных ходов
    bool have_beats;        // Наличие взятий
    int Max_depth;          // Глубина поиска для ИИ

private:
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const;

    double calc_score(const vector<vector<POS_T>>& mtx, const bool first_bot_color) const;

    void find_turns(const bool color, const vector<vector<POS_T>>& mtx);

    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>>& mtx);

private:
    default_random_engine rand_eng; // Генератор случайных чисел
    string scoring_mode;            // Метод оценки позиции
    string optimization;            // Режим оптимизации

    vector<move_pos> next_move;     // Следующий ход для ИИ
    vector<int> next_best_state;    // Состояния для анализа

    Board* board;                   // Указатель на доску
    Config* config;                 // Указатель на настройки
};
