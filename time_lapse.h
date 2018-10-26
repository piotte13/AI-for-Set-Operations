//
// Created by jeremie on 15/10/18.
//

#ifndef CDATAHUSTLE_TIME_LAPSE_H
#define CDATAHUSTLE_TIME_LAPSE_H


#include <chrono>

#define fw(what) std::forward<decltype(what)>(what)
#define NB_ITERATION 5

/**
* @ class measure
* @ brief Class to measure the execution time of a callable
*/
template <
        typename TimeT = std::chrono::milliseconds, class ClockT = std::chrono::system_clock
>
struct measure
{
    /**
    * @ fn    execution
    * @ brief Returns the quantity (count) of the elapsed time as TimeT units
    */
    template<typename F, typename ...Args>
    static typename TimeT::rep execution(F&& func, Args&&... args)
    {
        auto min = INT64_MAX;
        for (auto i = 0; i < NB_ITERATION; i++){
            auto start = ClockT::now();

            fw(func)(std::forward<Args>(args)...);

            auto duration = std::chrono::duration_cast<TimeT>(ClockT::now() - start).count();
            if(duration < min){
                min = duration;
            }
        }

        return min;
    }

    /**
    * @ fn    duration
    * @ brief Returns the duration (in chrono's type system) of the elapsed time
    */
    template<typename F, typename... Args>
    static TimeT duration(F&& func, Args&&... args)
    {
        auto start = ClockT::now();

        fw(func)(std::forward<Args>(args)...);

        return std::chrono::duration_cast<TimeT>(ClockT::now() - start);
    }
};

#undef NB_ITERATION
#endif //CDATAHUSTLE_TIME_LAPSE_H
