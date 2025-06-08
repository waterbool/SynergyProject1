// Заголовочный файл класса Config, предназначенного для работы с JSON-настройками проекта
#pragma once
#include <fstream>                      // Для чтения файлов
#include <nlohmann/json.hpp>           // Для работы с JSON
using json = nlohmann::json;

#include "../Models/Project_path.h"    // Путь к рабочей папке проекта

class Config
{
public:
    // Конструктор класса: сразу загружает конфигурацию из файла
    Config()
    {
        reload();
    }

    // Метод reload() читает JSON-файл настроек и сохраняет его содержимое в переменную config
    void reload()
    {
        std::ifstream fin(project_path + "settings.json");
        fin >> config;
        fin.close();
    }

    // Перегруженный оператор (), обеспечивающий удобный доступ к настройкам:
    // cfg("категория", "параметр") → вернёт соответствующее значение
    auto operator()(const string& setting_dir, const string& setting_name) const
    {
        return config[setting_dir][setting_name];
    }

private:
    json config;  // Переменная, хранящая содержимое файла настроек
};
