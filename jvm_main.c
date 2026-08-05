#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern int     num_metodos;
extern void    leer_cp(FILE *f, int cp_count);
extern void    cargar_metodos(FILE *f);
extern int32_t ejecutar(int met_idx, int32_t *locals_in, int num_args);

typedef struct {
    char     nombre[64];
    char     descriptor[64];
    uint8_t *code;
    uint32_t code_len;
    uint16_t max_locals;
} Metodo;
extern Metodo metodos[];

uint8_t* jar_extract(FILE *f, const char *filename, uint32_t *out_size);
char**   jar_list_classes(FILE *f, int *count);

uint8_t  ru8 (FILE *f) { uint8_t b; fread(&b,1,1,f); return b; }
uint16_t ru16(FILE *f) { uint8_t b[2]; fread(b,1,2,f); return (b[0]<<8)|b[1]; }
uint32_t ru32(FILE *f) { uint8_t b[4]; fread(b,1,4,f); return (b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3]; }

void cargar_class_desde_file(FILE *f) {
    ru32(f); ru16(f); ru16(f);
    uint16_t cp_count = ru16(f);
    leer_cp(f, cp_count);
    ru16(f); ru16(f); ru16(f);
    uint16_t ic = ru16(f);
    fseek(f, ic*2, SEEK_CUR);
    cargar_metodos(f);
}

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Uso: jvm <archivo.class|archivo.jar>\n"); return 1; }

    const char *path = argv[1];
    int len = strlen(path);
    int es_jar = (len > 4 && strcmp(path+len-4, ".jar") == 0);

    if (!es_jar) {
        FILE *f = fopen(path, "rb");
        if (!f) { printf("[JVM] No se pudo abrir %s\n", path); return 1; }
        cargar_class_desde_file(f);
        fclose(f);
    } else {
        FILE *f = fopen(path, "rb");
        if (!f) { printf("[JVM] No se pudo abrir %s\n", path); return 1; }

        int class_count = 0;
        char **classes = jar_list_classes(f, &class_count);
        printf("[JVM] JAR: %d clases encontradas\n", class_count);

        for (int i = 0; i < class_count; i++) {
            printf("[JVM] Cargando %s\n", classes[i]);
            uint32_t size = 0;
            uint8_t *data = jar_extract(f, classes[i], &size);
            if (!data) { free(classes[i]); continue; }

            FILE *tmp = tmpfile();
            fwrite(data, 1, size, tmp);
            rewind(tmp);
            free(data);

            cargar_class_desde_file(tmp);
            fclose(tmp);
            free(classes[i]);
        }
        free(classes);
        fclose(f);
    }

    int main_idx = -1;
    for (int i = 0; i < num_metodos; i++)
        if (strcmp(metodos[i].nombre, "main") == 0) { main_idx=i; break; }

    if (main_idx < 0) { printf("[JVM] No se encontro main\n"); return 1; }

    printf("[JVM] Ejecutando main...\n");
    int32_t args[1] = {0};
    ejecutar(main_idx, args, 1);
    printf("[JVM] Fin.\n");
    return 0;
}
