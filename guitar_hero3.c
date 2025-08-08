#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <linux/joystick.h>
#include <termios.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

// Definições da placa DE2i-150
#define DEVICE_FILE "/dev/my_device"
#define RD_SWITCHES 0x00000001
#define RD_PBUTTONS 0x00000002
#define WR_L_DISPLAY 0x00000003
#define WR_R_DISPLAY 0x00000004
#define WR_RED_LEDS 0x00000005
#define WR_GREEN_LEDS 0x00000006

// Configurações do jogo
#define WIDTH 4
#define HEIGHT 10
#define NOTE_TYPES 4
#define MAX_MISSES 3
#define NOTE_DELAY 150000

// Variáveis globais
int score = 0;
int consecutive_misses = 0;
bool game_over = false;
int dev_fd; // File descriptor para o dispositivo

// Função para escrever nos displays/LEDs
void write_hw(int command, unsigned long value) {
    if (dev_fd >= 0) {
        ioctl(dev_fd, command, value);
    }
}

// Configuração do terminal
void init_terminal() {
    printf("\033[?25l"); // Esconde o cursor
    printf("\033[2J");   // Limpa a tela
}

// Estrutura para representar uma nota
typedef struct {
    int type;
    int y;
    bool active;
} Note;

// Inicializa o joystick
int init_joystick() {
    int joy_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    if (joy_fd == -1) {
        perror("Erro ao abrir o joystick");
    }
    return joy_fd;
}

// Gera uma nova nota
void generate_note(Note *notes, int *note_count, int column) {
    if (*note_count < WIDTH * HEIGHT) {
        for (int i = 0; i < WIDTH * HEIGHT; i++) {
            if (!notes[i].active) {
                notes[i].type = column + 1;
                notes[i].y = 0;
                notes[i].active = true;
                (*note_count)++;
                break;
            }
        }
    }
}

// Atualiza a posição das notas
void update_notes(Note *notes, int note_count) {
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active) {
            notes[i].y++;
            if (notes[i].y >= HEIGHT) {
                notes[i].active = false;
                consecutive_misses++;
                if (consecutive_misses >= MAX_MISSES) {
                    game_over = true;
                    // Aciona LEDs vermelhos ao perder
                    write_hw(WR_RED_LEDS, 0xFF);
                }
            }
        }
    }
}

// Desenha o jogo
void draw_game(Note *notes, int note_count) {
    char buffer[HEIGHT][WIDTH];
    memset(buffer, ' ', sizeof(buffer));

    // Preenche o buffer com as notas
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active) {
            int col = notes[i].type - 1;
            if (col >= 0 && col < WIDTH && notes[i].y < HEIGHT) {
                buffer[notes[i].y][col] = '0' + notes[i].type;
            }
        }
    }

    // Limpa a tela e desenha
    printf("\033[H");
    
    const char* colors[] = {"\033[32m", "\033[31m", "\033[33m", "\033[34m"};
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            if (buffer[y][x] != ' ') {
                int note_type = buffer[y][x] - '1';
                printf("%s%c \033[0m", colors[note_type], buffer[y][x]);
            } else {
                printf(". ");
            }
        }
        printf("\n");
    }
    
    for (int i = 0; i < WIDTH; i++) printf("--");
    printf("\n");
    
    printf("Score: %d | Erros: %d/%d\n", score, consecutive_misses, MAX_MISSES);
    
    if (game_over) {
        printf("\033[31mGAME OVER! Pontuação: %d\033[0m\n", score);
    } else {
        printf("Pressione os botões 1-4 no joystick\n");
    }
    
    // Atualiza display da direita com a pontuação
    write_hw(WR_R_DISPLAY, score);
}

// Verifica acertos
void check_hits(Note *notes, int *note_count, int button) {
    bool hit = false;
    
    for (int i = 0; i < *note_count; i++) {
        if (notes[i].active && notes[i].y == HEIGHT - 1 && notes[i].type == button) {
            notes[i].active = false;
            score += 10;
            consecutive_misses = 0;
            hit = true;
            printf("\a");
            
            // Aciona LED verde ao acertar
            write_hw(WR_GREEN_LEDS, 1 << (button - 1)); // Acende apenas o LED correspondente
            usleep(100000); // Mantém aceso por 100ms
            write_hw(WR_GREEN_LEDS, 0); // Apaga o LED
            break;
        }
    }
    
    if (!hit && button != 0) {
        consecutive_misses++;
        
        // Aciona LED vermelho ao errar
        write_hw(WR_RED_LEDS, 1 << (button - 1));
        usleep(100000); // Mantém aceso por 100ms
        write_hw(WR_RED_LEDS, 0); // Apaga o LED
        
        if (consecutive_misses >= MAX_MISSES) {
            game_over = true;
            // Todos LEDs vermelhos ao perder
            write_hw(WR_RED_LEDS, 0xFF);
        }
    }
}

int main() {
    srand(time(NULL));
    
    // Abre dispositivo da placa
    dev_fd = open(DEVICE_FILE, O_RDWR);
    if (dev_fd < 0) {
        perror("Erro ao abrir dispositivo");
        return 1;
    }
    
    // Inicializa hardware
    write_hw(WR_R_DISPLAY, 0);  // Zera display direito
    write_hw(WR_RED_LEDS, 0);   // Apaga LEDs vermelhos
    write_hw(WR_GREEN_LEDS, 0);  // Apaga LEDs verdes
    
    int joy_fd = init_joystick();
    Note notes[WIDTH * HEIGHT] = {0};
    int note_count = 0;
    int frame = 0;
    
    init_terminal();
    
    while (!game_over) {
        // Geração de notas
        if (frame % 8 == 0 && rand() % 2 == 0) {
            int column = rand() % WIDTH;
            generate_note(notes, &note_count, column);
        }
        
        update_notes(notes, note_count);
        draw_game(notes, note_count);
        
        // Entrada do joystick
        if (joy_fd != -1) {
            struct js_event e;
            while (read(joy_fd, &e, sizeof(e)) > 0) {
                if (e.type == JS_EVENT_BUTTON && e.value == 1 && e.number < 4) {
                    check_hits(notes, &note_count, e.number + 1);
                }
            }
        }
        
        usleep(NOTE_DELAY);
        frame++;
    }
    
    // Tela final
    while (true) {
        draw_game(notes, note_count);
        usleep(100000);
    }
    
    if (joy_fd != -1) close(joy_fd);
    close(dev_fd);
    printf("\033[?25h");
    return 0;
}
