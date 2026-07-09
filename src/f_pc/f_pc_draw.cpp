/**
 * f_pc_draw.cpp
 * Framework - Process Draw
 */

#include "f_pc/f_pc_draw.h"
#include "SSystem/SComponent/c_API_graphic.h"
#include "d/d_stage.h"
#include "f_pc/f_pc_leaf.h"
#include "f_pc/f_pc_node.h"
#include "f_pc/f_pc_pause.h"
#include "dusk/frame_interpolation.h"
#include "dusk/logging.h"

#include <cstdlib>
#include <cstdio>

namespace {

struct PortmasterProcessDrawStatsEntry {
    s16 profname = -1;
    u32 calls = 0;
    u64 usec = 0;
    u64 exclusiveUsec = 0;
};

static PortmasterProcessDrawStatsEntry s_portmasterProcessDrawStats[256];
static u32 s_portmasterProcessDrawStatsOverflow = 0;
static u32 s_portmasterProcessDrawStatsTotalCalls = 0;
static u64 s_portmasterProcessDrawStatsTotalUsec = 0;
static u64 s_portmasterProcessDrawStatsTotalExclusiveUsec = 0;
static OSTick s_portmasterProcessDrawStatsLastLogTick = 0;
static bool s_portmasterProcessDrawStatsAnnounced = false;

struct PortmasterProcessDrawStackFrame {
    OSTick startTick = 0;
    u64 childUsec = 0;
};

static PortmasterProcessDrawStackFrame s_portmasterProcessDrawStack[64];
static u32 s_portmasterProcessDrawDepth = 0;
static u32 s_portmasterProcessDrawStackOverflow = 0;

static bool portmaster_process_draw_stats_enabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("DUSKLIGHT_PORTMASTER_PROCESS_DRAW_STATS");
        return value != NULL && value[0] != '\0' && value[0] != '0';
    }();
    return enabled;
}

static PortmasterProcessDrawStatsEntry* portmaster_process_draw_stats_entry(s16 profname) {
    PortmasterProcessDrawStatsEntry* empty = NULL;

    for (PortmasterProcessDrawStatsEntry& entry : s_portmasterProcessDrawStats) {
        if (entry.profname == profname) {
            return &entry;
        }
        if (entry.profname < 0 && empty == NULL) {
            empty = &entry;
        }
    }

    if (empty != NULL) {
        empty->profname = profname;
        return empty;
    }

    ++s_portmasterProcessDrawStatsOverflow;
    return NULL;
}

static const char* portmaster_process_draw_name(s16 profname) {
    const char* name = dStage_getName2(profname, 0);
    return name != NULL ? name : "UNKNOWN";
}

static void portmaster_log_process_draw_top(const char* label,
                                            u64 PortmasterProcessDrawStatsEntry::*field) {
    const PortmasterProcessDrawStatsEntry* top[8] = {};

    for (const PortmasterProcessDrawStatsEntry& entry : s_portmasterProcessDrawStats) {
        if (entry.profname < 0 || entry.*field == 0) {
            continue;
        }

        for (int i = 0; i < 8; ++i) {
            if (top[i] == NULL || entry.*field > top[i]->*field) {
                for (int j = 7; j > i; --j) {
                    top[j] = top[j - 1];
                }
                top[i] = &entry;
                break;
            }
        }
    }

    char line[768];
    int used = snprintf(line, sizeof(line), "PortMaster process_draw_top[%s]", label);
    for (const PortmasterProcessDrawStatsEntry* entry : top) {
        if (entry == NULL || used >= (int)sizeof(line)) {
            break;
        }
        used += snprintf(line + used, sizeof(line) - used, " %s(%d):%llu",
                         portmaster_process_draw_name(entry->profname), entry->profname,
                         (unsigned long long)(entry->*field));
    }
    DuskLog.info("{}", line);
}

static void portmaster_log_process_draw_top_calls() {
    const PortmasterProcessDrawStatsEntry* top[8] = {};

    for (const PortmasterProcessDrawStatsEntry& entry : s_portmasterProcessDrawStats) {
        if (entry.profname < 0 || entry.calls == 0) {
            continue;
        }

        for (int i = 0; i < 8; ++i) {
            if (top[i] == NULL || entry.calls > top[i]->calls) {
                for (int j = 7; j > i; --j) {
                    top[j] = top[j - 1];
                }
                top[i] = &entry;
                break;
            }
        }
    }

    char line[768];
    int used = snprintf(line, sizeof(line), "PortMaster process_draw_top[calls]");
    for (const PortmasterProcessDrawStatsEntry* entry : top) {
        if (entry == NULL || used >= (int)sizeof(line)) {
            break;
        }
        used += snprintf(line + used, sizeof(line) - used, " %s(%d):%u",
                         portmaster_process_draw_name(entry->profname), entry->profname, entry->calls);
    }
    DuskLog.info("{}", line);
}

static void portmaster_note_process_draw(base_process_class* proc, u64 usec, u64 exclusiveUsec) {
    if (!portmaster_process_draw_stats_enabled()) {
        return;
    }

    if (!s_portmasterProcessDrawStatsAnnounced) {
        s_portmasterProcessDrawStatsAnnounced = true;
        DuskLog.info("PortMaster process_draw_stats enabled in fpcDw_Execute");
    }

    ++s_portmasterProcessDrawStatsTotalCalls;
    s_portmasterProcessDrawStatsTotalUsec += usec;
    s_portmasterProcessDrawStatsTotalExclusiveUsec += exclusiveUsec;

    PortmasterProcessDrawStatsEntry* entry = portmaster_process_draw_stats_entry(proc->profname);
    if (entry != NULL) {
        ++entry->calls;
        entry->usec += usec;
        entry->exclusiveUsec += exclusiveUsec;
    }

    const OSTick now = OSGetTick();
    if (s_portmasterProcessDrawStatsLastLogTick != 0 &&
        OSTicksToMicroseconds(now - s_portmasterProcessDrawStatsLastLogTick) < 5000000) {
        return;
    }
    s_portmasterProcessDrawStatsLastLogTick = now;

    DuskLog.info("PortMaster process_draw_stats calls={} usec={} exclusive_usec={} overflow={} stack_overflow={}",
                 s_portmasterProcessDrawStatsTotalCalls,
                 (unsigned long long)s_portmasterProcessDrawStatsTotalUsec,
                 (unsigned long long)s_portmasterProcessDrawStatsTotalExclusiveUsec,
                 s_portmasterProcessDrawStatsOverflow, s_portmasterProcessDrawStackOverflow);
    portmaster_log_process_draw_top("usec", &PortmasterProcessDrawStatsEntry::usec);
    portmaster_log_process_draw_top("exclusive_usec", &PortmasterProcessDrawStatsEntry::exclusiveUsec);
    portmaster_log_process_draw_top_calls();

    for (PortmasterProcessDrawStatsEntry& entry : s_portmasterProcessDrawStats) {
        entry = PortmasterProcessDrawStatsEntry{};
    }
    s_portmasterProcessDrawStatsOverflow = 0;
    s_portmasterProcessDrawStatsTotalCalls = 0;
    s_portmasterProcessDrawStatsTotalUsec = 0;
    s_portmasterProcessDrawStatsTotalExclusiveUsec = 0;
    s_portmasterProcessDrawStackOverflow = 0;
}

}  // namespace

int fpcDw_Execute(base_process_class* i_proc) {
    if (!fpcPause_IsEnable(i_proc, 2)) {
        layer_class* save_layer;
        int ret;
        process_method_func draw_func;
    
        save_layer = fpcLy_CurrentLayer();
        if (fpcBs_Is_JustOfType(g_fpcLf_type, i_proc->subtype)) {
            draw_func = ((leafdraw_method_class*)i_proc->methods)->draw_method;
        } else {
            draw_func = ((nodedraw_method_class*)i_proc->methods)->draw_method;
        }
    
        fpcLy_SetCurrentLayer(i_proc->layer_tag.layer);
        const bool portmaster_stats_enabled = portmaster_process_draw_stats_enabled();
        u32 portmaster_stack_index = 0;
        bool portmaster_stack_active = false;
        if (portmaster_stats_enabled) {
            if (s_portmasterProcessDrawDepth < sizeof(s_portmasterProcessDrawStack) /
                                                    sizeof(s_portmasterProcessDrawStack[0])) {
                portmaster_stack_index = s_portmasterProcessDrawDepth++;
                s_portmasterProcessDrawStack[portmaster_stack_index].startTick = OSGetTick();
                s_portmasterProcessDrawStack[portmaster_stack_index].childUsec = 0;
                portmaster_stack_active = true;
            } else {
                ++s_portmasterProcessDrawStackOverflow;
            }
        }
        ret = draw_func(i_proc);
        if (portmaster_stack_active) {
            const OSTick portmaster_end_tick = OSGetTick();
            PortmasterProcessDrawStackFrame& frame = s_portmasterProcessDrawStack[portmaster_stack_index];
            const u64 usec = (u64)OSTicksToMicroseconds(portmaster_end_tick - frame.startTick);
            const u64 exclusiveUsec = usec > frame.childUsec ? usec - frame.childUsec : 0;

            s_portmasterProcessDrawDepth = portmaster_stack_index;
            if (s_portmasterProcessDrawDepth > 0) {
                s_portmasterProcessDrawStack[s_portmasterProcessDrawDepth - 1].childUsec += usec;
            }

            portmaster_note_process_draw(i_proc, usec, exclusiveUsec);
        }
        fpcLy_SetCurrentLayer(save_layer);
        return ret;
    }

    return 0;
}

int fpcDw_Handler(fpcDw_HandlerFuncFunc i_iterHandler, fpcDw_HandlerFunc i_func) {
    static int sDwLogCount = 0;
    int ret;
    if (sDwLogCount < 5) { DuskLog.debug("fpcDw_Handler: before BeforeOfDraw"); }
    cAPIGph_BeforeOfDraw();
    if (sDwLogCount < 5) { DuskLog.debug("fpcDw_Handler: before draw iteration"); }
    ret = i_iterHandler(i_func);
    if (sDwLogCount < 5) { DuskLog.debug("fpcDw_Handler: before AfterOfDraw"); }
    cAPIGph_AfterOfDraw();
    if (sDwLogCount < 5) { DuskLog.debug("fpcDw_Handler: done"); }
    sDwLogCount++;
    return ret;
}
