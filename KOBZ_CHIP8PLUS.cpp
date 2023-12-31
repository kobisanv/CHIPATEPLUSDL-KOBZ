#define SDL_MAIN_HANDLED
#include <iostream>
#include <cstring>
#include <SDL.h>

using namespace std;

class SDL_T {
    public:
        SDL_Window *window;     // Window
        SDL_Renderer *renderer; // Renderer
};

enum EXTENSION_T {
    CHIP8,
};

struct CONFIG_T {
    uint32_t window_width;      // Width of the SDL window
    uint32_t window_height;     // Height of the SDL window
    uint32_t fg_color;          // Foreground color in RGBA8888 format
    uint32_t bg_color;          // Background color in RGBA8888 format
    uint32_t scale_factor;      // Scaling factor for CHIP-8 pixel size
    uint32_t clock_rate;        // CHIP-8 CPU Clock Rate (in Layman's Terms, number of mHz or speed)
    EXTENSION_T current_ex;
};

enum EMULATOR_STATE_T {
    QUIT,       // State indicating the program should quit
    RUNNING,    // State indicating the emulator is running
    PAUSED,     // State indicating the emulator is paused
};

struct INSTRUCTION_T {
    uint16_t opcode;
    uint16_t NNN;    // 12-bit address
    uint8_t NN;     // 8-bit address
    uint8_t N;     // 4-bit address
    uint8_t X;    // 4-bit register identifier
    uint8_t Y;   // 4-bit register identifier
};

struct CHIP_8 {
    EMULATOR_STATE_T state;     // Current state of the CHIP-8 machine
    uint8_t ram[4096];          // Random Access Memory
    bool display[64 * 32];      // CHIP-8 pixels
    uint16_t stack[12];         // Subroutine stack (12 16-bytes)
    uint16_t *stack_ptr;        // Subroutine stack pointer
    uint8_t V[16];              // Data Registers V0-VF (16 8-bytes)
    uint16_t I;                 // Index Register
    uint16_t PC;                // Program Counter
    uint8_t delay_timer;        // Decrements at 60 hz when > 0
    bool keypad[16];            // Hexadecimal keypad 0x0-0xF
    const char *rom_name;       // Running ROM
    INSTRUCTION_T inst;         // Executing Instruction
};

// Initialize SDL
bool INIT(SDL_T *sdl, const CONFIG_T config) {
    // Initialize SDL subsystems for video, audio, and timer
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        SDL_Log("Could not initialize SDL subsystems! %s\n", SDL_GetError());
        return false; }

    // Create SDL window
    sdl->window = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   config.window_width * config.scale_factor, config.window_height * config.scale_factor, 0);

    if (!sdl->window) {
        SDL_Log("Could not create SDL window %s\n", SDL_GetError());
        return false; }

    // Create SDL renderer for hardware acceleration
    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdl->renderer) {
        SDL_Log("Could not create SDL renderer %s\n", SDL_GetError());
        return false;
    } return true;
}

// Set up default emulator configurations from passed in arguments
bool set_config_from_args(CONFIG_T *config, int argc, const char **argv) {
    // Set default configurations
    *config = (CONFIG_T) {
            .window_width = 64,                 // CHIP-8 X resolution
            .window_height = 32,                // CHIP-8 Y resolution
            .fg_color = 0xFFFFFFFF,             // WHITE
            .bg_color = 0x000000FF,             // BLACK
            .scale_factor = 15,                 // Default resolution will be 1280 X 640
            .clock_rate = 750,                  // .75 mHz -> 750,000 instructions are computed (or emulated in this case) per second
            .current_ex = CHIP8,                // Behaves as CHIP-8 system
    };

    // Override defaults from passed-in arguments
    for (int i = 1; i < argc; i++) {
        (void)argv[i]; // Prevent compiler error from unused variables argc/argv
        if (strncmp(argv[i], "--scale-factor", strlen("--scale-factor")) == 0) {
            i++;
            config->scale_factor = (uint32_t)strtol(argv[i], nullptr, 10);
        }
    } return true;
}

// Initialize CHIP-8 machine
bool init_chip8(CHIP_8 *chip8, const char rom_name[]) {
    const uint32_t entry_point = 0x200; // CHIP-8 ROM loaded to 0x200
    const uint8_t font[] = {
            0xF0, 0x90, 0x90, 0x90, 0xF0,   // 0
            0x20, 0x60, 0x20, 0x20, 0x70,   // 1
            0xF0, 0x10, 0xF0, 0x80, 0xF0,   // 2
            0xF0, 0x10, 0xF0, 0x10, 0xF0,   // 3
            0x90, 0x90, 0xF0, 0x10, 0x10,   // 4
            0xF0, 0x80, 0xF0, 0x10, 0xF0,   // 5
            0xF0, 0x80, 0xF0, 0x90, 0xF0,   // 6
            0xF0, 0x10, 0x20, 0x40, 0x40,   // 7
            0xF0, 0x90, 0xF0, 0x90, 0xF0,   // 8
            0xF0, 0x90, 0xF0, 0x10, 0xF0,   // 9
            0xF0, 0x90, 0xF0, 0x90, 0x90,   // A
            0xE0, 0x90, 0xE0, 0x90, 0xE0,   // B
            0xF0, 0x80, 0x80, 0x80, 0xF0,   // C
            0xE0, 0x90, 0x90, 0x90, 0xE0,   // D
            0xF0, 0x80, 0xF0, 0x80, 0xF0,   // E
            0xF0, 0x80, 0xF0, 0x80, 0x80,   // F
    };

    // Load font into RAM
    memcpy(&chip8->ram[0], font, sizeof(font));

    FILE *rom = fopen(rom_name, "rb");
    if (!rom) {
        SDL_Log("ROM file %s is invalid or does not exist\n", rom_name);
        return false; }

    // Get/check ROM size
    fseek(rom, 0, SEEK_END);
    const long rom_size = ftell(rom);
    const long max_size = sizeof chip8->ram - entry_point;
    rewind(rom);

    if (rom_size > max_size) {
        SDL_Log("Rom file %s is too big! Rom Size: %llu, Max Size Allowed: %llu", rom_name, (long long unsigned)rom_size, (long long unsigned)max_size);
        return false; }

    // Load ROM
    if (fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1) {
        SDL_Log("Could not read ROM file %s into CHIP-8 memory: %s\n", rom_name, strerror(errno));
        fclose(rom);
        return false;
    } fclose(rom);


    // Set CHIP-8 machine defaults
    chip8->state = RUNNING;  // Default machine state to ON
    chip8->PC = entry_point; // Start program counter at ROM's entry point
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0];

    return true;
}

// Clean up SDL resources
void final_cleanup(const SDL_T sdl) {
    SDL_DestroyRenderer(sdl.renderer);   // Destroy SDL renderer
    SDL_DestroyWindow(sdl.window);       // Destroy SDL window
    SDL_Quit();                          // Quit SDL subsystems
}

// Clear screen / SDL window to the background color
void clear_screen(const CONFIG_T config, const SDL_T sdl) {
    // Extract RGBA components from background color

    // Right shift bg_color by 24 positions, then extract the lowest 8 bits and store in r
    const uint8_t r = (config.bg_color >> 24) & 0xFF;
    // Right shift bg_color by 16 positions, then extract the lowest 8 bits and store in g
    const uint8_t g = (config.bg_color >> 16) & 0xFF;
    // Right shift bg_color by 8 positions, then extract the lowest 8 bits and store in b
    const uint8_t b = (config.bg_color >> 8) & 0xFF;
    // Extract the lowest 8 bits of bg_color and store in a
    const uint8_t a = (config.bg_color >> 0) & 0xFF;

    // Set the rendering draw color to the background color
    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    // Clear the renderer
    SDL_RenderClear(sdl.renderer);
}

// Update the screen
void update_screen(const SDL_T sdl, const CONFIG_T config, const CHIP_8 chip8) {
    SDL_Rect rect = {.x = 0, .y = 0, .w = static_cast<int>(config.scale_factor), .h = static_cast<int>(config.scale_factor)};

    // Obtain colors to draw
    const uint8_t fg_r = (config.fg_color >> 24) & 0xFF; // Right-shift by 24 and mask using 255 for red fg
    const uint8_t fg_g = (config.fg_color >> 16) & 0xFF; // Right-shift by 16 and mask using 255 for green fg
    const uint8_t fg_b = (config.fg_color >> 8) & 0xFF;  // Right-shift by 8 and mask using 255 for blue fg
    const uint8_t fg_a = (config.fg_color >> 0) & 0xFF;  // Right-shift by 0 and mask using 255 for alpha fg

    const uint8_t bg_r = (config.bg_color >> 24) & 0xFF; // Right-shift by 24 and mask using 255 for red fg
    const uint8_t bg_g = (config.bg_color >> 16) & 0xFF; // Right-shift by 16 and mask using 255 for green fg
    const uint8_t bg_b = (config.bg_color >> 8) & 0xFF; // Right-shift by 8 and mask using 255 for blue fg
    const uint8_t bg_a = (config.bg_color >> 0) & 0xFF; // Right-shift by 0 and mask using 255 for alpha fg

    // Loop through display pixels, draw rectangle per pixel to SDL window
    for (uint32_t i = 0; i < sizeof chip8.display; i++) {
        // Translate 1D index i value to 2D X/Y coordinates
        // X = i % window width
        // Y = i / window width
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if (chip8.display[i]) {
            // If pixel on, draw foreg color
            SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        } else {
            // If pixel off, draw backg color
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }

    // Present the renderer to the window
    SDL_RenderPresent(sdl.renderer);
}

void update_timers(CHIP_8 *chip8) {
    if (chip8->delay_timer > 0) chip8->delay_timer--;
}

// Handle user input events
void handle_input(CHIP_8 *chip8) {
    SDL_Event event;

    // Poll SDL events
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                // Exit window; End program
                chip8->state = QUIT; // Will exit main emulator loop
                break;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        // Escape key; Exit window & End program
                        chip8->state = QUIT;
                        break;

                    case SDLK_SPACE:
                        // Space bar
                        if (chip8->state == RUNNING) {
                            chip8->state = PAUSED;  // Pause
                            SDL_Log("==== PAUSED ====");
                        } else {
                            chip8->state = RUNNING; // Resume
                        } break;

                        // Map qwerty keys to CHIP8 keypad
                    case SDLK_1: chip8->keypad[0x1] = true; break; // 1 -> 1
                    case SDLK_2: chip8->keypad[0x2] = true; break; // 2 -> 2
                    case SDLK_3: chip8->keypad[0x3] = true; break; // 3 -> 3
                    case SDLK_4: chip8->keypad[0xC] = true; break; // C -> 4

                    case SDLK_q: chip8->keypad[0x4] = true; break; // 4 -> q
                    case SDLK_w: chip8->keypad[0x5] = true; break; // 5 -> w
                    case SDLK_e: chip8->keypad[0x6] = true; break; // 6 -> e
                    case SDLK_r: chip8->keypad[0xD] = true; break; // D -> r

                    case SDLK_a: chip8->keypad[0x7] = true; break; // 7 -> a
                    case SDLK_s: chip8->keypad[0x8] = true; break; // 8 -> s
                    case SDLK_d: chip8->keypad[0x9] = true; break; // 9 -> d
                    case SDLK_f: chip8->keypad[0xE] = true; break; // E -> f

                    case SDLK_z: chip8->keypad[0xA] = true; break; // A -> z
                    case SDLK_x: chip8->keypad[0x0] = true; break; // 0 -> x
                    case SDLK_c: chip8->keypad[0xB] = true; break; // B -> c
                    case SDLK_v: chip8->keypad[0xF] = true; break; // F -> v

                    default: break;
                } break;

            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    // Map qwerty keys to CHIP8 keypad
                    case SDLK_1: chip8->keypad[0x1] = false; break; // 1 -> 1
                    case SDLK_2: chip8->keypad[0x2] = false; break; // 2 -> 2
                    case SDLK_3: chip8->keypad[0x3] = false; break; // 3 -> 3
                    case SDLK_4: chip8->keypad[0xC] = false; break; // C -> 4

                    case SDLK_q: chip8->keypad[0x4] = false; break; // 4 -> q
                    case SDLK_w: chip8->keypad[0x5] = false; break; // 5 -> w
                    case SDLK_e: chip8->keypad[0x6] = false; break; // 6 -> e
                    case SDLK_r: chip8->keypad[0xD] = false; break; // D -> r

                    case SDLK_a: chip8->keypad[0x7] = false; break; // 7 -> a
                    case SDLK_s: chip8->keypad[0x8] = false; break; // 8 -> s
                    case SDLK_d: chip8->keypad[0x9] = false; break; // 9 -> d
                    case SDLK_f: chip8->keypad[0xE] = false; break; // E -> f

                    case SDLK_z: chip8->keypad[0xA] = false; break; // A -> z
                    case SDLK_x: chip8->keypad[0x0] = false; break; // 0 -> x
                    case SDLK_c: chip8->keypad[0xB] = false; break; // B -> c
                    case SDLK_v: chip8->keypad[0xF] = false; break; // F -> v

                    default: break;
                } break;

            default: break;
        }
    }
}

// Emulate CHIP-8 instruction
void emulate_instructions(CHIP_8 *chip8, const CONFIG_T config) {
    bool carry; // Carry flag

    // Fetch the next 16-bit opcode from memory (RAM) by combining the higher 8 bits from the current address with the lower 8 bits from the next address
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC + 1];
    chip8->PC += 2; // Pre-increment program counter for next opcode

    // Extract the lowest 12 bits of opcode and store in NNN (High-Bit)
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
    // Extract the lowest 8 bits of opcode and store in NN (Middle-Bit)
    chip8->inst.NN = chip8->inst.opcode & 0x0FF;
    // Extract the lowest 4 bits of opcode and store in N (Low-Bit)
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    // Right shift opcode by 8 positions, then extract the lowest 4 bits and store in X
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    // Right shift opcode by 4 positions, then extract the lowest 4 bits and store in Y
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;

    uint8_t X_coord;
    uint8_t Y_coord;
    uint8_t orig_X;

    // Emulate opcode
    switch ((chip8->inst.opcode >> 12) & 0x0F) { // Extract 4 LSB and mask with 15
        case 0x00:
            if (chip8->inst.NN == 0xE0) {
                memset(&chip8->display[0], false, sizeof chip8->display);
                SDL_Log("0x00E0: Cleared Screen");
            } else if (chip8->inst.NN == 0xEE) {
                // 0x00EE: Return from subroutine
                // Grab last address from subroutine stack ("pop")
                // so that the next opcode will be obtained from address
                chip8->PC = *--chip8->stack_ptr; // Obtain address from stack and assign to program counter
                // Debug information
                SDL_Log("0x00EE: Return from subroutine. PC set to %04X", chip8->PC);
            } else {
                // Invalid opcode; may be 0xNNN for calling machine code routine for RCA1802
                // Debug information
                SDL_Log("0x00: Invalid opcode. Ignoring instruction.");
            } break;
        case 0x01:
            // 0x1NNN: Jump to address NNN
            SDL_Log("Before jump. PC: %04X", chip8->PC);
            chip8->PC = chip8->inst.NNN; // Set program counter so that the next opcode is from NNN
            // Debug information
            SDL_Log("0x01: Jump to address NNN. PC set to %04X", chip8->PC);
            break;
        case 0x02:
            // 0x2NNN: Call subroutine at NNN
            // Store current address to return to on subroutine stack ("push")
            // and set program counter to subroutine address so that the next opcode is gotten from there
            SDL_Log("Stack pointer before push: %p", (void*)chip8->stack_ptr);
            *chip8->stack_ptr++ = chip8->PC; // Save current program counter on stack; increment stack pointer
            SDL_Log("Stack pointer after push: %p", (void*)chip8->stack_ptr);
            chip8->PC = chip8->inst.NNN; // Extract 12-bit and assign to program counter
            // Debug information
            SDL_Log("0x02: Call subroutine at NNN. PC set to %04X", chip8->PC);
            break;
        case 0x03:
            // 0x3XNN: Check if VX == NN, if so, skip next instruction
            if (chip8->V[chip8->inst.X] == chip8->inst.NN) {
                chip8->PC += 2; // Skips instruction
                // Debug information
                SDL_Log("0x03: Skip next instruction. VX[%X] == NN(%X)", chip8->inst.X, chip8->inst.NN);
            } break;
        case 0x04:
            // 0x4XNN: Check if VX != NN, if so, skip next instruction
            if (chip8->V[chip8->inst.X] != chip8->inst.NN) {
                chip8->PC += 2; // Skips instruction
                // Debug information
                SDL_Log("0x04: Skip next instruction. VX[%X] != NN(%X)", chip8->inst.X, chip8->inst.NN);
            } break;
        case 0x05:
            // 0x5XY0: Check if VX == VY, if so, skip next instruction
            if (chip8->inst.N != 0) {
                SDL_Log("0x05: Invalid opcode. Ignoring instruction.");
                break; }
            if (chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]) {
                chip8->PC += 2;
                // Debug information
                SDL_Log("0x05: Skip next instruction. VX[%X] == VY[%X]", chip8->inst.X, chip8->inst.Y);
            } break;
        case 0x06:
            // 0x6XNN: Set register VX to NN
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            // Debug information
            SDL_Log("0x06: Set VX[%X] to NN(%X)", chip8->inst.X, chip8->inst.NN);
            break;
        case 0x07:
            // 0x7XNN: Set register VX += NN
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            // Debug information
            SDL_Log("0x07: Add NN(%X) to VX[%X]", chip8->inst.NN, chip8->inst.X);
            break;
        case 0x08:
            switch (chip8->inst.N) {
                case 0:
                    // 0x8XY0: Assignment (VX, VY)
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
                    // Debug information
                    SDL_Log("0x08XY0: VX[%X] = VY[%X]", chip8->inst.X, chip8->inst.Y);
                    break;
                case 1:
                    // 0x8XY1: Set register VX |= VY
                    chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
                    if (config.current_ex == CHIP8)
                        chip8->V[0xF] = 0;  // Reset VF to 0
                    break;
                case 2:
                    // 0x8XY2: Set register VX &= VY
                    chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
                    if (config.current_ex == CHIP8)
                        chip8->V[0xF] = 0;  // Reset VF to 0
                    break;
                case 3:
                    // 0x8XY3: Set register VX ^= VY
                    chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
                    if (config.current_ex == CHIP8)
                        chip8->V[0xF] = 0;  // Reset VF to 0
                    break;
                case 4:
                    // 0x08XY4: Bitwise ADD_CARRY (VX, VY) 1 if carry 0 if not
                    carry = ((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255);
                    chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry; // Set last data register to carry
                    // Debug information
                    SDL_Log("0x08XY4: VX[%X] += VY[%X]. Carry: %d", chip8->inst.X, chip8->inst.Y, carry);
                    break;
                case 5:
                    // 0x08XY5: Bitwise SUBTRACT_BORROW (VX - VY) 1 if not borrow 0 if borrow
                    carry = (chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]); // Check for borrow
                    chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = carry; // Set last data register to carry
                    // Debug information
                    SDL_Log("0x08XY5: VX[%X] -= VY[%X]. Borrow: %d", chip8->inst.X, chip8->inst.Y, !carry);
                    break;
                case 6:
                    // 0x08XY6: Set register VX >>= 1, store shifted off bit in carry
                    if (config.current_ex == CHIP8) {
                        carry = chip8->V[chip8->inst.Y] & 1;    // Use VY
                        chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] >> 1; // Set VX = VY result
                    } else {
                        carry = chip8->V[chip8->inst.X] & 1;    // Use VX
                        chip8->V[chip8->inst.X] >>= 1;          // Use VX
                    }
                    chip8->V[0xF] = carry;
                    // Debug information
                    SDL_Log("DEBUG: Executed 0x8%X (SHR V%X, V%X) instruction. Shifted VX >>= 1. VF = %d", chip8->inst.opcode & 0x00FF, chip8->inst.X, chip8->inst.Y, chip8->V[0xF]);
                    break;
                case 7:
                    // 0x08XY7: Bitwise SUBTRACT_BORROW (VY - VX) 1 if not borrow 0 if borrow
                    carry = (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]); // Check for borrow
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
                    chip8->V[0xF] = carry; // Set last data register to carry
                    // Debug information
                    SDL_Log("0x08XY7: VX[%X] = VY[%X] - VX[%X]. Borrow: %d", chip8->inst.X, chip8->inst.Y, chip8->inst.X, !carry);
                    break;
                case 0xE:
                    // 0x08XYE: Set register VX <<= 1, store shifted off bit in carry
                    if (config.current_ex == CHIP8) {
                        carry = (chip8->V[chip8->inst.Y] & 0x80) >> 7;  // Use VY
                        chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] << 1; // Set VX = VY result
                    } else {
                        carry = (chip8->V[chip8->inst.X] & 0x80) >> 7;  // VX
                        chip8->V[chip8->inst.X] <<= 1;                  // Use VX
                    }
                    chip8->V[0xF] = carry;
                    // Debug information
                    SDL_Log("DEBUG: Executed 0x8%X (SHL V%X, V%X) instruction. Shifted VX <<= 1. VF = %d", chip8->inst.opcode & 0x00FF, chip8->inst.X, chip8->inst.Y, chip8->V[0xF]);
                    break;
                default: break; // Invalid opcode
            }
            break;
        case 0x09:
            // 0x09XY0: Check if VX != VY; Skip next instruction if so
            if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y])
                chip8->PC += 2;
            break;
        case 0x0A:
            // 0x0ANNN: Set index register I to NNN
            chip8->I = chip8->inst.NNN;
            // Debug information
            SDL_Log("0x0ANNN: I set to %04X", chip8->I);
            break;
        case 0x0B:
            // 0x0BNNN: Jump to V0 + NNN
            chip8->PC = chip8->V[0] + chip8->inst.NNN; // Set program counter to sum of the first data register and the 12-bit address
            // Debug information
            SDL_Log("0x0BNNN: Jump to V0 + NNN. PC set to %04X", chip8->PC);
            break;
        case 0x0C:
            // 0x0CNNN: Sets register VX = rand() % 256 & NN (Bitwise AND)
            chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN;
            // Debug information
            SDL_Log("0x0CNNN: VX[%X] = rand() %% 256 & NN(%X)", chip8->inst.X, chip8->inst.NN);
            break;
        case 0x0D:
            // 0x0DXYN: Draw N-height sprite at coords X, Y; read from memory location I
            // Screen pixels are XOR'd with sprite bits,
            // VF (Carry Flag) is set if any screen pixels are set off; useful for collision detection
            X_coord = chip8->V[chip8->inst.X] % config.window_width;
            Y_coord = chip8->V[chip8->inst.Y] % config.window_height;
            orig_X = X_coord; // Store original X coordinate

            chip8->V[0xF] = 0; // Initialize carry flag to 0

            // Loop over all N rows of sprite
            for (uint8_t i = 0; i < chip8->inst.N; i++) {
                // Get next byte/row of sprite data
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_coord = orig_X; // Reset X for next row to draw

                // Loop over each individual pixel in the sprite row
                for (int8_t j = 7; j >= 0; j--) {
                    // Get a pointer to the display pixel at the current (X, Y) coordinates
                    bool *pixel = &chip8->display[Y_coord * config.window_width + X_coord];

                    // Extract the j-th bit (pixel) from the sprite data
                    const bool sprite_bit = (sprite_data & (1 << j));

                    // If both the sprite bit and the display pixel are set (both on), set the carry flag (VF) (last data register) to 1
                    if (sprite_bit && *pixel) {
                        chip8->V[0xF] = 1;
                    }

                    // XOR the display pixel with the sprite bit
                    *pixel ^= sprite_bit;

                    // Stop drawing if hit right edge of the screen
                    if (++X_coord >= config.window_width) break;
                }

                // Stop drawing entire sprite if hit bottom edge of the screen
                if (++Y_coord >= config.window_height) break;
            }
            // Debug information
            SDL_Log("0x0DXYN: Draw sprite. VF(Carry): %d", chip8->V[0xF]);
            SDL_Log("Program Counter after 0x0DXYN: 0x%04X", chip8->PC);
            break;
        case 0x0E:
            if (chip8->inst.NN == 0x9E) {
                // 0x0EX9E: Skip next instruction if key in VX pressed
                if (chip8->keypad[chip8->V[chip8->inst.X]]) {
                    chip8->PC += 2;
                    // Debug information
                    SDL_Log("0x0EX9E: Skip next instruction. Key in V[%d] is pressed", chip8->inst.X);
                }
            } else if (chip8->inst.NN == 0xA1) {
                // 0x0EXA1: Skip next instruction if key in VX not pressed
                if (!chip8->keypad[chip8->V[chip8->inst.X]]) {
                    chip8->PC += 2;
                    // Debug information
                    SDL_Log("0x0EXA1: Skip next instruction. Key in V[%d] is not pressed", chip8->inst.X);
                }
            } break;
        case 0x0F:
            switch (chip8->inst.NN) {
                case 0x0A: {
                    // 0xFX0A: VX = get_key(); await until a keypress, and store in VX
                    static bool anykey_pressed = false;
                    static uint8_t key = 0xFF;

                    for (uint8_t i = 0; key == 0xFF && i < sizeof chip8->keypad; i++) {
                        if (chip8->keypad[i]) {
                            chip8->V[chip8->inst.X] = i; // i = key offset for keypad array
                            anykey_pressed = true;
                        }
                    }

                    // If no key pressed, keep getting the current opcode & running instruction
                    if (!anykey_pressed) {
                        chip8->PC -= 2;
                        SDL_Log("0xFX0A: Waiting for key press. PC decremented to %04X", chip8->PC);
                    } else {
                        if (chip8->keypad[key]) // Until key is released
                            chip8->PC -= 2;
                        else {
                            chip8->V[chip8->inst.X] = key; // VX = key
                            key = 0xFF;                    // Reset key to "not found"
                            anykey_pressed = false;        // Reset to "Nothing Pressed"
                        }
                    } break;
                }
                case 0x1E:
                    // 0xFX1E: I += VX; Add VX to Register 1
                    chip8->I += chip8->V[chip8->inst.X];
                    SDL_Log("0xFX1E: I += VX; I set to %04X", chip8->I);
                    break;
                case 0x07:
                    // 0xFX07: VX = delay timer
                    chip8->V[chip8->inst.X] = chip8->delay_timer;
                    SDL_Log("0xFX07: VX[%X] set to delay timer value %X", chip8->inst.X, chip8->V[chip8->inst.X]);
                    break;
                case 0x15:
                    // 0xFX15: delay timer = VX
                    chip8->delay_timer = chip8->V[chip8->inst.X];
                    SDL_Log("0xFX15: delay timer set to VX[%X] value %X", chip8->inst.X, chip8->V[chip8->inst.X]);
                    break;
                case 0x29:
                    // 0xFX29: Set register I to sprite location in memory for character in VX (0x0-0xF)
                    chip8->I = chip8->V[chip8->inst.X] * 5;
                    SDL_Log("0xFX29: I set to sprite location in memory for VX[%X]. I set to %04X", chip8->inst.X, chip8->I);
                    break;
                case 0x33: {
                    // 0xFX33: Store binary-coded decimal representation of VX at memory offset from I
                    // I = hundreds, I + 1 = tens, I + 2, ones
                    uint8_t bcd = chip8->V[chip8->inst.X];
                    chip8->ram[chip8->I + 2] = bcd % 10;
                    bcd /= 10;
                    chip8->ram[chip8->I + 1] = bcd % 10;
                    bcd /= 10;
                    chip8->ram[chip8->I] = bcd;
                    break;
                }
                case 0x55:
                    // 0xFX55: Register dump V0-VX inclusive to memory offset from I;
                    // SCHIP does not increment I, CHIP8 does increment I
                    for (uint8_t i = 0; i <= chip8->inst.X; i++) {
                        if (config.current_ex == CHIP8) {
                            chip8->ram[chip8->I++] = chip8->V[i]; // Increment I each time
                        } else {
                            chip8->ram[chip8->I + i] = chip8->V[i];
                        }
                    }
                    // Debug information
                    SDL_Log("DEBUG: Executed 0x55 (LD [I], Vx) instruction. Dumped registers V0-V%X to memory at address I.", chip8->inst.X);
                    break;
                case 0x65:
                    // 0xFX65: Register load V0-VX inclusive from memory offset from I;
                    // SCHIP does not increment I, CHIP8 does increment I
                    for (uint8_t i = 0; i <= chip8->inst.X; i++) {
                        if (config.current_ex == CHIP8) {
                            chip8->V[i] = chip8->ram[chip8->I++]; // Increment I each time
                        } else {
                            chip8->V[i] = chip8->ram[chip8->I + i];
                        }
                    }
                    // Debug information
                    SDL_Log("DEBUG: Executed 0x65 (LD Vx, [I]) instruction. Loaded registers V0-V%X from memory at address I.", chip8->inst.X);
                    break;
            } break;
        default: break; // Invalid opcode
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // Default Usage message for args
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom_name>\n", argv[0]);
        exit(EXIT_FAILURE); }

    // Initialize emulator options
    CONFIG_T config = {0};
    if (!set_config_from_args(&config, argc, (const char **) argv)) exit(EXIT_FAILURE);

    // Initialize SDL
    SDL_T sdl = {nullptr};
    if (!INIT(&sdl, config)) exit(EXIT_FAILURE);

    // Initialize CHIP-8 machine
    CHIP_8 chip8 = {};
    const char *rom_name = argv[1];
    if (!init_chip8(&chip8, rom_name)) exit(EXIT_FAILURE);

    // Initial screen clear
    clear_screen(config, sdl);

    // Main emulator loop
    while (chip8.state != QUIT) {
        // Handle user input
        handle_input(&chip8);

        if (chip8.state == PAUSED) continue;

        // Time before instruction
        const uint64_t before_frame = SDL_GetPerformanceCounter();

        // Emulate instructions
        for (uint32_t i = 0; i < config.clock_rate / 60; i++)
            emulate_instructions(&chip8, config);

        // Time taken to run instruction (Elapsed)
        const uint64_t after_frame = SDL_GetPerformanceCounter();

        // Calculate time elapsed in seconds between two frames and store the result in the variable time_elapsed
        const double time_elapsed = (double)((after_frame - before_frame) / 1000) / SDL_GetPerformanceFrequency();

        if (16.67f > time_elapsed) SDL_Delay(16.67f - time_elapsed);

        // Clear the screen
        clear_screen(config, sdl);

        // Draw the display
        update_screen(sdl, config, chip8);
        update_timers(&chip8);
    }

    // Final cleanup
    final_cleanup(sdl);
    exit(EXIT_SUCCESS);
}
