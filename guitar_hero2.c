#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <linux/joystick.h>
#include <termios.h>
#include <stdbool.h>
#include <string.h>

#define WIDTH 4           // 4 colunas (uma para cada botão)
#define HEIGHT 10         // Altura da área de jogo
#define NOTE_TYPES 4      // 4 tipos de notas (1-4)
#define MAX_MISSES 3      // Máximo de erros consecutivos permitidos
#define NOTE_DELAY 150000 // Delay em microssegundos (150ms)

// Variáveis globais para estado do jogo
int score = 0;
int consecutive_misses = 0;
bool game_over = false;

// Configuração do terminal
void init_terminal() {
    printf("\033[?25l"); // Esconde o cursor
    printf("\033[2J");   // Limpa a tela
}

// Estrutura para representar uma nota
typedef struct {
    int type;    // 1-4 representando os botões
    int y;       // Posição vertical
    bool active; // Se está ativa
} Note;

// Inicializa o joystick
int init_joystick() {
    int joy_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    if (joy_fd == -1) {
        perror("Erro ao abrir o joystick");
    }
    return joy_fd;
}

// Gera uma nova nota na coluna especificada
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
    printf("\033[H"); // Move o cursor para o início
    
    // Desenha as notas com cores
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
    
    // Linha de captura
    for (int i = 0; i < WIDTH; i++) printf("--");
    printf("\n");
    
    printf("Score: %d | Erros consecutivos: %d/%d\n", 
           score, consecutive_misses, MAX_MISSES);
    
    if (game_over) {
        printf("\033[31mGAME OVER! Pontuação final: %d\033[0m\n", score);
    } else {
        printf("Pressione os botões 1-4 no joystick\n");
    }
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
            printf("\a"); // Beep
            break;
        }
    }
    
    if (!hit && button != 0) {
        consecutive_misses++;
        if (consecutive_misses >= MAX_MISSES) {
            game_over = true;
        }
    }
}

int main() {
    srand(time(NULL));
    init_terminal();
    
    int joy_fd = init_joystick();
    Note notes[WIDTH * HEIGHT] = {0};
    int note_count = 0;
    int frame = 0;
    
    while (!game_over) {
        // Geração de notas aleatórias
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
        
        usleep(NOTE_DELAY); // Delay ajustável
        frame++;
    }
    
    // Tela final
    while (true) {
        draw_game(notes, note_count);
        usleep(100000);
    }
    
    if (joy_fd != -1) close(joy_fd);
    printf("\033[?25h"); // Mostra o cursor novamente
    return 0;
}