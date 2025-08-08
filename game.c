// guitar_hero_app.c
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>

#define DEVICE_FILE "/dev/guitar_hero"
#define GH_START_GAME 0x4000

int main() {
    int fd = open(DEVICE_FILE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    // Iniciar o jogo
    ioctl(fd, GH_START_GAME, 0);

    // Mapear memória para acesso direto aos registros
    void *regs = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (regs == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return -1;
    }

    // Loop principal do jogo
    while(1) {
        // Ler estado dos botões
        uint32_t buttons = *((uint32_t*)(regs + BUTTONS_REG_OFFSET));
        
        // Atualizar lógica do jogo
        // ...
        
        // Atualizar pontuação
        *((uint32_t*)(regs + SCORE_REG_OFFSET)) = new_score;
    }

    munmap(regs, 4096);
    close(fd);
    return 0;
}