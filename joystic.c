#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>

int main() {
    int joy_fd = open("/dev/input/js0", O_RDONLY);
    if (joy_fd == -1) {
        perror("Erro ao abrir o joystick");
        return 1;
    }

    printf("Pressione qualquer um dos 4 primeiros botões do joystick...\n");
    printf("Pressione Ctrl+C para sair\n");

    struct js_event e;
    while (1) {
        read(joy_fd, &e, sizeof(e));
        if (e.type == JS_EVENT_BUTTON && e.value == 1 && e.number < 4) {
            printf("Botão %d pressionado - ", e.number + 1);
            
            // Simula as cores do Guitar Hero
            const char* cores[4] = {"Verde", "Vermelho", "Amarelo", "Azul"};
            printf("Cor: %s\n", cores[e.number]);
            
            // Aqui você acionaria o LED correspondente na sua placa
        }
    }
    
    close(joy_fd);
    return 0;
}