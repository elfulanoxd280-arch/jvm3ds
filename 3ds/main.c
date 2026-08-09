#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern int  num_metodos;
extern void leer_cp(FILE *f, int cp_count);
extern void cargar_metodos(FILE *f);
extern uint16_t cp_get_a(int idx);
extern uint16_t cp_get_b(int idx);
extern const char* cp_get_str(int idx);
extern int32_t heap_new();
extern void    heap_put(int32_t r, int i, int32_t v);
extern int32_t heap_get(int32_t r, int i);

typedef struct {
    char nombre[64], descriptor[64];
    uint8_t *code; uint32_t code_len; uint16_t max_locals;
} Metodo;
extern Metodo metodos[];

uint8_t  ru8 (FILE *f){uint8_t b;fread(&b,1,1,f);return b;}
uint16_t ru16(FILE *f){uint8_t b[2];fread(b,1,2,f);return(b[0]<<8)|b[1];}
uint32_t ru32(FILE *f){uint8_t b[4];fread(b,1,4,f);return(b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3];}

#define MAX_FRAMES 8
typedef struct {
    uint8_t *code;
    uint32_t len, pc;
    int32_t  loc[16];
    int32_t  ls[32];
    int      lt;
} Frame;

static Frame frames[MAX_FRAMES];
static int   ftop = 0;

int get_slot(uint16_t idx){ return cp_get_a(cp_get_b(idx)) % 8; }

int find_method(const char *name){
    for(int i=0;i<num_metodos;i++)
        if(strcmp(metodos[i].nombre,name)==0) return i;
    return -1;
}

void jvm_run(int start_mi){
    ftop = 0;
    frames[0].code = metodos[start_mi].code;
    frames[0].len  = metodos[start_mi].code_len;
    frames[0].pc   = 0;
    frames[0].lt   = 0;
    memset(frames[0].loc, 0, sizeof(frames[0].loc));
    memset(frames[0].ls,  0, sizeof(frames[0].ls));

    while(ftop >= 0){
        Frame *fr = &frames[ftop];
        if(fr->pc >= fr->len){ ftop--; continue; }

        uint8_t op = fr->code[fr->pc++];

        switch(op){
            case 0x00: break;
            case 0x02: fr->ls[fr->lt++]=-1; break;
            case 0x03: fr->ls[fr->lt++]=0;  break;
            case 0x04: fr->ls[fr->lt++]=1;  break;
            case 0x05: fr->ls[fr->lt++]=2;  break;
            case 0x06: fr->ls[fr->lt++]=3;  break;
            case 0x07: fr->ls[fr->lt++]=4;  break;
            case 0x08: fr->ls[fr->lt++]=5;  break;
            case 0x10: {int8_t v=(int8_t)fr->code[fr->pc++]; fr->ls[fr->lt++]=v; break;}
            case 0x11: {int16_t v=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); fr->pc+=2; fr->ls[fr->lt++]=v; break;}
            case 0x1a: fr->ls[fr->lt++]=fr->loc[0]; break;
            case 0x1b: fr->ls[fr->lt++]=fr->loc[1]; break;
            case 0x1c: fr->ls[fr->lt++]=fr->loc[2]; break;
            case 0x1d: fr->ls[fr->lt++]=fr->loc[3]; break;
            case 0x15: {uint8_t i=fr->code[fr->pc++]; fr->ls[fr->lt++]=fr->loc[i]; break;}
            case 0x2a: fr->ls[fr->lt++]=fr->loc[0]; break;
            case 0x2b: fr->ls[fr->lt++]=fr->loc[1]; break;
            case 0x2c: fr->ls[fr->lt++]=fr->loc[2]; break;
            case 0x2d: fr->ls[fr->lt++]=fr->loc[3]; break;
            case 0x19: {uint8_t i=fr->code[fr->pc++]; fr->ls[fr->lt++]=fr->loc[i]; break;}
            case 0x3b: fr->loc[0]=fr->ls[--fr->lt]; break;
            case 0x3c: fr->loc[1]=fr->ls[--fr->lt]; break;
            case 0x3d: fr->loc[2]=fr->ls[--fr->lt]; break;
            case 0x3e: fr->loc[3]=fr->ls[--fr->lt]; break;
            case 0x36: {uint8_t i=fr->code[fr->pc++]; fr->loc[i]=fr->ls[--fr->lt]; break;}
            case 0x4b: fr->loc[0]=fr->ls[--fr->lt]; break;
            case 0x4c: fr->loc[1]=fr->ls[--fr->lt]; break;
            case 0x4d: fr->loc[2]=fr->ls[--fr->lt]; break;
            case 0x4e: fr->loc[3]=fr->ls[--fr->lt]; break;
            case 0x3a: {uint8_t i=fr->code[fr->pc++]; fr->loc[i]=fr->ls[--fr->lt]; break;}
            case 0x84: {uint8_t i=fr->code[fr->pc++]; int8_t v=(int8_t)fr->code[fr->pc++]; fr->loc[i]+=v; break;}
            case 0x60: {int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; fr->ls[fr->lt++]=a+b; break;}
            case 0x64: {int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; fr->ls[fr->lt++]=a-b; break;}
            case 0x68: {int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; fr->ls[fr->lt++]=a*b; break;}
            case 0x6c: {int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; fr->ls[fr->lt++]=a/b; break;}
            case 0x74: fr->ls[fr->lt-1]=-fr->ls[fr->lt-1]; break;
            case 0x7e: {int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; fr->ls[fr->lt++]=a&b; break;}
            case 0x80: {int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; fr->ls[fr->lt++]=a|b; break;}
            case 0x57: fr->lt--; break;
            case 0x59: fr->ls[fr->lt]=fr->ls[fr->lt-1]; fr->lt++; break;
            case 0xbb: {fr->pc+=2; fr->ls[fr->lt++]=heap_new(); break;}
            case 0xb5: {
                uint16_t idx=(fr->code[fr->pc]<<8)|fr->code[fr->pc+1]; fr->pc+=2;
                int32_t v=fr->ls[--fr->lt], r=fr->ls[--fr->lt];
                heap_put(r, get_slot(idx), v);
                break;
            }
            case 0xb4: {
                uint16_t idx=(fr->code[fr->pc]<<8)|fr->code[fr->pc+1]; fr->pc+=2;
                int32_t r=fr->ls[--fr->lt];
                fr->ls[fr->lt++]=heap_get(r, get_slot(idx));
                break;
            }
            case 0xb7: case 0xb6: {
                uint16_t idx=(fr->code[fr->pc]<<8)|fr->code[fr->pc+1]; fr->pc+=2;
                const char *mn=cp_get_str(cp_get_a(cp_get_b(idx)));
                const char *md=cp_get_str(cp_get_b(cp_get_b(idx)));
                if(strcmp(mn,"println")==0){
                    int32_t v=fr->ls[--fr->lt]; fr->lt--;
                    printf("%ld\n",(long)v);
                    break;
                }
                int na2=1;
                for(const char *p=md+1;*p&&*p!=')';p++){
                    if(*p=='L'){while(*p&&*p!=';')p++;na2++;}
                    else if(*p!='[')na2++;
                }
                int found=find_method(mn);
                if(found>=0 && ftop<MAX_FRAMES-1){
                    int32_t fargs[16]={0};
                    for(int i=na2-1;i>=0;i--) fargs[i]=fr->ls[--fr->lt];
                    ftop++;
                    Frame *nf=&frames[ftop];
                    nf->code=metodos[found].code;
                    nf->len=metodos[found].code_len;
                    nf->pc=0; nf->lt=0;
                    memset(nf->loc,0,sizeof(nf->loc));
                    memset(nf->ls,0,sizeof(nf->ls));
                    for(int i=0;i<na2&&i<16;i++) nf->loc[i]=fargs[i];
                } else {
                    for(int i=0;i<na2;i++) fr->lt--;
                }
                break;
            }
            case 0xb8: {fr->pc+=2; break;}
            case 0xb2: {fr->pc+=2; fr->ls[fr->lt++]=0; break;}
            case 0xba: {fr->pc+=4; break;}
            case 0x99: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t a=fr->ls[--fr->lt]; if(a==0)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0x9a: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t a=fr->ls[--fr->lt]; if(a!=0)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0x9b: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t a=fr->ls[--fr->lt]; if(a<0)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0x9c: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t a=fr->ls[--fr->lt]; if(a>=0)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0x9d: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t a=fr->ls[--fr->lt]; if(a>0)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0x9e: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t a=fr->ls[--fr->lt]; if(a<=0)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0x9f: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; if(a==b)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0xa0: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; if(a!=b)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0xa1: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; if(a<b)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0xa2: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; if(a>=b)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0xa3: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; if(a>b)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0xa4: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt]; if(a<=b)fr->pc+=o-1;else fr->pc+=2; break;}
            case 0xa7: {int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]); fr->pc+=o-1; break;}
            case 0xb1: case 0xac: case 0xb0: ftop--; break;
            default: ftop--; break;
        }
    }
}

int main(){
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);
    printf("JVM3DS v0.2\n\n");

    FILE *f=fopen("sdmc:/3ds/Game.class","rb");
    if(!f){printf("ERROR\n");goto wait;}
    ru32(f);ru16(f);ru16(f);
    uint16_t cp_count=ru16(f);
    leer_cp(f,cp_count);
    ru16(f);ru16(f);ru16(f);
    uint16_t ic=ru16(f);
    fseek(f,ic*2,SEEK_CUR);
    cargar_metodos(f);
    fclose(f);

    printf("Metodos: %d\n\n",num_metodos);
    int mi=find_method("main");
    if(mi<0){printf("No main\n");goto wait;}
    printf("Ejecutando...\n\n");
    jvm_run(mi);
    printf("\nFin!\n");

wait:
    printf("\nSTART=salir\n");
    while(aptMainLoop()){
        hidScanInput();
        if(hidKeysDown()&KEY_START) break;
        gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
