#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <termios.h>

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

// Função para configurar o terminal (que estava faltando)
void init_terminal() {
    struct termios new_termios;
    
    // Salva as configurações originais
    tcgetattr(STDIN_FILENO, &original_termios);
    new_termios = original_termios;
    
    // Desabilita eco e modo canônico
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    
    // Aplica as novas configurações
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    
    // Esconde o cursor e limpa a tela
    printf("\033[?25l");
    printf("\033[2J");
}

// Função para restaurar o terminal
void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
    printf("\033[?25h"); // Mostra o cursor novamente
}

// Função para escrever nos dispositivos de hardware
void write_hw(int command, unsigned long value) {
    ioctl(dev_fd, command, value);
}

// Estrutura para notas do jogo
typedef struct {
    int type; // 1-4
    int y;    // posição vertical
    bool active;
} Note;

// Gera uma nova nota
void generate_note(Note *notes, int *note_count) {
    if (*note_count < WIDTH * HEIGHT) {
        notes[*note_count].type = rand() % 4 + 1;
        notes[*note_count].y = 0;
        notes[*note_count].active = true;
        (*note_count)++;
    }
}

// Atualiza posição das notas
void update_notes(Note *notes, int note_count) {
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active) {
            notes[i].y++;
            if (notes[i].y >= HEIGHT) {
                notes[i].active = false;
                consecutive_misses++;
                if (consecutive_misses >= MAX_MISSES) {
                    game_over = true;
                    write_hw(WR_RED_LEDS, 0xFF); // Acende todos LEDs vermelhos
                }
            }
        }
    }
}

// Mostra o jogo no terminal
void draw_screen(Note *notes, int note_count) {
    printf("\033[H"); // Posiciona cursor no topo
    
    // Desenha as notas
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            bool has_note = false;
            for (int i = 0; i < note_count; i++) {
                if (notes[i].active && notes[i].y == y && notes[i].type == x+1) {
                    printf("%d ", x+1);
                    has_note = true;
                    break;
                }
            }
            if (!has_note) printf(". ");
        }
        printf("\n");
    }
    
    // Linha de base
    for (int i = 0; i < WIDTH; i++) printf("--");
    printf("\n");
    
    printf("Score: %d | Erros: %d/%d\n", score, consecutive_misses, MAX_MISSES);
    if (game_over) printf("GAME OVER! Pressione CTRL+C para sair.\n");
}

// Verifica se o jogador acertou a nota
void check_hit(int button, Note *notes, int note_count) {
    bool hit = false;
    
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active && notes[i].y == HEIGHT-1 && notes[i].type == button) {
            notes[i].active = false;
            score += 10;
            consecutive_misses = 0;
            hit = true;
            
            // Feedback visual - LED verde
            write_hw(WR_GREEN_LEDS, 1 << (button-1));
            usleep(100000);
            write_hw(WR_GREEN_LEDS, 0);
            break;
        }
    }
    
    if (!hit && button != 0) {
        consecutive_misses++;
        
        // Feedback visual - LED vermelho
        write_hw(WR_RED_LEDS, 1 << (button-1));
        usleep(100000);
        write_hw(WR_RED_LEDS, 0);
        
        if (consecutive_misses >= MAX_MISSES) {
            game_over = true;
            write_hw(WR_RED_LEDS, 0xFF);
        }
    }
    
    // Atualiza display com a pontuação
    write_hw(WR_R_DISPLAY, score);
}

int main() {
    srand(time(NULL));
    
    // Inicializa terminal
    init_terminal();
    
    // Inicializa hardware
    dev_fd = open(DEVICE_FILE, O_RDWR);
    if (dev_fd < 0) {
        perror("Erro ao abrir dispositivo");
        restore_terminal();
        return 1;
    }
    
    // Limpa displays e LEDs
    write_hw(WR_R_DISPLAY, 0);
    write_hw(WR_RED_LEDS, 0);
    write_hw(WR_GREEN_LEDS, 0);
    
    Note notes[WIDTH * HEIGHT] = {0};
    int note_count = 0;
    int frame = 0;
    
    printf("Jogo Guitar Hero - DE2i-150\n");
    printf("Use os botões 1-4 para jogar\n");
    
    while (!game_over) {
        // Gera novas notas periodicamente
        if (frame % 10 == 0) {
            generate_note(notes, &note_count);
        }
        
        update_notes(notes, note_count);
        draw_screen(notes, note_count);
        
        // Simula entrada do usuário (substitua pela leitura real dos botões)
        char input = 0;
        if (read(STDIN_FILENO, &input, 1) > 0) {
            if (input >= '1' && input <= '4') {
                check_hit(input - '0', notes, note_count);
            }
        }
        
        usleep(NOTE_DELAY);
        frame++;
    }
    
    // Mantém a tela final visível
    while (1) {
        usleep(100000);
    }
    
    // Limpeza (não será alcançado devido ao loop acima)
    close(dev_fd);
    restore_terminal();
    return 0;
}
