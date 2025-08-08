#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <string.h>

// Definições para a placa DE2i-150
#define DEVICE_FILE "/dev/de2i150_altera"
#define WR_R_DISPLAY 0x00000004
#define WR_RED_LEDS 0x00000005
#define WR_GREEN_LEDS 0x00000006

// Configurações do jogo
#define WIDTH 4
#define HEIGHT 10
#define NOTE_DELAY 200000 // 200ms
#define MAX_MISSES 3

// Variáveis globais
int score = 0;
int consecutive_misses = 0;
bool game_over = false;
int dev_fd;
struct termios original_termios;

// Função para configurar o terminal
void init_terminal() {
    struct termios new_termios;
    
    tcgetattr(STDIN_FILENO, &original_termios);
    new_termios = original_termios;
    
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    
    printf("\033[?25l"); // Esconde cursor
    printf("\033[2J");   // Limpa tela
}

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
    printf("\033[?25h"); // Mostra cursor
}

void write_hw(int command, unsigned long value) {
    ioctl(dev_fd, command, value);
}

typedef struct {
    int type; // 1-4
    int y;    // posição vertical
    bool active;
} Note;

void generate_note(Note *notes, int *note_count) {
    if (*note_count < WIDTH * HEIGHT) {
        notes[*note_count].type = rand() % 4 + 1;
        notes[*note_count].y = 0;
        notes[*note_count].active = true;
        (*note_count)++;
    }
}

void update_notes(Note *notes, int note_count) {
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active) {
            notes[i].y++;
            if (notes[i].y >= HEIGHT) {
                notes[i].active = false;
                consecutive_misses++;
                if (consecutive_misses >= MAX_MISSES) {
                    game_over = true;
                    write_hw(WR_RED_LEDS, 0xFF);
                }
            }
        }
    }
}

void draw_screen(Note *notes, int note_count) {
    char screen[HEIGHT][WIDTH];
    memset(screen, '.', sizeof(screen));

    // Preenche as notas ativas
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active) {
            int col = notes[i].type - 1;
            if (col >= 0 && col < WIDTH && notes[i].y < HEIGHT) {
                screen[notes[i].y][col] = '0' + notes[i].type;
            }
        }
    }

    printf("\033[H"); // Move cursor para o início
    
    // Desenha a tela
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            printf("%c ", screen[y][x]);
        }
        printf("\n");
    }
    
    // Linha de base
    for (int i = 0; i < WIDTH; i++) printf("--");
    printf("\n");
    
    printf("Score: %d | Erros: %d/%d\n", score, consecutive_misses, MAX_MISSES);
    if (game_over) printf("GAME OVER! Pressione CTRL+C para sair.\n");
}

void check_hit(int button, Note *notes, int note_count) {
    bool hit = false;
    
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active && notes[i].y == HEIGHT-1 && notes[i].type == button) {
            notes[i].active = false;
            score += 10;
            consecutive_misses = 0;
            hit = true;
            
            write_hw(WR_GREEN_LEDS, 1 << (button-1));
            usleep(100000);
            write_hw(WR_GREEN_LEDS, 0);
            break;
        }
    }
    
    if (!hit && button != 0) {
        consecutive_misses++;
        
        write_hw(WR_RED_LEDS, 1 << (button-1));
        usleep(100000);
        write_hw(WR_RED_LEDS, 0);
        
        if (consecutive_misses >= MAX_MISSES) {
            game_over = true;
            write_hw(WR_RED_LEDS, 0xFF);
        }
    }
    
    write_hw(WR_R_DISPLAY, score);
}

int main() {
    srand(time(NULL));
    init_terminal();
    
    dev_fd = open(DEVICE_FILE, O_RDWR);
    if (dev_fd < 0) {
        perror("Erro ao abrir dispositivo");
        restore_terminal();
        return 1;
    }
    
    write_hw(WR_R_DISPLAY, 0);
    write_hw(WR_RED_LEDS, 0);
    write_hw(WR_GREEN_LEDS, 0);
    
    Note notes[WIDTH * HEIGHT] = {0};
    int note_count = 0;
    int frame = 0;
    
    printf("Jogo Guitar Hero - DE2i-150\n");
    printf("Use os botões 1-4 para jogar\n");
    usleep(1000000); // Pausa para ler as instruções
    
    while (!game_over) {
        if (frame % 10 == 0) {
            generate_note(notes, &note_count);
        }
        
        update_notes(notes, note_count);
        draw_screen(notes, note_count);
        
        char input = 0;
        if (read(STDIN_FILENO, &input, 1) > 0) {
            if (input >= '1' && input <= '4') {
                check_hit(input - '0', notes, note_count);
            }
        }
        
        usleep(NOTE_DELAY);
        frame++;
    }
    
    while (1) {
        draw_screen(notes, note_count);
        usleep(100000);
    }
    
    close(dev_fd);
    restore_terminal();
    return 0;
}
