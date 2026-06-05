/**
 * f_pc_manager.cpp
 * Framework - Process Manager
 */

#include "f_pc/f_pc_manager.h"
#include "SSystem/SComponent/c_API_graphic.h"
#include "SSystem/SComponent/c_lib.h"
#include "Z2AudioLib/Z2SoundMgr.h"
#include "d/d_com_inf_game.h"
#include "d/d_error_msg.h"
#include "d/d_lib.h"
#include "d/d_particle.h"
#include "f_ap/f_ap_game.h"
#include "f_pc/f_pc_creator.h"
#include "f_pc/f_pc_deletor.h"
#include "f_pc/f_pc_draw.h"
#include "f_pc/f_pc_fstcreate_req.h"
#include "f_pc/f_pc_line.h"
#include "f_pc/f_pc_pause.h"
#include "f_pc/f_pc_priority.h"
#include "m_Do/m_Do_controller_pad.h"

#include "tracy/Tracy.hpp"
#include "dusk/logging.h"

#include <cstdlib>
#include <cstring>

namespace {

bool sPortmasterFrameDecisionActive = false;
bool sPortmasterDrawThisFrame = true;

int portmaster_draw_skip_interval() {
    static const int interval = [] {
        const char* value = std::getenv("DUSKLIGHT_PORTMASTER_DRAW_SKIP");
        if (value == nullptr || value[0] == '\0') {
            return 1;
        }

        char* end = nullptr;
        long parsed = std::strtol(value, &end, 10);
        if (end == value || parsed < 1) {
            DuskLog.warn("Ignoring invalid DUSKLIGHT_PORTMASTER_DRAW_SKIP={}", value);
            return 1;
        }

        if (parsed > 8) {
            DuskLog.warn("Clamping DUSKLIGHT_PORTMASTER_DRAW_SKIP={} to 8", parsed);
            parsed = 8;
        }

        DuskLog.info("PortMaster draw skip enabled: drawing every {} logic frame(s)", parsed);
        return static_cast<int>(parsed);
    }();

    return interval;
}

bool portmaster_compute_draw_this_frame() {
    const int interval = portmaster_draw_skip_interval();
    if (interval <= 1) {
        return true;
    }

    static u32 frame = 0;
    const bool draw = (frame % static_cast<u32>(interval)) == 0;
    if (frame < 10 || (frame % 300) == 0) {
        DuskLog.info("PortMaster draw skip: frame={} interval={} draw={}", frame, interval, draw);
    }
    ++frame;
    return draw;
}

}

#if TARGET_PC
bool fpcM_PortmasterBeginFrameDecision() {
    sPortmasterDrawThisFrame = portmaster_compute_draw_this_frame();
    sPortmasterFrameDecisionActive = true;
    return sPortmasterDrawThisFrame;
}

bool fpcM_PortmasterShouldDrawFrame() {
    if (!sPortmasterFrameDecisionActive) {
        return fpcM_PortmasterBeginFrameDecision();
    }
    return sPortmasterDrawThisFrame;
}

void fpcM_PortmasterEndFrameDecision() {
    sPortmasterFrameDecisionActive = false;
}
#endif

void fpcM_Draw(void* i_proc) {
    fpcDw_Execute((base_process_class*)i_proc);
}

int fpcM_DrawIterater(fpcM_DrawIteraterFunc i_drawIterFunc) {
    return fpcLyIt_OnlyHere(fpcLy_RootLayer(), (fpcLyIt_OnlyHereFunc)i_drawIterFunc, NULL);
}

int fpcM_Execute(void* i_proc) {
    return fpcEx_Execute((base_process_class*)i_proc);
}

int fpcM_Delete(void* i_proc) {
    return fpcDt_Delete((base_process_class*)i_proc);
}

BOOL fpcM_IsCreating(fpc_ProcID i_id) {
    return fpcCt_IsCreatingByID(i_id);
}

void fpcM_Management(fpcM_ManagementFunc i_preExecuteFn, fpcM_ManagementFunc i_postExecuteFn) {
    ZoneScoped;
#if TARGET_PC
    const bool drawThisFrame = fpcM_PortmasterShouldDrawFrame();
#else
    const bool drawThisFrame = true;
#endif
    MtxInit();
    if (drawThisFrame && !fapGm_HIO_c::isCaptureScreen()) {
        dComIfGd_peekZdata();
    }
    fapGm_HIO_c::executeCaptureScreen();

    bool shutdownRet = dShutdownErrorMsg_c::execute();
    if (!shutdownRet) {
        static bool l_dvdError = false;

        bool dvdErrRet = dDvdErrorMsg_c::execute();
        if (!dvdErrRet) {
            if (l_dvdError) {
                dLib_time_c::startTime();
                Z2GetSoundMgr()->pauseAllGameSound(false);
                l_dvdError = false;
            }

#ifdef TARGET_PC
            // FRAME INTERP NOTE: Called in m_Do_main when interp is enabled
            if (drawThisFrame && !dusk::frame_interp::is_enabled())
#endif
            {
                cAPIGph_Painter();
            }

            if (!dPa_control_c::isStatus(1)) {
                fpcDt_Handler();
            } else {
                dPa_control_c::offStatus(1);
            }

            if (!fpcPi_Handler()) {
                JUT_ASSERT(353, FALSE);
            }

            if (!fpcCt_Handler()) {
                JUT_ASSERT(357, FALSE);
            }

            if (i_preExecuteFn != NULL) {
                i_preExecuteFn();
            }

            if (!fapGm_HIO_c::isCaptureScreen()) {
                fpcEx_Handler((fpcLnIt_QueueFunc)fpcM_Execute);
            }

            if (drawThisFrame &&
                (!fapGm_HIO_c::isCaptureScreen() || fapGm_HIO_c::getCaptureScreenDivH() != 1)) {
                fpcDw_Handler((fpcDw_HandlerFuncFunc)fpcM_DrawIterater, (fpcDw_HandlerFunc)fpcM_Draw);
            }

            if (i_postExecuteFn != NULL) {
                i_postExecuteFn();
            }

            if (drawThisFrame) {
                dComIfGp_drawSimpleModel();
            }
        } else if (!l_dvdError) {
            dLib_time_c::stopTime();
            Z2GetSoundMgr()->pauseAllGameSound(true);
#if PLATFORM_GCN
#define FPCM_MANAGEMENT_GAMEPAD_COUNT 1
#elif PLATFORM_SHIELD && !DEBUG
#define FPCM_MANAGEMENT_GAMEPAD_COUNT 0
#else
#define FPCM_MANAGEMENT_GAMEPAD_COUNT 4
#endif
            for (u32 i = 0; i < FPCM_MANAGEMENT_GAMEPAD_COUNT; i++) {
                mDoCPd_c::stopMotorWaveHard(i);
            }
            l_dvdError = true;
        }
    }
}

void fpcM_Init() {
    static layer_class rootlayer;
    static node_list_class queue[10];

    fpcLy_Create(&rootlayer, NULL, queue, 10);
    fpcLn_Create();
}

base_process_class* fpcM_FastCreate(s16 i_procname, FastCreateReqFunc i_createReqFunc,
                                    void* i_createData, void* i_append) {
    return fpcFCtRq_Request(fpcLy_CurrentLayer(), i_procname, (fstCreateFunc)i_createReqFunc,
                            i_createData, i_append);
}

int fpcM_IsPause(void* i_proc, u8 i_flag) {
    return fpcPause_IsEnable((base_process_class*)i_proc, i_flag & 0xFF);
}

void fpcM_PauseEnable(void* i_proc, u8 i_flag) {
    fpcPause_Enable((process_node_class*)i_proc, i_flag & 0xFF);
}

void fpcM_PauseDisable(void* i_proc, u8 i_flag) {
    fpcPause_Disable((process_node_class*)i_proc, i_flag & 0xFF);
}

void* fpcM_JudgeInLayer(fpc_ProcID i_layerID, fpcCtIt_JudgeFunc i_judgeFunc, void* i_data) {
    layer_class* layer = fpcLy_Layer(i_layerID);
    if (layer != NULL) {
        void* ret = fpcCtIt_JudgeInLayer(i_layerID, i_judgeFunc, i_data);
        if (ret == NULL) {
            return fpcLyIt_Judge(layer, i_judgeFunc, i_data);
        }
        return ret;
    }

    return NULL;
}
