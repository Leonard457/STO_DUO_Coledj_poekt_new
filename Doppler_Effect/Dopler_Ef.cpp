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
struct RGB {
    Uint8 R;
    Uint8 G;
    Uint8 B;
};

struct FLine {
    SDL_FPoint start;
    SDL_FPoint end;
};

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;

// Глобальные переменные для текстуры Свезды и Ракеты
static SDL_Texture* star_texture = nullptr;
static SDL_FRect star_rect = { 0.0f, 0.0f, 0.0f, 0.0f };

static SDL_Texture* raketa_texture = nullptr;
static SDL_FRect raketa_rect = { 0.0f, 0.0f, 0.0f, 0.0f };

static FLine LightToTarget;
float lightToTargetAngle = 0;
const float DeviationAngle = 15.0f; //константный угол откланиния

static float center_x = 0, center_y = 0;
bool Prodaction = false;

float delta_y = 3.14; // движение по вертикали

//Физические переменные
float Beta = 0.0f; // (V / C)
float Y_factor; // лоренц фактор
float D_factor;

float Frequency;
float WaveCycle; 




void DrawLabel(SDL_Renderer* rend, float x, float y, const std::string& text, Uint8 r = 0, Uint8 g = 0, Uint8 b = 0) {
    SDL_SetRenderDrawColor(rend, r, g, b, 255);
    SDL_RenderDebugText(rend, x, y, text.c_str());
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


     // ЗАГРУЗКА КАРТИНКИ Звезды
    star_texture = IMG_LoadTexture(renderer, "resource/star.png");
    if (!star_texture) {
        SDL_Log("Ошибка загрузки Earth.png: %s", SDL_GetError());
    } else {
        float img_w = 0.0f, img_h = 0.0f;
        SDL_GetTextureSize(star_texture, &img_w, &img_h);
        
        // Масштабируем и выравниваем прямоугольник строго по центру экрана
        star_rect.w = img_w / 4;
        star_rect.h = img_h / 4;
        star_rect.x = center_x - (star_rect.w / 2) - 100;
        star_rect.y = center_y - (star_rect.h / 2);
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
        raketa_rect.x = center_x - (raketa_rect.w / 2) + 250;
        raketa_rect.y = center_y - (raketa_rect.h / 2);
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
                case SDL_SCANCODE_LEFT:
                    Beta -= 0.01;
                    if (Beta < 0) Beta = 0;
                    break;
                case SDL_SCANCODE_RIGHT:
                    Beta += 0.01;
                    if (Beta > 0.99) Beta = 0.99;
                    break;
                case SDL_SCANCODE_UP:
                    raketa_rect.y -= delta_y; // движение вверх 
                    if(raketa_rect.y < 0) raketa_rect.y = 0;
                    break;
                case SDL_SCANCODE_DOWN: //движение вниз
                    raketa_rect.y += delta_y;
                    if(raketa_rect.y > center_y * 2 - raketa_rect.h) raketa_rect.y = center_y * 2 - raketa_rect.h;
                    break;
            }
    }
    return SDL_APP_CONTINUE;
}


#include <cmath>
#include <algorithm>

RGB WavelengthToRGB(float wavelength) {
    // ЕСЛИ ВОЛНА ВЫШЛА ЗА ПРЕДЕЛЫ ВИДИМОСТИ ГЛАЗА — ВКЛЮЧАЕМ ЧИСТЫЙ БЕЛЫЙ ЦВЕТ
    if (wavelength < 380.0f || wavelength > 780.0f) {
        return RGB{ 255, 255, 255 };
    }

    // 1. Аналитическая аппроксимация кривой X (CIE 1931)
    float x1 = (wavelength - 442.0f) * ((wavelength < 442.0f) ? 0.0150f : 0.0118f);
    float x2 = (wavelength - 599.0f) * ((wavelength < 599.0f) ? 0.0264f : 0.0254f);
    float X = 0.362f * std::exp(-0.5f * x1 * x1) + 1.056f * std::exp(-0.5f * x2 * x2) 
            - 0.065f * std::exp(-0.5f * (wavelength - 501.1f) * (wavelength - 501.1f) * 0.0388f * 0.0388f);

    // 2. Аналитическая аппроксимация кривой Y (Яркость)
    float y1 = (wavelength - 568.8f) * ((wavelength < 568.8f) ? 0.0164f : 0.0128f);
    float Y = 0.821f * std::exp(-0.5f * y1 * y1) + 0.286f * std::exp(-0.5f * (wavelength - 530.9f) * (wavelength - 530.9f) * 0.0169f * 0.0169f);

    // 3. Аналитическая аппроксимация кривой Z (Синий)
    float z1 = (wavelength - 437.0f) * ((wavelength < 437.0f) ? 0.0323f : 0.0147f);
    float Z = 1.217f * std::exp(-0.5f * z1 * z1) + 0.681f * std::exp(-0.5f * (wavelength - 459.0f) * (wavelength - 459.0f) * 0.0385f * 0.0385f);

    X = std::max(0.0f, X);
    Y = std::max(0.0f, Y);
    Z = std::max(0.0f, Z);

    // 4. Линейный перевод из пространства XYZ в sRGB
    float r =  3.2404542f * X - 1.5371385f * Y - 0.4985314f * Z;
    float g = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
    float b =  0.0556434f * X - 0.2040259f * Y + 1.0572252f * Z;

    // 5. Стандартная нелинейная гамма-коррекция монитора (гамма ~2.2)
    auto gamma_correct = [](float val) -> float {
        if (val <= 0.0031308f) return 12.92f * val;
        return 1.055f * std::pow(val, 1.0f / 2.4f) - 0.055f;
    };

    // Нормализуем и зажимаем в интервале [0.0; 1.0]
    r = std::clamp(gamma_correct(r), 0.0f, 1.0f);
    g = std::clamp(gamma_correct(g), 0.0f, 1.0f);
    b = std::clamp(gamma_correct(b), 0.0f, 1.0f);

    // Выводим честный, сочный и бескомпромиссный RGB
    RGB color;
    color.R = (Uint8)(r * 255.0f);
    color.G = (Uint8)(g * 255.0f);
    color.B = (Uint8)(b * 255.0f);

    return color;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // --- 1. ГЕОМЕТРИЯ СЦЕНЫ ---
    LightToTarget =  {{star_rect.x + (star_rect.w / 2), star_rect.y + (star_rect.h / 2)}, 
            {raketa_rect.x + (raketa_rect.w / 2), raketa_rect.y + (raketa_rect.h / 2)}};

    lightToTargetAngle = atan2f(LightToTarget.end.y - LightToTarget.start.y, LightToTarget.end.x - LightToTarget.start.x) * (180.0f / M_PI);

    float dx = LightToTarget.end.x - LightToTarget.start.x;
    float dy = LightToTarget.end.y - LightToTarget.start.y;
    float length = sqrtf(dx * dx + dy * dy);

    float rad_main = lightToTargetAngle * (M_PI / 180.0f);
    float rad_dev = DeviationAngle * (M_PI / 180.0f);


    // --- 2. ФИЗИКА СТО И ЭФФЕКТ ДОПЛЕРА ---
    Y_factor = (float)sto::lorentz_factor(Beta);
    
    // Передаем текущий угол луча для учета изменения проекции скорости
    D_factor = (float)sto::relativistic_doppler_factor(Beta, Y_factor, rad_main);

    // Задаем базовую частоту в пиксельном пространстве
    float base_frequency = 0.08f; 
    float base_wave_cycle = 1.0f / base_frequency;

    // Обновляем глобальные физические переменные
    Frequency = (float)sto::Frequency(D_factor, base_frequency);
    WaveCycle = (float)sto::WaveCycle(base_wave_cycle, D_factor);

    // Пересчет физической длины волны света. 
    // На Земле свет зеленый (532 нм). При Доплере: lambda = lambda_0 / D_factor
    float lambda_0 = 532.0f; 
    float current_lambda = lambda_0 / D_factor;

    // Получаем реальный физический RGB цвет
    RGB wave_color = WavelengthToRGB(current_lambda);


    // --- 3. ОТРИСОВКА ОГРАНИЧИТЕЛЬНЫХ ЛУЧЕЙ (Зеленый конус) ---
    SDL_SetRenderDrawColor(renderer, 39, 146, 39, 255);
    
    float border1_x = LightToTarget.start.x + length * cosf(rad_main + rad_dev);
    float border1_y = LightToTarget.start.y + length * sinf(rad_main + rad_dev);
    
    float border2_x = LightToTarget.start.x + length * cosf(rad_main - rad_dev);
    float border2_y = LightToTarget.start.y + length * sinf(rad_main - rad_dev);
    if(!Prodaction) {
        SDL_RenderLine(renderer, LightToTarget.start.x, LightToTarget.start.y, border1_x, border1_y);
        SDL_RenderLine(renderer, LightToTarget.start.x, LightToTarget.start.y, border2_x, border2_y);
    }

    // --- 4. РЕНДЕРИНГ ФИЗИЧЕСКОЙ СИНУСОИДЫ ---
    SDL_SetRenderDrawColor(renderer, wave_color.R, wave_color.G, wave_color.B, 255);

    float dirX = cosf(rad_main);
    float dirY = sinf(rad_main);
    float perpX = -dirY; 
    float perpY = dirX;

    int steps = 120; 
    float prev_wave_x = LightToTarget.start.x;
    float prev_wave_y = LightToTarget.start.y;

    // СКОРОСТЬ СТРОГО ПО НАШЕЙ БИБЛИОТЕКЕ:
    // Мы берем глобальную переменную Frequency, которая уже посчитана через sto::Frequency.
    // Умножаем её на временной шаг (0.5f), чтобы отрегулировать общую скорость анимации на экране.
    static float phase = 0.0f;
    phase -= Frequency * 0.5f; 

    for (int i = 1; i <= steps; ++i) {
        float t = (float)i / steps; 
        float current_dist = t * length;

        float max_amplitude = current_dist * tanf(rad_dev);

        // И витки, и движение волны теперь завязаны на одну и ту же физическую Frequency
        float sin_val = sinf(current_dist * Frequency + phase);

        float wave_x = LightToTarget.start.x + current_dist * dirX + (sin_val * max_amplitude) * perpX;
        float wave_y = LightToTarget.start.y + current_dist * dirY + (sin_val * max_amplitude) * perpY;

        SDL_RenderLine(renderer, prev_wave_x, prev_wave_y, wave_x, wave_y);

        prev_wave_x = wave_x;
        prev_wave_y = wave_y;
    }

    // --- СПРАВОЧНАЯ ИНФОРМАЦИЯ (HUD) ---
    {
        // Настройки положения и размеров увеличенной панели
        float panel_x = 15.0f;
        float panel_y = 15.0f;
        float panel_w = 280.0f;
        float panel_h = 191.0f; // Увеличена высота для новых физических параметров

        // 1. Рисуем подложку (темное полупрозрачное стекло)
        SDL_FRect hud_rect = { panel_x, panel_y, panel_w, panel_h };
        SDL_SetRenderDrawColor(renderer, 10, 15, 25, 220); // Темно-синий глубокий фон
        SDL_RenderFillRect(renderer, &hud_rect);

        // 2. Рисуем рамку панели
        SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255); // Ярко-синяя граница
        SDL_RenderRect(renderer, &hud_rect);

        // 3. Заголовок панели
        DrawLabel(renderer, panel_x + 65, panel_y + 10, "DOPPLER SIMULATION", 255, 255, 255);
        SDL_SetRenderDrawColor(renderer, 0, 150, 255, 100);
        SDL_RenderLine(renderer, panel_x + 10, panel_y + 25, panel_x + panel_w - 10, panel_y + 25);

        // 4. Вывод данных с выравниванием
        float text_x = panel_x + 15;
        float start_y = panel_y + 35;
        float line_spacing = 16.0f;

        // Строка 1: Относительная скорость (b)
        DrawLabel(renderer, text_x, start_y, "Relative speed (b):", 180, 180, 180);
        DrawLabel(renderer, text_x + 180, start_y, format_double(Beta, 3), 0, 255, 255);

        // Строка 2: Лоренц-фактор (Y)
        DrawLabel(renderer, text_x, start_y + line_spacing, "Lorentz factor (Y):", 180, 180, 180);
        DrawLabel(renderer, text_x + 180, start_y + line_spacing, format_double(Y_factor, 4), 255, 100, 100);

        // Строка 3: Доплер-фактор (D)
        DrawLabel(renderer, text_x, start_y + line_spacing * 2, "Doppler factor (D):", 180, 180, 180);
        DrawLabel(renderer, text_x + 180, start_y + line_spacing * 2, format_double(D_factor, 4), 255, 255, 100);

        // Строка 4: Физическая частота волны (v)
        DrawLabel(renderer, text_x, start_y + line_spacing * 3, "Frequency (v):", 180, 180, 180);
        DrawLabel(renderer, text_x + 180, start_y + line_spacing * 3, format_double(Frequency, 4), 100, 255, 100);

        // Строка 5: Длина волны лазера на экране (L)
        DrawLabel(renderer, text_x, start_y + line_spacing * 4, "WaveCycle (L):", 180, 180, 180);
        DrawLabel(renderer, text_x + 180, start_y + line_spacing * 4, format_double(WaveCycle, 2), 255, 150, 255);

        // Строка 6: Реальная длина волны в нанометрах (для цвета)
        DrawLabel(renderer, text_x, start_y + line_spacing * 5, "Wavelength:", 180, 180, 180);
        DrawLabel(renderer, text_x + 180, start_y + line_spacing * 5, format_double(current_lambda, 1) + " nm", 255, 255, 255);

        // Строка 7: Угол в градусах 
        DrawLabel(renderer, text_x, start_y + line_spacing * 6, "Doppler Angle:", 180, 180, 180);
        DrawLabel(renderer, text_x + 180, start_y + line_spacing * 6, format_double(lightToTargetAngle * -1, 1) + " deg", 0, 190, 255);

        // 5. Разделительная линия для примечаний
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
        SDL_RenderLine(renderer, panel_x + 10, panel_y + panel_h - 38, panel_x + panel_w - 10, panel_y + panel_h - 38);
        
        // 6. Примечания про управление стрелками
        DrawLabel(renderer, text_x - 5, panel_y + panel_h - 32, "<- / -> : Change Speed (Beta)", 150, 150, 150);
        DrawLabel(renderer, text_x - 5, panel_y + panel_h - 18, "UP / DN : Maneuver Rocket (Y)", 150, 150, 150);
    }

    // --- 6. ТЕКСТУРЫ ОБЪЕКТОВ ---
    if (star_texture) {
        SDL_RenderTexture(renderer, star_texture, nullptr, &star_rect);
    } else {
        DrawLabel(renderer, center_x - 100, center_y, "star.png не найдена!", 255, 0, 0);
    }

    if (raketa_texture) {
        // Ракета автоматически «смотрит» по направлению идущего к ней луча
        SDL_RenderTextureRotated(renderer, raketa_texture, nullptr, &raketa_rect, -90.0f, nullptr, SDL_FLIP_NONE);
    } else {
        DrawLabel(renderer, center_x - 100, center_y, "Raketa.png не найдена!", 255, 0, 0);
    }

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}


void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    SDL_Quit();
}