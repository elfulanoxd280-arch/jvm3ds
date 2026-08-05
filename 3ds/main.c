#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ── Declaraciones de jvm.c ───────────────────────────────────────────────────
extern int   num_metodos;
extern void  leer_cp(FILE *f, int cp_count);
extern void  cargar_metodos(FILE *f);
extern int32_t ejecutar(int met_idx, int32_t *locals_in, int num_args);

typedef struct {
    char     nombre[64];
    char     descriptor[64];
    uint8_t *code;
    uint32_t code_len;
    uint16_t max_locals;
} Metodo;
extern Metodo metodos[];

// ── Override de printf para 3DS ──────────────────────────────────────────────
// jvm.c usa printf — en 3DS con consoleInit eso ya funciona directo

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    printf("JVM3DS v0.1\n");
    printf("Buscando Hello.class...\n\n");

    // Abrir el .class desde la SD
    FILE *f = fopen("sdmc:/3ds/Hello.class", "rb");
    if (!f) {
        printf("ERROR: No se encontro\nsdmc:/3ds/Hello.class\n");
        printf("\nPon el archivo en la SD\n");
        printf("\nPRESIONA START para salir\n");
    } else {
        printf("Archivo encontrado!\n");
        printf("Cargando bytecode...\n\n");

        // Leer header del .class
        uint8_t buf[4];
        fread(buf, 1, 4, f); // magic
        uint16_t minor, major, cp_count;
        uint8_t b[2];
        fread(b,1,2,f); minor=(b[0]<<8)|b[1];
        fread(b,1,2,f); major=(b[0]<<8)|b[1];
        fread(b,1,2,f); cp_count=(b[0]<<8)|b[1];

        printf("Java version: %d\n", major-44);
        printf("Constant pool: %d entradas\n\n", cp_count-1);

        // Resetear archivo y dejar que jvm lo procese
        rewind(f);

        // Leer usando funciones de jvm.c
        // (leer magic/minor/major/cp_count de nuevo)
        fread(buf,1,4,f);
        fread(b,1,2,f); fread(b,1,2,f);
        fread(b,1,2,f); cp_count=(b[0]<<8)|b[1];

        leer_cp(f, cp_count);

        // Saltar flags, this, super, interfaces
        fread(b,1,2,f); fread(b,1,2,f); fread(b,1,2,f);
        fread(b,1,2,f);
        uint16_t ic=(b[0]<<8)|b[1];
        fseek(f, ic*2, SEEK_CUR);

        cargar_metodos(f);
        fclose(f);

        printf("Metodos cargados: %d\n\n", num_metodos);
        printf("--- Ejecutando main ---\n\n");

        // Buscar y ejecutar main
        int main_idx = -1;
        for (int i = 0; i < num_metodos; i++) {
            if (strcmp(metodos[i].nombre, "main") == 0) {
                main_idx = i; break;
            }
        }

        if (main_idx < 0) {
            printf("ERROR: No se encontro main\n");
        } else {
            int32_t args[1] = {0};
            ejecutar(main_idx, args, 1);
            printf("\n--- Fin ---\n");
        }
    }

    printf("\nPRESIONA START para salir\n");

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
