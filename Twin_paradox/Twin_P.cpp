#define SDL_MAIN_USE_CALLBACKS 1
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h> 
#include "..\physics\STO.h"

// Вспомогательная функция для красивого форматирования чисел
std::string format_double(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

struct FLine {
    SDL_FPoint start;
    SDL_FPoint end;
};

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;

bool start_raketa = false;
float povort = -1;


// Глобальные переменные для текстуры Земли и Ракеты
static SDL_Texture* earth_texture = nullptr;
static SDL_FRect earth_rect = { 0.0f, 0.0f, 0.0f, 0.0f };

static SDL_Texture* raketa_texture = nullptr;
static SDL_FRect raketa_rect = { 0.0f, 0.0f, 0.0f, 0.0f };

float B = 0.0f;
float k_dx = 0.1;

static float center_x = 0, center_y = 0;

float earth_time = 0.0f;
float raketa_time = 0.0f;
float delta_time = 2.0;

//lorenc-factor
float Y = 1.0f;

void DrawLabel(SDL_Renderer* rend, float x, float y, const std::string& text, Uint8 r = 0, Uint8 g = 0, Uint8 b = 0) {
    SDL_SetRenderDrawColor(rend, r, g, b, 255);
    SDL_RenderDebugText(rend, x, y, text.c_str());
}
void DrawRelativityHUD(SDL_Renderer* renderer, float panel_x, float panel_y, float panel_w,
    float panel_h, float beta, double gamma, double earth_time, double rocket_time) {

    SDL_FRect hud_rect = { panel_x, panel_y, panel_w, panel_h };

    // 1) Подложка
    SDL_SetRenderDrawColor(renderer, 10, 15, 25, 220);
    SDL_RenderFillRect(renderer, &hud_rect);

    // 2) Рамка
    SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255);
    SDL_RenderRect(renderer, &hud_rect);

    // 3) Заголовок
    DrawLabel(renderer, panel_x + 28, panel_y + 10, "RELATIVISTIC FLIGHT HUD", 255, 255, 255);

    // Линия под заголовком
    SDL_SetRenderDrawColor(renderer, 0, 150, 255, 100);
    SDL_RenderLine(renderer,
                   panel_x + 10, panel_y + 28,
                   panel_x + panel_w - 10, panel_y + 28);

    // 4) Параметры
    float text_x = panel_x + 12.0f;
    float value_x = panel_x + 165.0f;
    float start_y = panel_y + 40.0f;
    float line_spacing = 18.0f;

    DrawLabel(renderer, text_x, start_y, "Beta (v/c):", 180, 180, 180);
    DrawLabel(renderer, value_x, start_y, format_double(beta, 3), 0, 255, 255);

    DrawLabel(renderer, text_x, start_y + line_spacing, "Lorentz factor:", 180, 180, 180);
    DrawLabel(renderer, value_x, start_y + line_spacing, format_double(gamma, 4), 255, 100, 100);

    DrawLabel(renderer, text_x, start_y + line_spacing * 2, "Earth time:", 180, 180, 180);
    DrawLabel(renderer, value_x, start_y + line_spacing * 2, format_double(earth_time, 2) + " s", 100, 200, 255);

    DrawLabel(renderer, text_x, start_y + line_spacing * 3, "Rocket time:", 180, 180, 180);
    DrawLabel(renderer, value_x, start_y + line_spacing * 3, format_double(rocket_time, 2) + " s", 100, 255, 100);

    DrawLabel(renderer, text_x, start_y + line_spacing * 4, "Time dilation:", 180, 180, 180);
    DrawLabel(renderer, value_x, start_y + line_spacing * 4, format_double(gamma, 2) + "x", 255, 220, 80);

    DrawLabel(renderer, text_x, start_y + line_spacing * 5, "Speed:", 180, 180, 180);
    DrawLabel(renderer, value_x, start_y + line_spacing * 5, format_double(beta * 100.0f, 1) + "% c", 100, 255, 100);

    // 5) Нижняя подсказка
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
    SDL_RenderLine(renderer,
                   panel_x + 10, panel_y + panel_h - 25,
                   panel_x + panel_w - 10, panel_y + panel_h - 25);

    DrawLabel(renderer, panel_x + 12, panel_y + panel_h - 18, "ENTER - launch   ARROWS - beta", 120, 120, 120);
}


SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return SDL_APP_FAILURE;
    if (!SDL_CreateWindowAndRenderer("Twin paradox", 840, 640, 0, &window, &renderer)) return SDL_APP_FAILURE;


    // --- ПРОВЕРКА НОВОЙ БИБЛИОТЕКИ В СТИЛЕ SDL3 ---
    // Так как IMG_Init удалена, мы запрашиваем версию скомпилированной библиотеки
    int version = IMG_Version();
    
    if (version > 0) {
        // Извлекаем мажорную, минорную версии и патч
        int major = version / 1000000;
        int minor = (version / 1000) % 1000;
        int patch = version % 1000;
        
        SDL_Log("Локальная библиотека SDL3_image успешно подключена!");
        SDL_Log("Версия библиотеки на уровень выше: %d.%d.%d", major, minor, patch);
    } else {
        SDL_Log("Ошибка подключения: не удалось получить версию SDL3_image.");
        return SDL_APP_FAILURE;
    }
    // ----------------------------------------------

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    center_x = (float)w / 2.0f;
    center_y = (float)h / 2.0f;

    // ЗАГРУЗКА КАРТИНКИ ЗЕМЛИ
    earth_texture = IMG_LoadTexture(renderer, "resource/Earth.png");
    if (!earth_texture) {
        SDL_Log("Ошибка загрузки Earth.png: %s", SDL_GetError());
    } else {
        float img_w = 0.0f, img_h = 0.0f;
        SDL_GetTextureSize(earth_texture, &img_w, &img_h);
        
        // Масштабируем и выравниваем прямоугольник строго по центру экрана
        earth_rect.w = img_w / 2;
        earth_rect.h = img_h / 2;
        earth_rect.x = center_x - (earth_rect.w / 2.0f) + 300;
        earth_rect.y = center_y - (earth_rect.h / 2.0f) + 150;
    }
    // ЗАГРУЗКА КАРТИНКИ Ракеты
    raketa_texture = IMG_LoadTexture(renderer, "resource/Raketa.png");
    if(!raketa_texture) {
        SDL_Log("Ошибка загрузки Raketa.png: %s", SDL_GetError());
    } else {
        float img_w = 0.0f, img_h = 0.0f;
        SDL_GetTextureSize(raketa_texture, &img_w, &img_h);
        
        // Масштабируем и выравниваем прямоугольник строго по центру экрана
        raketa_rect.w = 100.0f;
        raketa_rect.h = 100.0f;
        raketa_rect.x = center_x + 250;
        raketa_rect.y = center_y - 150;
    }


    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    switch(event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
            break;
        case SDL_EVENT_KEY_DOWN:
            switch(event->key.scancode) {
                case SDL_SCANCODE_RETURN:
                    start_raketa = true;
                    break;
                case SDL_SCANCODE_LEFT:
                    B -= 0.01; // Сделал шаг чуть меньше для плавности
                    if (B < 0) B = 0;
                    break;
                case SDL_SCANCODE_RIGHT:
                    B += 0.01; // Сделал шаг чуть меньше для плавности
                    if (B > 0.99) B = 0.99;
                    break;
            }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    float dx = k_dx + B * 4;
    Y = sto::lorentz_factor(B);

    if((raketa_rect.x <= center_x + 250 + 1) && start_raketa) {
        raketa_rect.x += dx * povort;
        earth_time += delta_time;
        raketa_time += delta_time / Y;
    }

    DrawRelativityHUD(renderer, 25, 25, 260, 180, B, Y, earth_time, raketa_time);

    if(raketa_rect.x - raketa_rect.w/2 <= 0) {
        povort *= povort;
    }
    // 1. РЕНДЕРИНГ КАРТИНКИ ЗЕМЛИ (По центру экрана)
    if (earth_texture) {
        SDL_RenderTexture(renderer, earth_texture, nullptr, &earth_rect);
    } else {
        DrawLabel(renderer, center_x - 100, center_y, "Earth.png не найдена в Twin_paradox/resource/", 255, 0, 0);
    }

    if (raketa_texture) {
        // Передаем nullptr в качестве последнего параметра, чтобы вращение шло строго вокруг её центра.
        SDL_RenderTextureRotated(renderer, raketa_texture, nullptr, &raketa_rect, 90.0f * povort, nullptr, SDL_FLIP_NONE);
        // Поворачиваем ракету на 90.0f градусов по часовой стрелке.
    } else {
        DrawLabel(renderer, center_x - 100, center_y, "Raketa.png не найдена в Twin_paradox/resource/", 255, 0, 0);
    }
    SDL_Delay(16);
    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    SDL_Quit();
}