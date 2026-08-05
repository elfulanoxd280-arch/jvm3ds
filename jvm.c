#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ─── Lectura ─────────────────────────────────────────────────────────────────
uint8_t  read_u8 (FILE *f) { uint8_t b; fread(&b,1,1,f); return b; }
uint16_t read_u16(FILE *f) { uint8_t b[2]; fread(b,1,2,f); return (b[0]<<8)|b[1]; }
uint32_t read_u32(FILE *f) { uint8_t b[4]; fread(b,1,4,f); return (b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3]; }

// ─── Heap ────────────────────────────────────────────────────────────────────
#define MAX_OBJECTS 256
#define MAX_FIELDS   16
#define STR_LEN     256

typedef struct {
    int     usado;
    int     es_string;
    char    str[STR_LEN];   // si es_string
    int      es_array;
    int      array_len;
    int32_t *array_data;
    int32_t campos[MAX_FIELDS];
} Objeto;

Objeto heap[MAX_OBJECTS];

int32_t heap_new() {
    for (int i = 1; i < MAX_OBJECTS; i++) {
        if (!heap[i].usado) {
            heap[i].usado = 1;
            heap[i].es_string = 0;
            heap[i].es_array = 0;
            heap[i].array_len = 0;
            heap[i].array_data = NULL;
            memset(heap[i].campos, 0, sizeof(heap[i].campos));
            heap[i].str[0] = '\0';
            return i;
        }
    }
    printf("[JVM] ERROR: heap lleno\n");
    return -1;
}

int32_t heap_new_string(const char *s) {
    int32_t ref = heap_new();
    if (ref < 0) return -1;
int32_t heap_new_array(int len) {
    int32_t ref = heap_new();
    if (ref < 0) return -1;
    heap[ref].es_array  = 1;
    heap[ref].array_len = len;
    heap[ref].array_data = calloc(len, sizeof(int32_t));
    return ref;
}
    heap[ref].es_string = 1;
    strncpy(heap[ref].str, s, STR_LEN-1);
    return ref;
}


int32_t heap_new_array(int len) {
    int32_t ref = heap_new();
    if (ref < 0) return -1;
    heap[ref].es_array   = 1;
    heap[ref].array_len  = len;
    heap[ref].array_data = calloc(len, sizeof(int32_t));
    return ref;
}
void    heap_put(int32_t ref, int idx, int32_t val) { heap[ref].campos[idx] = val; }
int32_t heap_get(int32_t ref, int idx)              { return heap[ref].campos[idx]; }

// ─── Constant pool ───────────────────────────────────────────────────────────
#define MAX_CP 256
typedef struct {
    uint8_t  tag;
    char     str[256];
    uint16_t a, b;
} CPEntry;

CPEntry cp[MAX_CP];
int     cp_size = 0;

void leer_cp(FILE *f, int cp_count) {
    cp_size = cp_count;
    for (int i = 1; i < cp_count; i++) {
        uint8_t tag = read_u8(f);
        cp[i].tag = tag;
        switch(tag) {
            case 1: {
                uint16_t len = read_u16(f);
                if (len >= 255) { fseek(f,len,SEEK_CUR); break; }
                fread(cp[i].str, 1, len, f);
                cp[i].str[len] = '\0';
                break;
            }
            case 3: case 4: fseek(f,4,SEEK_CUR); break;
            case 5: case 6: fseek(f,8,SEEK_CUR); i++; break;
            case 7: case 8: case 16: cp[i].a=read_u16(f); break;
            case 9: case 10: case 11: case 12: case 18:
                cp[i].a=read_u16(f); cp[i].b=read_u16(f); break;
            case 15: read_u8(f); read_u16(f); break;
        }
    }
}

// Resolver un string del CP (tag 8 apunta a tag 1)
const char* cp_string(int idx) {
    if (cp[idx].tag == 8) return cp[cp[idx].a].str;
    if (cp[idx].tag == 1) return cp[idx].str;
    return "";
}

// ─── Métodos ─────────────────────────────────────────────────────────────────
#define MAX_METHODS 32
typedef struct {
    char     nombre[64];
    char     descriptor[64];
    uint8_t *code;
    uint32_t code_len;
    uint16_t max_locals;
} Metodo;

Metodo metodos[MAX_METHODS];
int    num_metodos = 0;

// ─── Intérprete ──────────────────────────────────────────────────────────────
int32_t ejecutar(int met_idx, int32_t *locals_in, int num_args);

int32_t ejecutar(int met_idx, int32_t *locals_in, int num_args) {
    Metodo  *m    = &metodos[met_idx];
    uint8_t *code = m->code;
    uint32_t code_len = m->code_len;

    int32_t locals[32] = {0};
    for (int i = 0; i < num_args; i++) locals[i] = locals_in[i];

    int32_t lstack[256];
    int     ltop = 0;
    #define lpush(v) lstack[ltop++] = (v)
    #define lpop()   lstack[--ltop]
    #define lpeek()  lstack[ltop-1]

    uint32_t pc = 0;
    while (pc < code_len) {
        uint8_t op = code[pc++];
        switch(op) {

            // ── Constantes ──────────────────────────────────────────────────
            case 0x01: lpush(0);  break; // aconst_null
            case 0x02: lpush(-1); break;
            case 0x03: lpush(0);  break;
            case 0x04: lpush(1);  break;
            case 0x05: lpush(2);  break;
            case 0x06: lpush(3);  break;
            case 0x07: lpush(4);  break;
            case 0x08: lpush(5);  break;

            case 0x10: { int8_t  v=(int8_t) code[pc++]; lpush(v); break; }
            case 0x11: { int16_t v=(int16_t)((code[pc]<<8)|code[pc+1]); pc+=2; lpush(v); break; }

            // ldc — cargar constante (int o String)
            case 0x12: {
                uint8_t idx = code[pc++];
                if (cp[idx].tag == 8) {
                    // Es un String — crear objeto string en heap
                    int32_t ref = heap_new_string(cp_string(idx));
                    lpush(ref);
                } else if (cp[idx].tag == 3) {
                    lpush((int32_t)cp[idx].a);
                } else {
                    lpush(0);
                }
                break;
            }

            // ldc_w — igual pero índice de 2 bytes
            case 0x13: {
                uint16_t idx = (code[pc]<<8)|code[pc+1]; pc+=2;
                if (cp[idx].tag == 8) {
                    int32_t ref = heap_new_string(cp_string(idx));
                    lpush(ref);
                } else {
                    lpush(0);
                }
                break;
            }

            // ── istore ──────────────────────────────────────────────────────
            case 0x3b: locals[0]=lpop(); break;
            case 0x3c: locals[1]=lpop(); break;
            case 0x3d: locals[2]=lpop(); break;
            case 0x3e: locals[3]=lpop(); break;
            case 0x36: { uint8_t i=code[pc++]; locals[i]=lpop(); break; }

            // ── astore (referencias) ─────────────────────────────────────────
            case 0x4b: locals[0]=lpop(); break;
            case 0x4c: locals[1]=lpop(); break;
            case 0x4d: locals[2]=lpop(); break;
            case 0x4e: locals[3]=lpop(); break;
            case 0x3a: { uint8_t i=code[pc++]; locals[i]=lpop(); break; }

            // ── iload ────────────────────────────────────────────────────────
            case 0x1a: lpush(locals[0]); break;
            case 0x1b: lpush(locals[1]); break;
            case 0x1c: lpush(locals[2]); break;
            case 0x1d: lpush(locals[3]); break;
            case 0x15: { uint8_t i=code[pc++]; lpush(locals[i]); break; }

            // ── aload (referencias) ──────────────────────────────────────────
            case 0x2a: lpush(locals[0]); break;
            case 0x2b: lpush(locals[1]); break;
            case 0x2c: lpush(locals[2]); break;
            case 0x2d: lpush(locals[3]); break;
            case 0x19: { uint8_t i=code[pc++]; lpush(locals[i]); break; }

            // ── iinc ─────────────────────────────────────────────────────────
            case 0x84: { uint8_t i=code[pc++]; int8_t v=(int8_t)code[pc++]; locals[i]+=v; break; }

            // ── Aritmética ───────────────────────────────────────────────────
            case 0x60: { int32_t b=lpop(),a=lpop(); lpush(a+b); break; }
            case 0x64: { int32_t b=lpop(),a=lpop(); lpush(a-b); break; }
            case 0x68: { int32_t b=lpop(),a=lpop(); lpush(a*b); break; }
            case 0x6c: { int32_t b=lpop(),a=lpop(); lpush(a/b); break; }

            // ── Stack ops ────────────────────────────────────────────────────
            case 0x57: lpop(); break;
            case 0x58: lpop(); lpop(); break;
            case 0x59: { int32_t v=lpop(); lpush(v); lpush(v); break; }
            case 0x00: break;

            // ── Objetos ──────────────────────────────────────────────────────
            // ── Arrays ──────────────────────────────────────────────────────
            case 0xbc: { uint8_t t=code[pc++]; int32_t n=lpop(); int32_t r=heap_new_array(n); lpush(r); (void)t; break; }
            case 0xbd: { pc+=2; int32_t n=lpop(); lpush(heap_new_array(n)); break; }
            case 0x4f: { int32_t v=lpop(),i=lpop(),r=lpop(); if(r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len) heap[r].array_data[i]=v; break; }
            case 0x54: { int32_t v=lpop(),i=lpop(),r=lpop(); if(r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len) heap[r].array_data[i]=v&0xFF; break; }
            case 0x2e: { int32_t i=lpop(),r=lpop(); lpush((r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len)?heap[r].array_data[i]:0); break; }
            case 0x33: { int32_t i=lpop(),r=lpop(); lpush((r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len)?(int8_t)heap[r].array_data[i]:0); break; }
            case 0xbe: { int32_t r=lpop(); lpush((r>0&&heap[r].es_array)?heap[r].array_len:0); break; }

            case 0xbb: { pc+=2; int32_t ref=heap_new(); lpush(ref); break; }

            case 0xb5: {
                uint16_t idx=(code[pc]<<8)|code[pc+1]; pc+=2;
                uint16_t nat=cp[idx].b;
                char *fname=cp[cp[nat].a].str;
                int slot=(fname[0]=='x')?0:(fname[0]=='y')?1:2;
                int32_t val=lpop(), ref=lpop();
                heap_put(ref,slot,val);
                break;
            }

            case 0xb4: {
                uint16_t idx=(code[pc]<<8)|code[pc+1]; pc+=2;
                uint16_t nat=cp[idx].b;
                char *fname=cp[cp[nat].a].str;
                int slot=(fname[0]=='x')?0:(fname[0]=='y')?1:2;
                int32_t ref=lpop();
                lpush(heap_get(ref,slot));
                break;
            }

            // ── Llamadas ─────────────────────────────────────────────────────
            case 0xb7: // invokespecial
            case 0xb6: // invokevirtual
            {
                uint16_t idx=(code[pc]<<8)|code[pc+1]; pc+=2;
                uint16_t nat=cp[idx].b;
                char *mname=cp[cp[nat].a].str;
                char *mdesc=cp[cp[nat].b].str;

                // println — detectar si es String o int
                if (strcmp(mname,"println")==0) {
                    int32_t val=lpop();
                    lpop(); // obj (System.out)
                    if (val > 0 && val < MAX_OBJECTS && heap[val].usado && heap[val].es_string) {
                        printf("%s\n", heap[val].str);
                    } else {
                        printf("%d\n", val);
                    }
                    break;
                }

                // Contar args
                int nargs=1;
                for (char *p=mdesc+1; *p && *p!=')'; p++) {
                    if (*p=='L') { while(*p && *p!=';') p++; nargs++; }
                    else if (*p!='[') nargs++;
                }

                // Buscar método propio
                int found=-1;
                for (int i=0; i<num_metodos; i++) {
                    if (strcmp(metodos[i].nombre,mname)==0 &&
                        strcmp(metodos[i].descriptor,mdesc)==0) { found=i; break; }
                }

                if (found >= 0) {
                    int32_t args[16]={0};
                    for (int i=nargs-1; i>=0; i--) args[i]=lpop();
                    int32_t ret=ejecutar(found,args,nargs);
                    char *rtype=strchr(mdesc,')');
                    if (rtype && *(rtype+1)!='V') lpush(ret);
                } else {
                    for (int i=0; i<nargs; i++) lpop();
                }
                break;
            }

            case 0xb8: { pc+=2; break; } // invokestatic
            case 0xb2: { pc+=2; lpush(0); break; } // getstatic
            case 0xba: { pc+=4; break; } // invokedynamic (ignorar)

            // ── Saltos ───────────────────────────────────────────────────────
            case 0xa7: { int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]); pc+=o-1; break; }
            case 0xa4: { int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]); int32_t b=lpop(),a=lpop(); if(a<=b) pc+=o-1; else pc+=2; break; }
            case 0xa2: { int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]); int32_t b=lpop(),a=lpop(); if(a>=b) pc+=o-1; else pc+=2; break; }
            case 0xa3: { int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]); int32_t b=lpop(),a=lpop(); if(a>b)  pc+=o-1; else pc+=2; break; }
            case 0xa1: { int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]); int32_t b=lpop(),a=lpop(); if(a<b)  pc+=o-1; else pc+=2; break; }
            case 0x9f: { int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]); int32_t b=lpop(),a=lpop(); if(a==b) pc+=o-1; else pc+=2; break; }
            case 0xa0: { int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]); int32_t b=lpop(),a=lpop(); if(a!=b) pc+=o-1; else pc+=2; break; }
            case 0xc7: { int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]); int32_t a=lpop(); if(a==0) pc+=o-1; else pc+=2; break; } // ifnull
            case 0xc6: { int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]); int32_t a=lpop(); if(a!=0) pc+=o-1; else pc+=2; break; } // ifnonnull

            // ── Return ───────────────────────────────────────────────────────
            case 0xb1: return 0;
            case 0xac: return lpop();
            case 0xb0: return lpop();

            // ── Checkcast / instanceof (ignorar) ────────────────────────────
            case 0xc0: case 0xc1: pc+=2; break;

            default:
                printf("[JVM] Opcode no soportado: 0x%02X en pc=%d (%s)\n", op, pc-1, m->nombre);
                return 0;
        }
    }
    #undef lpush
    #undef lpop
    #undef lpeek
    return 0;
}

// ─── Carga de métodos ─────────────────────────────────────────────────────────
void cargar_metodos(FILE *f) {
    uint16_t fc=read_u16(f);
    for (int i=0;i<fc;i++) {
        fseek(f,6,SEEK_CUR);
        uint16_t ac=read_u16(f);
        for(int j=0;j<ac;j++){fseek(f,2,SEEK_CUR);uint32_t l=read_u32(f);fseek(f,l,SEEK_CUR);}
    }
    uint16_t mc=read_u16(f);
    num_metodos=0;
    for (int m=0;m<mc;m++) {
        read_u16(f);
        uint16_t ni=read_u16(f), di=read_u16(f);
        strncpy(metodos[num_metodos].nombre,    cp[ni].str,63);
        strncpy(metodos[num_metodos].descriptor,cp[di].str,63);
        uint16_t ac=read_u16(f);
        for (int a=0;a<ac;a++) {
            read_u16(f);
            uint32_t alen=read_u32(f);
            if (alen>8) {
                read_u16(f);
                metodos[num_metodos].max_locals=read_u16(f);
                uint32_t clen=read_u32(f);
                metodos[num_metodos].code=malloc(clen);
                fread(metodos[num_metodos].code,1,clen,f);
                metodos[num_metodos].code_len=clen;
                fseek(f,alen-8-clen,SEEK_CUR);
                num_metodos++;
            } else fseek(f,alen,SEEK_CUR);
        }
    }
}

int jvm_main_unused(int argc, char *argv[]) {
    if (argc<2) { printf("Uso: jvm <archivo.class>\n"); return 1; }
    FILE *f=fopen(argv[1],"rb");
    if (!f) { printf("No se pudo abrir\n"); return 1; }

    read_u32(f); read_u16(f); read_u16(f);
    uint16_t cp_count=read_u16(f);
    leer_cp(f,cp_count);

    read_u16(f); read_u16(f); read_u16(f);
    uint16_t ic=read_u16(f);
    fseek(f,ic*2,SEEK_CUR);

    cargar_metodos(f);

    int main_idx=-1;
    for (int i=0;i<num_metodos;i++)
        if (strcmp(metodos[i].nombre,"main")==0) { main_idx=i; break; }

    if (main_idx<0) { printf("[JVM] No se encontro main\n"); return 1; }

    printf("[JVM] Ejecutando main...\n");
    int32_t args[1]={0};
    ejecutar(main_idx,args,1);
    printf("[JVM] Fin.\n");

    fclose(f);
    return 0;
}
