#pragma once

#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sto {
    // Используем constexpr double для максимальной точности вычислений
    constexpr double C = 299792458.0; 

    // Расчет беты (B = V/C)
    inline double beta(double V) {  
        return V / C;
    }

    // Расчет лоренц-фактора (Y). Убран лишний параметр V.
    inline double lorentz_factor(double B) { 
        // Защита от превышения скорости света
        if (std::abs(B) >= 1.0) {
            return std::numeric_limits<double>::infinity(); // Или можно кидать исключение throw
        }
        return 1.0 / std::sqrt(1.0 - B * B); 
    }

    // --- Преобразования Лоренца ---

    inline double new_x(double x, double ct, double B, double Y) {
        return Y * (x - B * ct); // x'
    }

    inline double new_ct(double ct, double x, double B, double Y) {
        return Y * (ct - B * x); // ct'
    }

    inline double old_x(double new_x, double new_ct, double B, double Y) {
        return Y * (new_x + B * new_ct); // x
    }

    inline double old_ct(double new_ct, double new_x, double B, double Y) {
        return Y * (new_ct + B * new_x); // ct
    }

    // --- Функции специально для построения диаграммы Минковского ---

    // Расчет масштаба (соотношение длины единичного отрезка движущейся системы к неподвижной)
    inline double scale_factor(double B) {
        if (std::abs(B) >= 1.0) return std::numeric_limits<double>::quiet_NaN();
        return std::sqrt((1.0 + B * B) / (1.0 - B * B));
    }

    // Угол наклона осей x' и ct' к осям x и ct (в радианах)
    // Для перевода в градусы умножьте результат на (180.0 / M_PI)
    inline double axis_angle_rad(double B) {
        return std::atan(B);
    }

    // Квадрат инвариантного интервала S^2
    // Полезно для assert(): interval_squared(x, ct) == interval_squared(new_x, new_ct)
    inline double interval_squared(double x, double ct) {
        return (ct * ct) - (x * x);
    }

    // --- Удобные конвертеры ---

    // Перевод секунд в метры (t -> ct)
    inline double t_to_ct(double t) {
        return C * t;
    }

    // Перевод метров обратно в секунды (ct -> t)
    inline double ct_to_t(double ct) {
        return ct / C;
    }
}