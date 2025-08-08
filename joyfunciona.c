#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>

int main() {
    ButtonMap botoes;
    set_default_button_map(&botoes);
    
    int joy_fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    if (joy_fd == -1) {
        perror("Erro ao abrir o joystick");
        return 1;
    }
    
    printf("Controle configurado com os botões:\n");
    printf("Botão 1: %d\n", botoes.botao1);
    printf("Botão 2: %d\n", botoes.botao2);
    printf("Botão 3: %d\n", botoes.botao3);
    printf("Botão 4: %d\n", botoes.botao4);
    printf("Pressione os botões para testar...\n");

    struct js_event e;
    while (1) {
        while (read(joy_fd, &e, sizeof(e)) > 0) {
            if (e.type == JS_EVENT_BUTTON && e.value == 1) {
                if (e.number == botoes.botao1) {
                    printf("Botão 1 pressionado!\n");
                    // Acione o LED correspondente aqui
                }
                else if (e.number == botoes.botao2) {
                    printf("Botão 2 pressionado!\n");
                    // Acione o LED correspondente aqui
                }
                else if (e.number == botoes.botao3) {
                    printf("Botão 3 pressionado!\n");
                    // Acione o LED correspondente aqui
                }
                else if (e.number == botoes.botao4) {
                    printf("Botão 4 pressionado!\n");
                    // Acione o LED correspondente aqui
                }
            }
        }
        usleep(10000); // Pequena pausa
    }
    
    close(joy_fd);
    return 0;
}