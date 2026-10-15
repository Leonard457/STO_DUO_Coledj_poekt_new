// ============================================================================
//  PHYSICS SUITE LAUNCHER  (тестовый вариант)
//  Единое меню запуска для трёх программ проекта по физике.
//  Стиль оформления сохранён: неоновая HUD-панель в стиле существующих
//  симуляций — фон (10,15,25,220), рамка (0,150,255), текст через
//  SDL_RenderDebugText (тот же текстовый стиль, что в Doppler/Minkowski/Twin).
//  Справа, в области, не пересекающейся с интерфейсом, — симуляция
//  песочных часов: сине-неоново-фиолетовая палитра, вращение часов,
//  всё на чёрном фоне.
//
//  Выбор программы: мышь (клик по кнопке) или клавиши 1 / 2 / 3, Enter.
//  Запуск — через System() / posix_spawn отдельным процессом (вариант А),
//  поэтому исходный код трёх программ менять не требуется.
// ============================================================================

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#include <spawn.h>
extern char** environ;
#else
#include <process.h>
#endif

// ----------------------------------------------------------------------------
// Палитра проекта (идентична HUD из Dopler_Ef.cpp) + акцент песочных часов
// ----------------------------------------------------------------------------
struct RGB { Uint8 r, g, b; };

static const RGB COL_PANEL_BG   = {  10,  15,  25 }; // фон панели  (alpha 220)
static const RGB COL_NEON_BLUE  = {   0, 150, 255 }; // неоновая рамка / заголовки
static const RGB COL_WHITE      = { 255, 255, 255 };
static const RGB COL_GRAY       = { 150, 150, 150 };
static const RGB COL_CYAN       = {   0, 220, 255 }; // hover-подсветка
static const RGB COL_VIOLET     = { 140,  60, 255 }; // фиолетовый акцент часов
static const RGB COL_MAGENTA    = { 200,  80, 255 };

// ----------------------------------------------------------------------------
// Элементы меню
// ----------------------------------------------------------------------------
struct MenuItem {
    std::string title;      // надпись на кнопке (ASCII — требование DebugText)
    std::string subtitle;   // короткое описание
    std::string exe_name;   // имя исполняемого файла программы
    SDL_FRect   rect;       // зона кнопки (в координатах окна)
    bool        hover = false;
};

static SDL_Window*  window   = nullptr;
static SDL_Renderer* renderer = nullptr;

static std::vector<MenuItem> g_items;
static int   g_selected = 0;          // текущая выбранная строка (клавиатура)
static float g_time     = 0.0f;       // общее время в секундах
static Uint64 g_prev_ts = 0;

static const float WIN_W = 960.0f;
static const float WIN_H = 640.0f;

// ----------------------------------------------------------------------------
// Вспомогательная функция текста — тот же стиль, что DrawLabel в симуляциях
// ----------------------------------------------------------------------------
static void DrawLabel(SDL_Renderer* rend, float x, float y,
                      const std::string& text, RGB c = COL_WHITE) {
    SDL_SetRenderDrawColor(rend, c.r, c.g, c.b, 255);
    SDL_RenderDebugText(rend, x, y, text.c_str());
}

// ----------------------------------------------------------------------------
// Запуск выбранной программы отдельным процессом
// ----------------------------------------------------------------------------
static void LaunchProgram(const std::string& exe_name) {
    // Ищем бинарь рядом с лаунчером, затем в подпапках build-проектов
    std::string candidates[] = {
        "./" + exe_name,
        "../Diogram_Minkovskogo/build/" + exe_name,
        "../Doppler_Effect/build/" + exe_name,
        "../Twin_paradox/build/" + exe_name,
        "./build/" + exe_name,
    };
    std::string path;
    for (const auto& p : candidates) {
        if (access(p.c_str(), 0) == 0) { path = p; break; }
    }
    if (path.empty()) {
        SDL_Log("Launcher: executable '%s' not found", exe_name.c_str());
        return;
    }

#ifdef _WIN32
    std::string cmd = "\"" + path + "\"";
    int rc = system(cmd.c_str());
    (void)rc;
#else
    // Не блокируем лаунчер: posix_spawn + detach
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID); // отцепить от терминала
    char* argv[] = { const_cast<char*>(path.c_str()), nullptr };
    pid_t pid;
    int rc = posix_spawnp(&pid, path.c_str(), nullptr, &attr, argv, environ);
    posix_spawnattr_destroy(&attr);
    if (rc != 0) SDL_Log("Launcher: spawn failed (%d)", rc);
#endif
}

// ----------------------------------------------------------------------------
// Песочные часы (справа, не пересекаются с панелью меню)
//   - корпус рисуется как многоугольник, вращается вокруг центра;
//   - «песок» — поток частиц из верхней колбы в нижнюю, циклический;
//   - палитра: синий -> неон-циан -> фиолетовый.
// ----------------------------------------------------------------------------
struct SandParticle {
    float x, y;     // локальные координаты внутри часов (до вращения)
    float vy;       // скорость падения
    float t;        // жизнь
    bool  falling;  // true — летит по горлышку, false — лежит в нижней колбе
};

static const int HOURS_CX_OFF = 660;   // центр часов по X (правая часть экрана)
static const int HOURS_CY_OFF = 320;   // центр часов по Y
static const float HOURS_SCALE = 1.0f;
static std::vector<SandParticle> g_sand;
static float g_flip_timer = 0.0f;      // периодический переворот часов

static void InitSand() {
    g_sand.clear();
    // Частицы в верхней колбе (пирамида вверх дном): локальные координаты
    // Колбы: верхняя — y от -110 до -10, нижняя — y от 10 до 110.
    for (int i = 0; i < 260; ++i) {
        SandParticle p;
        float u = (float)i / 260.0f;
        float rowY = -105.0f + u * 90.0f;            // заполняем сверху вниз
        float halfW = 6.0f + (rowY + 110.0f) * 0.42f; // чем ниже — тем шире
        p.x = (SDL_randf() * 2.0f - 1.0f) * halfW;
        p.y = rowY;
        p.vy = 0.0f;
        p.t = 0.0f;
        p.falling = false;
        g_sand.push_back(p);
    }
}

// Помощник: точка корпуса часов (локально) -> мировые координаты с вращением
static SDL_FPoint HoursPt(float lx, float ly, float ang) {
    float ca = std::cos(ang), sa = std::sin(ang);
    SDL_FPoint out;
    out.x = (float)HOURS_CX_OFF + (lx * ca - ly * sa) * HOURS_SCALE;
    out.y = (float)HOURS_CY_OFF + (lx * sa + ly * ca) * HOURS_SCALE;
    return out;
}

static void UpdateSand(float dt) {
    // Поток: частицы перетекают из верха в низ. Простейшая модель:
    // верхние частицы «просеиваются» к горлышку и падают.
    static float flow_acc = 0.0f;
    flow_acc += dt * 40.0f;                 // ~40 частиц/сек
    int to_release = (int)flow_acc;
    flow_acc -= to_release;

    for (auto& p : g_sand) {
        if (!p.falling && p.y > -20.0f) {
            // нижний слой верхней колбы уже у горлышка — отпускаем
            if (to_release > 0) { p.falling = true; p.vy = 20.0f; --to_release; }
        }
        if (p.falling) {
            p.vy += 260.0f * dt;            // гравитация внутри часов
            p.y  += p.vy * dt;
            // стягиваем к оси горлышка
            p.x *= (1.0f - 3.0f * dt);
            if (p.y >= 100.0f) {            // осел на дно нижней колбы
                p.falling = false;
                p.y = 100.0f - SDL_randf() * 12.0f;
                float h = 6.0f + (p.y + 110.0f) * 0.42f;
                p.x = (SDL_randf() * 2.0f - 1.0f) * h;
                p.vy = 0.0f;
            }
        } else {
            // в нижней колбе — slight settling
            if (p.y > 0) p.y += 6.0f * dt;
            if (p.y > 100.0f) p.y = 100.0f;
        }
    }

    // Каждые ~8 секунд — переворот часов (сброс песка наверх)
    g_flip_timer += dt;
    if (g_flip_timer > 8.0f) {
        g_flip_timer = 0.0f;
        for (auto& p : g_sand) {
            p.falling = false;
            p.vy = 0.0f;
            float rowY = -105.0f + SDL_randf() * 90.0f;
            float halfW = 6.0f + (rowY + 110.0f) * 0.42f;
            p.x = (SDL_randf() * 2.0f - 1.0f) * halfW;
            p.y = rowY;
        }
    }
}

static void DrawHours(SDL_Renderer* rend) {
    // Медленное постоянное вращение + рывок при перевороте
    float spin = g_time * 0.35f;                       // рад/сек
    float flip = 0.0f;
    if (g_flip_timer > 7.2f) {                          // финальная четверть цикла — переворот
        float k = (g_flip_timer - 7.2f) / 0.8f;         // 0..1
        flip = (float)M_PI * k * k * (3.0f - 2.0f * k); // smoothstep на Пи
    }
    float ang = spin + flip;

    // --- Корпус: две трапеции-колбы + стойки + крышки ---
    // Контур верхней колбы
    SDL_FPoint top[4] = {
        HoursPt(-70, -110, ang), HoursPt(70, -110, ang),
        HoursPt(10,  -10,  ang), HoursPt(-10, -10,  ang),
    };
    SDL_FPoint bot[4] = {
        HoursPt(-10, 10, ang), HoursPt(10, 10, ang),
        HoursPt(70, 110, ang), HoursPt(-70, 110, ang),
    };

    // Контур корпуса рисуется неоновыми линиями — в стиле проекта
    auto poly = [&](SDL_FPoint* pts, int n, RGB c, Uint8 a) {
        SDL_SetRenderDrawColor(rend, c.r, c.g, c.b, a);
        for (int i = 0; i < n; ++i)
            SDL_RenderLine(rend, pts[i].x, pts[i].y,
                           pts[(i + 1) % n].x, pts[(i + 1) % n].y);
    };
    poly(top, 4, COL_NEON_BLUE, 255);
    poly(bot, 4, COL_VIOLET,   255);

    // Крышки (верх/низ) — яркие неоновые полосы
    SDL_FPoint capA[2] = { HoursPt(-80, -118, ang), HoursPt(80, -118, ang) };
    SDL_FPoint capB[2] = { HoursPt(-80,  118, ang), HoursPt(80,  118, ang) };
    SDL_SetRenderDrawColor(rend, COL_CYAN.r, COL_CYAN.g, COL_CYAN.b, 255);
    SDL_RenderLine(rend, capA[0].x, capA[0].y, capA[1].x, capA[1].y);
    SDL_SetRenderDrawColor(rend, COL_MAGENTA.r, COL_MAGENTA.g, COL_MAGENTA.b, 255);
    SDL_RenderLine(rend, capB[0].x, capB[0].y, capB[1].x, capB[1].y);

    // Боковые стойки
    for (float sx : { -80.0f, 80.0f }) {
        SDL_FPoint a = HoursPt(sx, -118, ang);
        SDL_FPoint b = HoursPt(sx,  118, ang);
        SDL_SetRenderDrawColor(rend, 60, 100, 200, 255);
        SDL_RenderLine(rend, a.x, a.y, b.x, b.y);
    }

    // --- Песок: светящиеся точки, цвет зависит от состояния ---
    for (const auto& p : g_sand) {
        SDL_FPoint w = HoursPt(p.x, p.y, ang);
        if (p.falling) {
            SDL_SetRenderDrawColor(rend, COL_CYAN.r, COL_CYAN.g, COL_CYAN.b, 255);
        } else if (p.y < 0) {
            SDL_SetRenderDrawColor(rend, 90, 140, 255, 220);   // верх — синий
        } else {
            SDL_SetRenderDrawColor(rend, COL_VIOLET.r, COL_VIOLET.g, COL_VIOLET.b, 230); // низ — фиолет
        }
        SDL_RenderPoint(rend, w.x, w.y);
        // лёгкое свечение: соседняя точка
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_ADD);
        SDL_SetRenderDrawColor(rend, 40, 60, 160, 60);
        SDL_RenderPoint(rend, w.x + 1, w.y);
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
    }
}

// ----------------------------------------------------------------------------
// Меню (левая панель — фирменный HUD-стиль)
// ----------------------------------------------------------------------------
static void LayoutMenu() {
    g_items.clear();
    const float px = 15.0f, pw = 380.0f;
    struct Def { const char* t; const char* s; const char* e; };
    Def defs[] = {
        { "MINKOWSKI DIAGRAM", "Lorentz transforms / spacetime", "minkowski" },
        { "DOPPLER SIMULATION", "Relativistic Doppler effect",    "doppler"   },
        { "TWIN PARADOX",      "Time dilation experiment",        "twin"      },
    };
    for (int i = 0; i < 3; ++i) {
        MenuItem it;
        it.title = defs[i].t;
        it.subtitle = defs[i].s;
        it.exe_name = defs[i].e;
        it.rect = { px + 20, 150.0f + i * 92.0f, pw - 40, 70.0f };
        g_items.push_back(it);
    }
}

static void DrawMenu(SDL_Renderer* rend) {
    const float panel_x = 15.0f, panel_y = 15.0f, panel_w = 380.0f, panel_h = 420.0f;

    // Фон панели (полупрозрачный тёмно-синий) + неоновая рамка — как в симуляциях
    SDL_FRect hud = { panel_x, panel_y, panel_w, panel_h };
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, COL_PANEL_BG.r, COL_PANEL_BG.g, COL_PANEL_BG.b, 220);
    SDL_RenderFillRect(rend, &hud);
    SDL_SetRenderDrawColor(rend, COL_NEON_BLUE.r, COL_NEON_BLUE.g, COL_NEON_BLUE.b, 255);
    SDL_RenderRect(rend, &hud);
    // Тонкая внутренняя линия-разделитель под заголовком
    SDL_SetRenderDrawColor(rend, COL_NEON_BLUE.r, COL_NEON_BLUE.g, COL_NEON_BLUE.b, 100);
    SDL_RenderLine(rend, panel_x + 10, panel_y + 55, panel_x + panel_w - 10, panel_y + 55);

    // Заголовок (капс, по центру — фирменный стиль)
    DrawLabel(rend, panel_x + 78, panel_y + 15, "PHYSICS PROJECT LAUNCHER", COL_WHITE);
    DrawLabel(rend, panel_x + 120, panel_y + 32, "SELECT A SIMULATION", COL_GRAY);

    // Кнопки программ
    for (size_t i = 0; i < g_items.size(); ++i) {
        MenuItem& it = g_items[i];
        bool sel = ((int)i == g_selected);

        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        if (it.hover || sel) {
            SDL_SetRenderDrawColor(rend, COL_NEON_BLUE.r, COL_NEON_BLUE.g, COL_NEON_BLUE.b, 60);
            SDL_RenderFillRect(rend, &it.rect);
            SDL_SetRenderDrawColor(rend, COL_CYAN.r, COL_CYAN.g, COL_CYAN.b, 255);
        } else {
            SDL_SetRenderDrawColor(rend, 20, 30, 50, 200);
            SDL_RenderFillRect(rend, &it.rect);
            SDL_SetRenderDrawColor(rend, COL_NEON_BLUE.r, COL_NEON_BLUE.g, COL_NEON_BLUE.b, 140);
        }
        SDL_RenderRect(rend, &it.rect);

        DrawLabel(rend, it.rect.x + 14, it.rect.y + 14, "[" + std::to_string(i + 1) + "] " + it.title,
                  (it.hover || sel) ? COL_CYAN : COL_WHITE);
        DrawLabel(rend, it.rect.x + 14, it.rect.y + 38, it.subtitle, COL_GRAY);
    }

    // Подвал с подсказками — как нижняя строка HUD в Doppler
    SDL_SetRenderDrawColor(rend, 255, 255, 255, 50);
    SDL_RenderLine(rend, panel_x + 10, panel_y + panel_h - 60, panel_x + panel_w - 10, panel_y + panel_h - 60);
    DrawLabel(rend, panel_x + 15, panel_y + panel_h - 52, "UP / DN : Select Program", COL_GRAY);
    DrawLabel(rend, panel_x + 15, panel_y + panel_h - 38, "ENTER / CLICK : Launch", COL_GRAY);
    DrawLabel(rend, panel_x + 15, panel_y + panel_h - 24, "ESC : Quit", COL_GRAY);

    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
}

// ----------------------------------------------------------------------------
// Точка входа — обратные вызовы SDL3 (тот же каркас, что в программах проекта)
// ----------------------------------------------------------------------------
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return SDL_APP_FAILURE;
    if (!SDL_CreateWindowAndRenderer("PHYSICS LAUNCHER",
                                     (int)WIN_W, (int)WIN_H, 0, &window, &renderer))
        return SDL_APP_FAILURE;

    LayoutMenu();
    InitSand();
    g_prev_ts = SDL_GetPerformanceCounter();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    // delta-time в секундах
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)((now - g_prev_ts) / (double)SDL_GetPerformanceFrequency());
    if (dt > 0.1f) dt = 0.1f;
    g_prev_ts = now;
    g_time += dt;

    UpdateSand(dt);

    // Чёрный фон всего экрана
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    DrawHours(renderer);   // справа
    DrawMenu(renderer);    // слева — панель меню

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION: {
        float mx = event->motion.x, my = event->motion.y;
        for (size_t i = 0; i < g_items.size(); ++i) {
            SDL_FRect& r = g_items[i].rect;
            bool in = mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
            g_items[i].hover = in;
            if (in) g_selected = (int)i;
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        if (event->button.button != SDL_BUTTON_LEFT) break;
        float mx = event->button.x, my = event->button.y;
        for (auto& it : g_items) {
            SDL_FRect& r = it.rect;
            if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                LaunchProgram(it.exe_name);
                break;
            }
        }
        break;
    }
    case SDL_EVENT_KEY_DOWN: {
        switch (event->key.key) {
        case SDLK_UP:    g_selected = (g_selected + 2) % 3; break;
        case SDLK_DOWN:  g_selected = (g_selected + 1) % 3; break;
        case SDLK_1: case SDLK_KP_1: g_selected = 0; LaunchProgram(g_items[0].exe_name); break;
        case SDLK_2: case SDLK_KP_2: g_selected = 1; LaunchProgram(g_items[1].exe_name); break;
        case SDLK_3: case SDLK_KP_3: g_selected = 2; LaunchProgram(g_items[2].exe_name); break;
        case SDLK_RETURN: case SDLK_RETURN2: case SDLK_KP_ENTER:
            LaunchProgram(g_items[g_selected].exe_name);
            break;
        case SDLK_ESCAPE: return SDL_APP_SUCCESS;
        default: break;
        }
        break;
    }
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    // Окна/рендереры освобождаются автоматически (SDL3 quit behavior)
}
