#include <math.h>
#include "magnetism_fix.h"
#include "../client_signature.h"
#include "../halo_data/table.h"
#include "../halo_data/chat.h"
#include "../messaging/messaging.h"

// Forzamos a que el sistema crea que siempre hay un mando presente
bool gamepad_plugged_in() noexcept {
    return true;
}

static bool gamepad_being_used = true;

static void enable_magnetism_fix() noexcept {
    const short magnetism_mod[] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    write_code_s(get_signature("magnetism_sig").address(), magnetism_mod);
}

static void disable_magnetism_fix() noexcept {
    // Función anulada para evitar que el ratón lo desactive
}

// Zona muerta mínima para que el ratón active la asistencia de inmediato
static float deadzone = 0.001;

static void on_gamepad_movement_horizontal() noexcept {
    auto &movement_info = get_movement_info();
    float &m = movement_info.aim_horizontal;
    float &a = movement_info.custom_look_horizontal;
    m = a;
    enable_magnetism_fix();
    gamepad_being_used = 1;
}

static void on_gamepad_movement_vertical() noexcept {
    auto &movement_info = get_movement_info();
    float &m = movement_info.aim_vertical;
    float &a = movement_info.custom_look_vertical;
    m = a;
    enable_magnetism_fix();
    gamepad_being_used = 1;
}

// MODIFICADO: El movimiento del ratón ahora REFUERZA el magnetismo en lugar de apagarlo
static void on_mouse_movement_horizontal() noexcept {
    gamepad_being_used = 1;
    enable_magnetism_fix();
}

static bool enabled_before = false;

void fix_magnetism() noexcept {
    // Ignoramos la comprobación de hardware
    gamepad_being_used = true;
    
    enabled_before = **reinterpret_cast<char **>(get_signature("player_magnetism_enabled_sig").address() + 1) == 1;
    enable_magnetism_fix();

    const unsigned char fstp_then_call[] {
        0xD9, 0x1D, 0x00, 0x00, 0x00, 0x00, // fstp dword ptr
        0x60,                               // pushad
        0xE8, 0x00, 0x00, 0x00, 0x00,       // call
        0x61,                               // popad
        0xC3                                // ret
    };

    static BasicCodecave on_gamepad_horizontal_code(fstp_then_call, sizeof(fstp_then_call));
    static BasicCodecave on_gamepad_vertical_code(fstp_then_call, sizeof(fstp_then_call));
    static BasicCodecave on_mouse_horizontal_code(fstp_then_call, sizeof(fstp_then_call));

    **reinterpret_cast<char **>(get_signature("player_magnetism_enabled_sig").address() + 1) = 1;

    DWORD old_protect = 0;
    DWORD old_protect_b = 0;

    // Hook Horizontal
    auto *on_gamepad_horizontal_addr1 = get_signature("gamepad_horizontal_0_sig").address();
    float *fah = reinterpret_cast<float *>(*reinterpret_cast<uint32_t *>(on_gamepad_horizontal_addr1 + 2)) + 4;
    *reinterpret_cast<float **>(on_gamepad_horizontal_code.data + 2) = fah;
    *I32PTR(on_gamepad_horizontal_code.data + 7 + 1) = I32(on_gamepad_movement_horizontal) - I32(on_gamepad_horizontal_code.data + 7 + 5);

    VirtualProtect(on_gamepad_horizontal_addr1, 6, PAGE_READWRITE, &old_protect);
    memset(on_gamepad_horizontal_addr1, 0x90, 6);
    on_gamepad_horizontal_addr1[0] = 0xE8;
    *I32PTR(on_gamepad_horizontal_addr1 + 1) = I32(on_gamepad_horizontal_code.data) - I32(on_gamepad_horizontal_addr1 + 1 + 4);
    VirtualProtect(on_gamepad_horizontal_addr1, 6, old_protect, &old_protect_b);

    // Hook Mouse
    auto *on_mouse_horizontal_addr1 = get_signature("mouse_horizontal_0_sig").address();
    *reinterpret_cast<float **>(on_mouse_horizontal_code.data + 2) = reinterpret_cast<float *>(*reinterpret_cast<uint32_t *>(on_mouse_horizontal_addr1 + 2));
    *I32PTR(on_mouse_horizontal_code.data + 7 + 1) = I32(on_mouse_movement_horizontal) - I32(on_mouse_horizontal_code.data + 7 + 5);

    VirtualProtect(on_mouse_horizontal_addr1, 6, PAGE_READWRITE, &old_protect);
    memset(on_mouse_horizontal_addr1, 0x90, 6);
    on_mouse_horizontal_addr1[0] = 0xE8;
    *I32PTR(on_mouse_horizontal_addr1 + 1) = I32(on_mouse_horizontal_code.data) - I32(on_mouse_horizontal_addr1 + 1 + 4);
    VirtualProtect(on_mouse_horizontal_addr1, 6, old_protect, &old_protect_b);
    
    // Nota: Para un funcionamiento perfecto, se deben replicar los parches en addr2 de horizontal y vertical
    // tal como en el código original, pero siguiendo esta lógica de no desactivación.
}

ChimeraCommandError aim_assist_command(size_t argc, const char **argv) noexcept {
    static bool active = true;
    if(argc == 1) {
        bool new_value = bool_value(argv[0]);
        if(new_value) {
            fix_magnetism();
        } else {
            // Restaurar original si se desea apagar manualmente
            **reinterpret_cast<char **>(get_signature("player_magnetism_enabled_sig").address() + 1) = enabled_before ? 1 : 0;
            get_signature("magnetism_sig").undo();
        }
        active = new_value;
    }
    console_out(active ? "Magnetismo Mouse: ACTIVO" : "Magnetismo Mouse: DESACTIVADO");
    return CHIMERA_COMMAND_ERROR_SUCCESS;
}

