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

// ── Framebuffer ──────────────────────────────────────────────────────────────
// Pantalla bottom: 320x240, BGR
static uint8_t cur_r=255, cur_g=255, cur_b=255;

void fb_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b){
    // Bottom screen: 320 wide, 240 tall, column-major
    if(x<0||x>=320||y<0||y>=240) return;
    uint8_t *fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    int idx = (x * 240 + (239 - y)) * 3;
    fb[idx+0] = b;
    fb[idx+1] = g;
    fb[idx+2] = r;
}

void fb_fillrect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b){
    for(int px=x;px<x+w;px++)
        for(int py=y;py<y+h;py++)
            fb_pixel(px,py,r,g,b);
}

// ── Call stack ───────────────────────────────────────────────────────────────
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

void jvm_step(){
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
                int32_t v=fr->ls[--fr->lt],r=fr->ls[--fr->lt];
                heap_put(r,get_slot(idx),v); break;}
            case 0xb4: {
                uint16_t idx=(fr->code[fr->pc]<<8)|fr->code[fr->pc+1]; fr->pc+=2;
                int32_t r=fr->ls[--fr->lt];
                fr->ls[fr->lt++]=heap_get(r,get_slot(idx)); break;}
            case 0xb7: case 0xb6: {
                uint16_t idx=(fr->code[fr->pc]<<8)|fr->code[fr->pc+1]; fr->pc+=2;
                const char *mn=cp_get_str(cp_get_a(cp_get_b(idx)));
                const char *md=cp_get_str(cp_get_b(cp_get_b(idx)));

                // Interceptar metodos graficos
                if(strcmp(mn,"setColor")==0){
                    // args: this, r, g, b
                    int32_t b2=fr->ls[--fr->lt];
                    int32_t g2=fr->ls[--fr->lt];
                    int32_t r2=fr->ls[--fr->lt];
                    fr->lt--; // this
                    cur_r=(uint8_t)r2; cur_g=(uint8_t)g2; cur_b=(uint8_t)b2;
                    break;
                }
                if(strcmp(mn,"fillRect")==0){
                    // args: this, g, x, y, w, h
                    int32_t h=fr->ls[--fr->lt];
                    int32_t w=fr->ls[--fr->lt];
                    int32_t py=fr->ls[--fr->lt];
                    int32_t px=fr->ls[--fr->lt];
                    fr->lt--; // g
                    fr->lt--; // this
                    fb_fillrect(px,py,w,h,cur_r,cur_g,cur_b);
                    break;
                }
                if(strcmp(mn,"println")==0){
                    int32_t v=fr->ls[--fr->lt]; fr->lt--;
                    // print en top screen
                    break;
                }

                int na2=1;
                for(const char *p=md+1;*p&&*p!=')';p++){
                    if(*p=='L'){while(*p&&*p!=';')p++;na2++;}
                    else if(*p!='[')na2++;
                }
                int found=find_method(mn);
                if(found>=0&&ftop<MAX_FRAMES-1){
                    int32_t fa[16]={0};
                    for(int i=na2-1;i>=0;i--) fa[i]=fr->ls[--fr->lt];
                    ftop++;
                    Frame *nf=&frames[ftop];
                    nf->code=metodos[found].code; nf->len=metodos[found].code_len;
                    nf->pc=0; nf->lt=0;
                    memset(nf->loc,0,sizeof(nf->loc)); memset(nf->ls,0,sizeof(nf->ls));
                    for(int i=0;i<na2&&i<16;i++) nf->loc[i]=fa[i];
                } else { for(int i=0;i<na2;i++) fr->lt--; }
                break;}
            case 0xb8: {fr->pc+=2; break;}
            case 0xb2: {fr->pc+=2; fr->ls[fr->lt++]=0; break;}
            case 0xba: {fr->pc+=4; break;}
            case 0x99:{int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]);int32_t a=fr->ls[--fr->lt];if(a==0)fr->pc+=o-1;else fr->pc+=2;break;}
            case 0x9a:{int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]);int32_t a=fr->ls[--fr->lt];if(a!=0)fr->pc+=o-1;else fr->pc+=2;break;}
            case 0x9f:{int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]);int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt];if(a==b)fr->pc+=o-1;else fr->pc+=2;break;}
            case 0xa0:{int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]);int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt];if(a!=b)fr->pc+=o-1;else fr->pc+=2;break;}
            case 0xa1:{int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]);int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt];if(a<b)fr->pc+=o-1;else fr->pc+=2;break;}
            case 0xa2:{int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]);int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt];if(a>=b)fr->pc+=o-1;else fr->pc+=2;break;}
            case 0xa3:{int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]);int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt];if(a>b)fr->pc+=o-1;else fr->pc+=2;break;}
            case 0xa4:{int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]);int32_t b=fr->ls[--fr->lt],a=fr->ls[--fr->lt];if(a<=b)fr->pc+=o-1;else fr->pc+=2;break;}
            case 0xa7:{int16_t o=(int16_t)((fr->code[fr->pc]<<8)|fr->code[fr->pc+1]);fr->pc+=o-1;break;}
            case 0xb1: case 0xac: case 0xb0: ftop--; break;
            default: ftop--; break;
        }
    }
}

#define MIDP_UP    1
#define MIDP_DOWN  2
#define MIDP_LEFT  4
#define MIDP_RIGHT 8
#define MIDP_FIRE  16

int32_t get_midp_keys(){
    u32 k=hidKeysHeld();
    int32_t m=0;
    if(k&KEY_UP)    m|=MIDP_UP;
    if(k&KEY_DOWN)  m|=MIDP_DOWN;
    if(k&KEY_LEFT)  m|=MIDP_LEFT;
    if(k&KEY_RIGHT) m|=MIDP_RIGHT;
    if(k&KEY_A)     m|=MIDP_FIRE;
    return m;
}

void run_method(int mi, int32_t *args, int na){
    if(mi<0) return;
    ftop=0;
    frames[0].code=metodos[mi].code;
    frames[0].len=metodos[mi].code_len;
    frames[0].pc=0; frames[0].lt=0;
    memset(frames[0].loc,0,sizeof(frames[0].loc));
    memset(frames[0].ls,0,sizeof(frames[0].ls));
    for(int i=0;i<na&&i<16;i++) frames[0].loc[i]=args[i];
    jvm_step();
}

int main(){
    gfxInitDefault();
    // Top screen para debug, bottom para juego
    consoleInit(GFX_TOP, NULL);

    printf("JVM3DS v0.3\n\n");

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

    printf("Metodos: %d\n",num_metodos);

    {
        int init_idx=find_method("<init>");
        int upd_idx =find_method("update");
        int drw_idx =find_method("draw");

        if(init_idx<0){printf("No init\n");goto wait;}

        int32_t game_obj=heap_new();
        int32_t iargs[1]={game_obj};
        run_method(init_idx,iargs,1);

        printf("Game listo!\n");
        printf("DPAD=mover A=color\n");
        printf("START=salir\n");

        // Objeto Graphics falso = referencia 99
        int32_t gfx_obj = 99;

        while(aptMainLoop()){
            hidScanInput();
            if(hidKeysHeld()&KEY_START) break;

            // Update
            int32_t uargs[2]={game_obj, get_midp_keys()};
            run_method(upd_idx, uargs, 2);

            // Draw en bottom screen
            if(drw_idx>=0){
                int32_t dargs[2]={game_obj, gfx_obj};
                run_method(drw_idx, dargs, 2);
            }

            gfxFlushBuffers();
            gfxSwapBuffers();
            gspWaitForVBlank();
        }
    }

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
