#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <string.h>

// Configurações da placa DE2i-150
#define DEVICE_FILE "/dev/de2i150_altera"
#define WR_R_DISPLAY 0x00000004
#define WR_RED_LEDS 0x00000005
#define WR_GREEN_LEDS 0x00000006
#define RD_PBUTTONS 0x00000002

// Configurações do jogo
#define WIDTH 4
#define HEIGHT 10
#define NOTE_DELAY 150000
#define MAX_MISSES 3
#define NOTE_SPAWN_RATE 15

// Variáveis globais
int score = 0;
int consecutive_misses = 0;
bool game_active = true;
int dev_fd;
struct termios original_termios;

// Inicialização do terminal
void init_terminal() {
    tcgetattr(STDIN_FILENO, &original_termios);
    struct termios new_termios = original_termios;
    
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 0;
    new_termios.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    
    printf("\033[?25l\033[2J\033[H");
}

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
    printf("\033[?25h\033[2J\033[H");
}

// Controle de hardware
void write_hw(int command, unsigned long value) {
    ioctl(dev_fd, command, value);
}

unsigned long read_hw(int command) {
    unsigned long value = 0;
    ioctl(dev_fd, command, &value);
    return value;
}

// Estrutura do jogo
typedef struct {
    int column;
    int position;
    bool active;
} Note;

Note notes[WIDTH * HEIGHT * 2];
int note_count = 0;

// Geração de notas
void spawn_note() {
    if (note_count < WIDTH * HEIGHT * 2) {
        notes[note_count].column = rand() % WIDTH;
        notes[note_count].position = 0;
        notes[note_count].active = true;
        note_count++;
    }
}

// Atualização do jogo
void update_game() {
    // Movimenta as notas
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active) {
            notes[i].position++;
            if (notes[i].position >= HEIGHT) {
                notes[i].active = false;
                consecutive_misses++;
                if (consecutive_misses >= MAX_MISSES) {
                    game_active = false;
                    write_hw(WR_RED_LEDS, 0xFF);
                }
            }
        }
    }
}

// Renderização do jogo
void render_game() {
    char screen[HEIGHT][WIDTH+1];
    
    // Inicializa a tela
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            screen[y][x] = '.';
        }
        screen[y][WIDTH] = '\0';
    }
    
    // Coloca as notas na tela
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active) {
            int y = notes[i].position;
            int x = notes[i].column;
            if (y >= 0 && y < HEIGHT && x >= 0 && x < WIDTH) {
                screen[y][x] = '1' + x;
            }
        }
    }
    
    // Desenha a tela
    printf("\033[H");
    for (int y = 0; y < HEIGHT; y++) {
        printf("%s\n", screen[y]);
    }
    
    // Linha de base
    for (int x = 0; x < WIDTH; x++) printf("--");
    printf("\n");
    
    printf("Score: %d | Erros: %d/%d\n", score, consecutive_misses, MAX_MISSES);
    
    if (!game_active) {
        printf("\n\033[31mGAME OVER! Pontuacao final: %d\033[0m\n", score);
    }
}

// Verificação de acertos
void check_input() {
    static unsigned long prev_buttons = 0;
    unsigned long buttons = read_hw(RD_PBUTTONS);
    unsigned long changes = buttons ^ prev_buttons;
    
    for (int btn = 0; btn < WIDTH; btn++) {
        if (changes & (1 << btn)) {
            if (buttons & (1 << btn)) {
                // Botão pressionado
                bool hit = false;
                
                for (int i = 0; i < note_count; i++) {
                    if (notes[i].active && notes[i].position == HEIGHT-1 && notes[i].column == btn) {
                        notes[i].active = false;
                        score += 10;
                        consecutive_misses = 0;
                        hit = true;
                        
                        write_hw(WR_GREEN_LEDS, 1 << btn);
                        usleep(50000);
                        write_hw(WR_GREEN_LEDS, 0);
                        break;
                    }
                }
                
                if (!hit) {
                    consecutive_misses++;
                    write_hw(WR_RED_LEDS, 1 << btn);
                    usleep(50000);
                    write_hw(WR_RED_LEDS, 0);
                    
                    if (consecutive_misses >= MAX_MISSES) {
                        game_active = false;
                        write_hw(WR_RED_LEDS, 0xFF);
                    }
                }
                
                write_hw(WR_R_DISPLAY, score);
            }
        }
    }
    
    prev_buttons = buttons;
}

int main() {
    srand(time(NULL));
    init_terminal();
    
    // Inicializa hardware
    dev_fd = open(DEVICE_FILE, O_RDWR);
    if (dev_fd < 0) {
        perror("Falha ao abrir dispositivo");
        restore_terminal();
        return 1;
    }
    
    write_hw(WR_R_DISPLAY, 0);
    write_hw(WR_RED_LEDS, 0);
    write_hw(WR_GREEN_LEDS, 0);
    
    printf("Guitar Hero DE2i-150\n");
    printf("Preparando...\n");
    usleep(1000000);
    
    int frame = 0;
    while (game_active) {
        // Gera novas notas
        if (frame % NOTE_SPAWN_RATE == 0) {
            spawn_note();
        }
        
        update_game();
        render_game();
        check_input();
        
        usleep(NOTE_DELAY);
        frame++;
    }
    
    // Tela de game over
    while (1) {
        render_game();
        usleep(100000);
    }
    
    close(dev_fd);
    restore_terminal();
    return 0;
}
