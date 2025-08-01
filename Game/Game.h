#pragma once
#include <chrono>       // Для измерения времени
#include <thread>       // Для задержки выполнения

#include "../Models/Project_path.h"  // Путь к папке проекта
#include "Board.h"       // Класс игрового поля
#include "Config.h"      // Работа с настройками из JSON
#include "Hand.h"        // Управление взаимодействием с игроком
#include "Logic.h"       // Логика игры (поиск ходов, проверка победы и т.д.)


class Game
{
public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        // При запуске очищаем лог-файл
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    int play()
    {
        auto start = chrono::steady_clock::now(); // Засекаем начало партии

        if (is_replay)
        {
            logic = Logic(&board, &config); // Сброс логики для новой игры
            config.reload();                // Перезагрузка настроек
            board.redraw();                 // Перерисовка поля
        }
        else
        {
            board.start_draw();            // Инициализация первого отображения
        }
        is_replay = false;

        int turn_num = -1;                       // Счётчик ходов
        bool is_quit = false;                    // Флаг выхода
        const int Max_turns = config("Game", "MaxNumTurns"); // Макс. количество ходов

        while (++turn_num < Max_turns)
        {
            beat_series = 0;                         // Обнуляем счётчик ударов
            logic.find_turns(turn_num % 2);          // Поиск ходов для текущего игрока (0 – белый, 1 – чёрный)
            if (logic.turns.empty())
                break; // Нет ходов — завершение игры

            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + "BotLevel"); // Настройка глубины хода бота

            // Если ходит игрок (а не бот)
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + "Bot"))
            {
                auto resp = player_turn(turn_num % 2); // Выполняем ход игрока

                if (resp == Response::QUIT)
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)
                {
                    // Откат ходов при определённых условиях
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + "Bot") &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
            {
                bot_turn(turn_num % 2); // Ход делает бот
            }
        }

        auto end = chrono::steady_clock::now(); // Засекаем окончание игры

        // Логируем продолжительность всей партии
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        if (is_replay)
            return play(); // Перезапуск
        if (is_quit)
            return 0; // Завершение

        // Определяем результат
        int res = 2;
        if (turn_num == Max_turns)
            res = 0; // Ничья
        else if (turn_num % 2)
            res = 1; // Победа чёрных

        board.show_final(res); // Показываем финал
        auto resp = hand.wait(); // Ждём от игрока действия после окончания

        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play(); // Перезапуск по запросу
        }

        return res;
    }


  private:
      void bot_turn(const bool color)
      {
          auto start = chrono::steady_clock::now(); // Засекаем время хода бота

          auto delay_ms = config("Bot", "BotDelayMS");
          thread th(SDL_Delay, delay_ms); // Задержка — имитация размышления
          auto turns = logic.find_best_turns(color); // Поиск оптимального хода
          th.join(); // Дожидаемся окончания задержки

          bool is_first = true;
          for (auto turn : turns)
          {
              if (!is_first)
              {
                  SDL_Delay(delay_ms); // Задержка между последовательными ударами
              }
              is_first = false;
              beat_series += (turn.xb != -1); // Учёт удара
              board.move_piece(turn, beat_series); // Выполнение хода
          }

          auto end = chrono::steady_clock::now();
          // Логирование времени хода бота
          ofstream fout(project_path + "log.txt", ios_base::app);
          fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
          fout.close();
      }


      // Функция player_turn() — обрабатывает ход игрока (белого или чёрного)
      // Возвращает Response::OK при успешном ходе,
      // либо Response::QUIT / REPLAY / BACK при соответствующем действии игрока.
      Response player_turn(const bool color)
      {
          // Создаём вектор координат фигур, которые можно двигать
          vector<pair<POS_T, POS_T>> cells;
          for (auto turn : logic.turns)
          {
              // Добавляем в список все фигуры, у которых есть допустимые ходы
              cells.emplace_back(turn.x, turn.y);
          }

          // Подсвечиваем на игровом поле эти клетки (доступные фигуры)
          board.highlight_cells(cells);

          // Инициализируем переменные:
          // pos — структура хода (откуда и куда),
          // x и y — координаты активной (выделенной) фигуры
          move_pos pos = { -1, -1, -1, -1 };
          POS_T x = -1, y = -1;


          // Выбор клетки и направления хода
          while (true)
          {
              auto resp = hand.get_cell(); // Получаем выбор игрока
              if (get<0>(resp) != Response::CELL)
                  return get<0>(resp); // Обработка других действий: выход, повтор и т.п.

              pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };
              bool is_correct = false;

              for (auto turn : logic.turns)
              {
                  if (turn.x == cell.first && turn.y == cell.second)
                  {
                      is_correct = true;
                      break;
                  }
                  if (turn == move_pos{ x, y, cell.first, cell.second })
                  {
                      pos = turn;
                      break;
                  }
              }

              if (pos.x != -1)
                  break; // Ход подтверждён

              if (!is_correct)
              {
                  if (x != -1)
                  {
                      board.clear_active();
                      board.clear_highlight();
                      board.highlight_cells(cells);
                  }
                  x = -1;
                  y = -1;
                  continue;
              }

              x = cell.first;
              y = cell.second;
              board.clear_highlight();
              board.set_active(x, y);

              // Подсветка направлений из выбранной клетки
              vector<pair<POS_T, POS_T>> cells2;
              for (auto turn : logic.turns)
              {
                  if (turn.x == x && turn.y == y)
                  {
                      cells2.emplace_back(turn.x2, turn.y2);
                  }
              }
              board.highlight_cells(cells2);
          }

          board.clear_highlight();
          board.clear_active();
          board.move_piece(pos, pos.xb != -1);
          if (pos.xb == -1)
              return Response::OK;

          // Продолжение серии ударов
          beat_series = 1;
          while (true)
          {
              logic.find_turns(pos.x2, pos.y2);
              if (!logic.have_beats)
                  break;

              vector<pair<POS_T, POS_T>> cells;
              for (auto turn : logic.turns)
              {
                  cells.emplace_back(turn.x2, turn.y2);
              }
              board.highlight_cells(cells);
              board.set_active(pos.x2, pos.y2);

              while (true)
              {
                  auto resp = hand.get_cell();
                  if (get<0>(resp) != Response::CELL)
                      return get<0>(resp);

                  pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };
                  bool is_correct = false;

                  for (auto turn : logic.turns)
                  {
                      if (turn.x2 == cell.first && turn.y2 == cell.second)
                      {
                          is_correct = true;
                          pos = turn;
                          break;
                      }
                  }

                  if (!is_correct)
                      continue;

                  board.clear_highlight();
                  board.clear_active();
                  beat_series += 1;
                  board.move_piece(pos, beat_series);
                  break;
              }
          }

          return Response::OK;
      }

private:
    Config config;    // Настройки из settings.json
    Board board;      // Игровое поле
    Hand hand;        // Ввод от игрока
    Logic logic;      // Расчёт ходов
    int beat_series;  // Количество последовательных ударов
    bool is_replay = false; // Флаг повторной игры
};

