#define SDL_MAIN_USE_CALLBACKS 1
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "..\physics\STO.h" 

// Вспомогательная функция для красивого форматирования чисел
std::string format_double(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

struct GridLine {
    SDL_FPoint start;
    SDL_FPoint end;
    int step;
};

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;

static float center_x = 0, center_y = 0;
static float scale = 50.0f;

static double current_beta = 0.0; 

std::vector<GridLine> x_axis_grid; 
std::vector<GridLine> y_axis_grid; 

void DrawLabel(SDL_Renderer* rend, float x, float y, const std::string& text, Uint8 r = 0, Uint8 g = 0, Uint8 b = 0) {
    SDL_SetRenderDrawColor(rend, r, g, b, 255);
    SDL_RenderDebugText(rend, x, y, text.c_str());
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return SDL_APP_FAILURE;
    if (!SDL_CreateWindowAndRenderer("Minkowski – Interaction (Project Edition)", 840, 640, 0, &window, &renderer))
        return SDL_APP_FAILURE;

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    center_x = (float)w / 2.0f;
    center_y = (float)h / 2.0f;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = -10; i <= 10; ++i) {
        if (i == 0) continue;
        GridLine line;
        line.step = i;
        line.start = { center_x + (i * scale), 0 };
        line.end   = { center_x + (i * scale), (float)h };
        x_axis_grid.push_back(line);
    }

    for (int i = -10; i <= 10; ++i) {
        if (i == 0) continue;
        GridLine line;
        line.step = i;
        line.start = { 0, center_y - (i * scale) };
        line.end   = { (float)w, center_y - (i * scale) };
        y_axis_grid.push_back(line);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;

    if (event->type == SDL_EVENT_KEY_DOWN) {
        switch (event->key.key) { 
            case SDLK_LEFT:
                current_beta -= 0.01; // Сделал шаг чуть меньше для плавности
                if (current_beta <= -0.99) current_beta = -0.99;
                break;
            case SDLK_RIGHT:
                current_beta += 0.01;
                if (current_beta >= 0.99) current_beta = 0.99;
                break;
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    int w, h;
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);

    // Расчет физических параметров для справки
    double gamma = sto::lorentz_factor(current_beta);
    double angle_rad = std::atan(current_beta);
    double angle_deg = angle_rad * (180.0 / 3.14159265358979323846);

    // БАЗОВАЯ СЕТКА
    { 
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        for (const auto& line : x_axis_grid) {
            float shift = line.step; 
            SDL_RenderLine(renderer, line.start.x + shift, line.start.y, 
                                     line.end.x + shift, line.end.y);
        }
        for (const auto& line : y_axis_grid) {
            SDL_RenderLine(renderer, line.start.x, line.start.y, 
                                     line.end.x, line.end.y);
        }
    }

    auto to_screen = [&](double x_prime, double ct_prime, double beta, float& out_sx, float& out_sy) {
        double Y = sto::lorentz_factor(beta);
        double x = sto::old_x(x_prime, ct_prime, beta, Y);
        double ct = sto::old_ct(ct_prime, x_prime, beta, Y);
        out_sx = center_x + (float)(x * scale) + (float)x; 
        out_sy = center_y - (float)(ct * scale); 
    };

    // ВТОРИЧНАЯ СЕТКА (S')
    if (current_beta != 0.0) {
        float sx1, sy1, sx2, sy2;
        for (int i = -15; i <= 15; ++i) {
            if (i == 0) SDL_SetRenderDrawColor(renderer, 0, 100, 255, 255); 
            else SDL_SetRenderDrawColor(renderer, 150, 200, 255, 100);      
            to_screen(i, -20, current_beta, sx1, sy1);
            to_screen(i, 20, current_beta, sx2, sy2);
            SDL_RenderLine(renderer, sx1, sy1, sx2, sy2);
            to_screen(-20, i, current_beta, sx1, sy1);
            to_screen(20, i, current_beta, sx2, sy2);
            SDL_RenderLine(renderer, sx1, sy1, sx2, sy2);
        }
    }
    
    
    
    
    // --- ОТРИСОВКА ПАТТЕРНА КВАДРАТОВ ---
    struct Point { double x, ct; };
    auto draw_filled_quad = [&](const Point pts[4], double beta, SDL_FColor fill_color, Uint8 r, Uint8 g, Uint8 b) {
        SDL_Vertex verts[4];
        float sx, sy;
        for (int i = 0; i < 4; ++i) {
            to_screen(pts[i].x, pts[i].ct, beta, sx, sy);
            verts[i].position.x = sx; verts[i].position.y = sy;
            verts[i].color = fill_color; verts[i].tex_coord = {0,0};
        }
        const int indices[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(renderer, nullptr, verts, 4, indices, 6);
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        for (int i = 0; i < 4; ++i) {
            int next_i = (i + 1) % 4;
            SDL_RenderLine(renderer, verts[i].position.x, verts[i].position.y, verts[next_i].position.x, verts[next_i].position.y);
        }
    };

    SDL_FColor grey_fill = {0.5f, 0.5f, 0.5f, 0.4f};
    SDL_FColor red_fill = {1.0f, 0.0f, 0.0f, 0.4f};
    
    for (int i = -8; i <= 8; ++i) {
        std::pair<int, int> cells[] = {{i, i}, {i, -i}, {i, 0}, {0, i}};
        for (int j = 0; j < ((i == 0) ? 1 : 4); ++j) {
            int u = cells[j].first, v = cells[j].second;
            Point pts[4] = {{u+0.2, v+0.2}, {u+0.8, v+0.2}, {u+0.8, v+0.8}, {u+0.2, v+0.8}};
            draw_filled_quad(pts, 0.0, grey_fill, 100, 100, 100);
            if (current_beta != 0.0) draw_filled_quad(pts, current_beta, red_fill, 255, 0, 0);
        }
    }

    // ГЛАВНЫЕ ОСИ
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderLine(renderer, center_x, 0, center_x, (float)h);
    SDL_RenderLine(renderer, 0, center_y, (float)w, center_y);
    SDL_RenderLine(renderer, (float)w - 15, center_y - 5, (float)w, center_y);
    SDL_RenderLine(renderer, (float)w - 15, center_y + 5, (float)w, center_y);
    SDL_RenderLine(renderer, center_x - 5, 15, center_x, 0);
    SDL_RenderLine(renderer, center_x + 5, 15, center_x, 0);
    DrawLabel(renderer, (float)w - 20, center_y - 20, "X", 0, 0, 0);
    DrawLabel(renderer, center_x + 10, 10, "ct", 0, 0, 0);
    
    // --- СПРАВОЧНАЯ ИНФОРМАЦИЯ (HUD) ---
    // --- ОТРИСОВКА ИНФОРМАЦИОННОЙ ПАНЕЛИ (HUD) ---
    {
        // Настройки положения панели
        float panel_x = 15.0f;
        float panel_y = 15.0f;
        float panel_w = 260.0f;
        float panel_h = 130.0f;

        // 1. Рисуем подложку (темное полупрозрачное стекло)
        SDL_FRect hud_rect = { panel_x, panel_y, panel_w, panel_h };
        SDL_SetRenderDrawColor(renderer, 10, 15, 25, 220); // Темно-синий глубокий фон
        SDL_RenderFillRect(renderer, &hud_rect);

        // 2. Рисуем рамку панели
        SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255); // Ярко-синяя граница
        SDL_RenderRect(renderer, &hud_rect);

        // 3. Заголовок панели
        DrawLabel(renderer, panel_x + 60, panel_y + 10, "SYSTEM PARAMETERS", 255, 255, 255);
        SDL_SetRenderDrawColor(renderer, 0, 150, 255, 100);
        SDL_RenderLine(renderer, panel_x + 10, panel_y + 25, panel_x + panel_w - 10, panel_y + 25);

        // 4. Вывод данных с выравниванием
        float text_x = panel_x + 15;
        float start_y = panel_y + 40;
        float line_spacing = 18.0f;

        // Расчеты (те же самые)
        double gamma = sto::lorentz_factor(current_beta);
        double angle_deg = std::atan(current_beta) * (180.0 / 3.14159265);

        // Красивый вывод строк
        DrawLabel(renderer, text_x, start_y, "Relative speed (b):", 180, 180, 180);
        DrawLabel(renderer, text_x + 160, start_y, format_double(current_beta, 3), 0, 255, 255);

        DrawLabel(renderer, text_x, start_y + line_spacing, "Lorentz factor (Y):", 180, 180, 180);
        DrawLabel(renderer, text_x + 160, start_y + line_spacing, format_double(fabs(gamma), 4), 255, 100, 100);

        DrawLabel(renderer, text_x, start_y + line_spacing * 2, "speed(V)% of light(C):", 180, 180, 180);
        DrawLabel(renderer, text_x + 180, start_y + line_spacing * 2, format_double(fabs((current_beta * sto::C) / sto::C * 100), 1) + "%", 100, 255, 100);

        // Небольшая подсказка внизу панели
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
        SDL_RenderLine(renderer, panel_x + 10, panel_y + panel_h - 25, panel_x + panel_w - 10, panel_y + panel_h - 25);
        DrawLabel(renderer, text_x + 10, panel_y + panel_h - 18, "Press Arrows to change Beta", 120, 120, 120);
    }
    
    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    SDL_Quit();
}