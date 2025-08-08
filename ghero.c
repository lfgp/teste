#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <linux/joystick.h>
#include <termios.h>
#include <stdbool.h>

#define WIDTH 20
#define HEIGHT 10
#define NOTE_TYPES 4

// Configuração do terminal para entrada não bloqueante
void set_nonblocking_mode() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

// Estrutura para representar uma nota musical
typedef struct {
    int type;   // 1-4 representando os botões
    int y;      // Posição vertical na tela
    bool active; // Se a nota está ativa
} Note;

// Inicializa o joystick
int init_joystick() {
    int joy_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    if (joy_fd == -1) {
        perror("Erro ao abrir o joystick");
        return -1;
    }
    return joy_fd;
}

// Gera uma nova nota aleatória
void generate_note(Note *notes, int *note_count) {
    if (*note_count < WIDTH) {
        notes[*note_count].type = rand() % NOTE_TYPES + 1;
        notes[*note_count].y = 0;
        notes[*note_count].active = true;
        (*note_count)++;
    }
}

// Atualiza a posição das notas
void update_notes(Note *notes, int note_count) {
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active) {
            notes[i].y++;
            if (notes[i].y >= HEIGHT) {
                notes[i].active = false; // Nota saiu da tela
            }
        }
    }
}

// Desenha o jogo na tela
void draw_game(Note *notes, int note_count, int score) {
    printf("\033[H\033[J"); // Limpa a tela
    
    // Desenha as notas
    char note_chars[NOTE_TYPES] = {'1', '2', '3', '4'};
    char colors[NOTE_TYPES][10] = {"\033[32m", "\033[31m", "\033[33m", "\033[34m"};
    
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active) {
            printf("\033[%d;%dH%s%c\033[0m", 
                   notes[i].y + 1, 
                   i + 1, 
                   colors[notes[i].type - 1], 
                   note_chars[notes[i].type - 1]);
        }
    }
    
    // Desenha a linha de "captura"
    printf("\033[%d;1H", HEIGHT);
    for (int i = 0; i < WIDTH; i++) {
        printf("-");
    }
    
    // Mostra a pontuação
    printf("\nScore: %d\n", score);
    printf("Pressione os botões 1-4 no joystick (ou 1-4 no teclado para teste)\n");
    printf("Pressione Q para sair\n");
}

// Verifica se alguma nota foi acertada
void check_hits(Note *notes, int *note_count, int button_pressed, int *score) {
    for (int i = 0; i < *note_count; i++) {
        if (notes[i].active && notes[i].y == HEIGHT - 1 && notes[i].type == button_pressed) {
            notes[i].active = false;
            (*score) += 10;
            printf("\a"); // Beep
        }
    }
}

int main() {
    srand(time(NULL));
    set_nonblocking_mode();
    
    int joy_fd = init_joystick();
    if (joy_fd == -1) {
        printf("Usando teclado para teste (joystick não encontrado)\n");
    }
    
    Note notes[WIDTH] = {0};
    int note_count = 0;
    int score = 0;
    int frame_count = 0;
    
    printf("\033[2J"); // Limpa a tela
    
    while (1) {
        // Geração de notas
        if (frame_count % 15 == 0 && rand() % 3 == 0) {
            generate_note(notes, &note_count);
        }
        
        // Atualização do jogo
        update_notes(notes, note_count);
        draw_game(notes, note_count, score);
        
        // Entrada do joystick
        if (joy_fd != -1) {
            struct js_event e;
            while (read(joy_fd, &e, sizeof(e)) > 0) {
                if (e.type == JS_EVENT_BUTTON && e.value == 1) {
                    if (e.number < 4) { // Apenas os 4 primeiros botões
                        int button_pressed = e.number + 1;
                        check_hits(notes, &note_count, button_pressed, &score);
                    }
                }
            }
        }
        
        // Entrada do teclado (para teste)
        char c;
        if (read(STDIN_FILENO, &c, 1) > 0) {
            if (c >= '1' && c <= '4') {
                int button_pressed = c - '0';
                check_hits(notes, &note_count, button_pressed, &score);
            } else if (c == 'q' || c == 'Q') {
                break;
            }
        }
        
        usleep(50000); // 50ms
        frame_count++;
    }
    
    if (joy_fd != -1) close(joy_fd);
    printf("\033[0m"); // Reseta cores
    printf("Jogo encerrado. Pontuação final: %d\n", score);
    
    return 0;
}