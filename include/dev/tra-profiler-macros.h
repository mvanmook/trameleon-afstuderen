#ifndef DEV_PROFILER_TRACY_H
#define DEV_PROFILER_TRACY_H

#include <tracy/TracyC.h>

#define TRA_PROFILER_FUNC_SET_THREAD_NAME(threadName)      TracyCSetThreadName(threadName);
#define TRA_PROFILER_FUNC_FRAME_BEGIN(varName, frameTitle) TracyCFrameMarkStart(frameTitle);
#define TRA_PROFILER_FUNC_FRAME_END(varName, frameTitle)   TracyCFrameMarkEnd(frameTitle);  
#define TRA_PROFILER_FUNC_TIMER_BEGIN(varName, timerTitle) TracyCZoneNS(varName, timerTitle, 1, 1); 
#define TRA_PROFILER_FUNC_TIMER_END(varName, timerTitle)   TracyCZoneEnd(varName);  

#endif
