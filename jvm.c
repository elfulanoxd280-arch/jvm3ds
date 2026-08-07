
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OBJECTS 512
#define MAX_FIELDS   16
#define STR_LEN     256

typedef struct {
    int      usado;
    int      es_string;
    int      es_array;
    int      array_len;
    int32_t *array_data;
    char     str[STR_LEN];
    int32_t  campos[MAX_FIELDS];
} Objeto;

Objeto heap[MAX_OBJECTS];

int32_t heap_new() {
    for (int i=1;i<MAX_OBJECTS;i++) {
        if (!heap[i].usado) {
            heap[i].usado=1; heap[i].es_string=0;
            heap[i].es_array=0; heap[i].array_len=0;
            heap[i].array_data=NULL; heap[i].str[0]=0;
            memset(heap[i].campos,0,sizeof(heap[i].campos));
            return i;
        }
    }
    printf("[JVM] heap lleno\n"); return -1;
}

int32_t heap_new_string(const char *s) {
    int32_t r=heap_new(); if(r<0)return -1;
    heap[r].es_string=1; strncpy(heap[r].str,s,STR_LEN-1); return r;
}

int32_t heap_new_array(int len) {
    int32_t r=heap_new(); if(r<0)return -1;
    heap[r].es_array=1; heap[r].array_len=len;
    heap[r].array_data=calloc(len,sizeof(int32_t)); return r;
}

void    heap_put(int32_t r,int i,int32_t v){heap[r].campos[i]=v;}
int32_t heap_get(int32_t r,int i){return heap[r].campos[i];}

#define MAX_CP 256
typedef struct { uint8_t tag; char str[256]; uint16_t a,b; } CPEntry;
CPEntry cp[MAX_CP];
int cp_size=0;

void leer_cp(FILE *f,int cp_count){
    cp_size=cp_count;
    for(int i=1;i<cp_count;i++){
        uint8_t tag; fread(&tag,1,1,f); cp[i].tag=tag;
        switch(tag){
            case 1:{uint8_t b[2];fread(b,1,2,f);uint16_t l=(b[0]<<8)|b[1];
                if(l>=255){fseek(f,l,SEEK_CUR);break;}
                fread(cp[i].str,1,l,f);cp[i].str[l]=0;break;}
            case 3:case 4:fseek(f,4,SEEK_CUR);break;
            case 5:case 6:fseek(f,8,SEEK_CUR);i++;break;
            case 7:case 8:case 16:{uint8_t b[2];fread(b,1,2,f);cp[i].a=(b[0]<<8)|b[1];break;}
            case 9:case 10:case 11:case 12:case 18:{
                uint8_t b[4];fread(b,1,4,f);
                cp[i].a=(b[0]<<8)|b[1];cp[i].b=(b[2]<<8)|b[3];break;}
            case 15:{uint8_t x;fread(&x,1,1,f);fseek(f,2,SEEK_CUR);break;}
        }
    }
}

const char* cp_string(int i){
    if(cp[i].tag==8)return cp[cp[i].a].str;
    if(cp[i].tag==1)return cp[i].str;
    return "";
}

#define MAX_METHODS 64
static int call_depth = 0;
typedef struct {
    char nombre[64],descriptor[64];
    uint8_t *code; uint32_t code_len; uint16_t max_locals;
} Metodo;
Metodo metodos[MAX_METHODS];
int num_metodos=0;

int32_t ejecutar(int mi,int32_t *lin,int na);

int32_t ejecutar(int mi,int32_t *lin,int na){
    if(++call_depth>64){if(strcmp(metodos[mi].nombre,"<init>")!=0)printf("[JVM] Stack overflow en %s\n",metodos[mi].nombre);call_depth--;return 0;}
    Metodo *m=&metodos[mi];
    uint8_t *code=m->code; uint32_t code_len=m->code_len;
    int32_t locals[32]={0};
    for(int i=0;i<na&&i<32;i++)locals[i]=lin[i];
    int32_t ls[256]; int lt=0;
    #define lp(v) ls[lt++]=(v)
    #define lo()  ls[--lt]
    uint32_t pc=0; int ic=0;
    while(pc<code_len){
        if(++ic>2000000){printf("[JVM] TIMEOUT %s pc=%d\n",m->nombre,pc);call_depth--;return 0;}
        uint8_t op=code[pc++];
        switch(op){
            case 0x00:break;
            case 0x01:lp(0);break;
            case 0x02:lp(-1);break;
            case 0x03:lp(0);break;
            case 0x04:lp(1);break;
            case 0x05:lp(2);break;
            case 0x06:lp(3);break;
            case 0x07:lp(4);break;
            case 0x08:lp(5);break;
            case 0x10:{int8_t v=(int8_t)code[pc++];lp(v);break;}
            case 0x11:{int16_t v=(int16_t)((code[pc]<<8)|code[pc+1]);pc+=2;lp(v);break;}
            case 0x12:{uint8_t i=code[pc++];lp(cp[i].tag==8?heap_new_string(cp_string(i)):0);break;}
            case 0x13:{uint16_t i=(code[pc]<<8)|code[pc+1];pc+=2;lp(cp[i].tag==8?heap_new_string(cp_string(i)):0);break;}
            case 0x1a:lp(locals[0]);break;
            case 0x1b:lp(locals[1]);break;
            case 0x1c:lp(locals[2]);break;
            case 0x1d:lp(locals[3]);break;
            case 0x15:{uint8_t i=code[pc++];lp(locals[i]);break;}
            case 0x2a:lp(locals[0]);break;
            case 0x2b:lp(locals[1]);break;
            case 0x2c:lp(locals[2]);break;
            case 0x2d:lp(locals[3]);break;
            case 0x19:{uint8_t i=code[pc++];lp(locals[i]);break;}
            case 0x3b:locals[0]=lo();break;
            case 0x3c:locals[1]=lo();break;
            case 0x3d:locals[2]=lo();break;
            case 0x3e:locals[3]=lo();break;
            case 0x36:{uint8_t i=code[pc++];locals[i]=lo();break;}
            case 0x4b:locals[0]=lo();break;
            case 0x4c:locals[1]=lo();break;
            case 0x4d:locals[2]=lo();break;
            case 0x4e:locals[3]=lo();break;
            case 0x3a:{uint8_t i=code[pc++];locals[i]=lo();break;}
            case 0x84:{uint8_t i=code[pc++];int8_t v=(int8_t)code[pc++];locals[i]+=v;break;}
            case 0x60:{int32_t b=lo(),a=lo();lp(a+b);break;}
            case 0x64:{int32_t b=lo(),a=lo();lp(a-b);break;}
            case 0x68:{int32_t b=lo(),a=lo();lp(a*b);break;}
            case 0x6c:{int32_t b=lo(),a=lo();lp(a/b);break;}
            case 0x70:{int32_t b=lo(),a=lo();lp(a%b);break;}
            case 0x74:{lp(-lo());break;}
            case 0x7e:{int32_t b=lo(),a=lo();lp(a&b);break;}
            case 0x80:{int32_t b=lo(),a=lo();lp(a|b);break;}
            case 0x82:{int32_t b=lo(),a=lo();lp(a^b);break;}
            case 0x78:{int32_t b=lo(),a=lo();lp(a<<(b&31));break;}
            case 0x7a:{int32_t b=lo(),a=lo();lp(a>>(b&31));break;}
            case 0x7c:{int32_t b=lo(),a=lo();lp((int32_t)((uint32_t)a>>(b&31)));break;}
            case 0x57:lo();break;
            case 0x58:lo();lo();break;
            case 0x59:{int32_t v=lo();lp(v);lp(v);break;}
            case 0x5a:{int32_t v=lo(),w=lo();lp(v);lp(w);lp(v);break;}
            case 0x91:{lp((int8_t)lo());break;}
            case 0x92:{lp((uint16_t)lo());break;}
            case 0x93:{lp((int16_t)lo());break;}
            case 0xbc:{uint8_t t=code[pc++];int32_t n=lo();lp(heap_new_array(n));(void)t;break;}
            case 0xbd:{pc+=2;lp(heap_new_array(lo()));break;}
            case 0x4f:{int32_t v=lo(),i=lo(),r=lo();if(r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len)heap[r].array_data[i]=v;break;}
            case 0x54:{int32_t v=lo(),i=lo(),r=lo();if(r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len)heap[r].array_data[i]=v&0xFF;break;}
            case 0x55:{int32_t v=lo(),i=lo(),r=lo();if(r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len)heap[r].array_data[i]=v&0xFFFF;break;}
            case 0x56:{int32_t v=lo(),i=lo(),r=lo();if(r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len)heap[r].array_data[i]=v&0xFFFF;break;}
            case 0x2e:{int32_t i=lo(),r=lo();lp((r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len)?heap[r].array_data[i]:0);break;}
            case 0x33:{int32_t i=lo(),r=lo();lp((r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len)?(int8_t)heap[r].array_data[i]:0);break;}
            case 0x34:{int32_t i=lo(),r=lo();lp((r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len)?heap[r].array_data[i]&0xFFFF:0);break;}
            case 0x35:{int32_t i=lo(),r=lo();lp((r>0&&heap[r].es_array&&i>=0&&i<heap[r].array_len)?(int16_t)heap[r].array_data[i]:0);break;}
            case 0xbe:{int32_t r=lo();lp((r>0&&heap[r].es_array)?heap[r].array_len:0);break;}
            case 0xbb:{pc+=2;lp(heap_new());break;}
            case 0xb5:{
                uint16_t idx=(code[pc]<<8)|code[pc+1];pc+=2;
                uint16_t nat=cp[idx].b;
                // Usar combinacion de name+type para slot unico
                int slot=(cp[nat].a*31+cp[nat].b)%MAX_FIELDS;
                int32_t v=lo(),r=lo();
                if(r>0&&r<MAX_OBJECTS&&heap[r].usado)heap_put(r,slot,v);
                break;}
            case 0xb4:{
                uint16_t idx=(code[pc]<<8)|code[pc+1];pc+=2;
                uint16_t nat=cp[idx].b;
                int slot=(cp[nat].a*31+cp[nat].b)%MAX_FIELDS;
                int32_t r=lo();
                lp((r>0&&r<MAX_OBJECTS&&heap[r].usado)?heap_get(r,slot):0);
                break;}
            case 0xb7:case 0xb6:{
                uint16_t idx=(code[pc]<<8)|code[pc+1];pc+=2;
                uint16_t nat=cp[idx].b;
                char *mn=cp[cp[nat].a].str,*md=cp[cp[nat].b].str;

                if(strcmp(mn,"println")==0){
                    int32_t v=lo();lo();
                    if(v>0&&v<MAX_OBJECTS&&heap[v].usado&&heap[v].es_string)printf("%s\n",heap[v].str);
                    else printf("%d\n",v);
                    break;
                }
                if(strcmp(mn,"print")==0){
                    int32_t v=lo();lo();
                    if(v>0&&v<MAX_OBJECTS&&heap[v].usado&&heap[v].es_string)printf("%s",heap[v].str);
                    else printf("%d",v);
                    break;
                }
                int na2=1;
                for(char *p=md+1;*p&&*p!=')';p++){
                    if(*p=='L'){while(*p&&*p!=';')p++;na2++;}
                    else if(*p!='[')na2++;
                }
                int found=-1;
                for(int i=0;i<num_metodos;i++)
                    if(strcmp(metodos[i].nombre,mn)==0&&strcmp(metodos[i].descriptor,md)==0){found=i;break;}
                if(found>=0){
                    int32_t args[16]={0};
                    for(int i=na2-1;i>=0;i--)args[i]=lo();
                    int32_t ret=ejecutar(found,args,na2);
                    char *rt=strchr(md,')');
                    if(rt&&*(rt+1)!='V')lp(ret);
                }else{
                    // Metodo no encontrado - limpiar stack
                    char *rt=strchr(md,')');
                    for(int i=0;i<na2;i++)lo();
                    // Solo push resultado si no es void y no es init
                    if(rt&&*(rt+1)!='V'&&*(rt+1)!='['&&strcmp(mn,"<init>")!=0)lp(0);
                }
                break;}
            case 0xb8:{pc+=2;break;}
            case 0xb2:{pc+=2;lp(0);break;}
            case 0xba:{pc+=4;break;}
            case 0x99:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t a=lo();if(a==0)pc+=o-1;else pc+=2;break;}
            case 0x9a:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t a=lo();if(a!=0)pc+=o-1;else pc+=2;break;}
            case 0x9b:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t a=lo();if(a<0)pc+=o-1;else pc+=2;break;}
            case 0x9c:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t a=lo();if(a>=0)pc+=o-1;else pc+=2;break;}
            case 0x9d:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t a=lo();if(a>0)pc+=o-1;else pc+=2;break;}
            case 0x9e:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t a=lo();if(a<=0)pc+=o-1;else pc+=2;break;}
            case 0x9f:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t b=lo(),a=lo();if(a==b)pc+=o-1;else pc+=2;break;}
            case 0xa0:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t b=lo(),a=lo();if(a!=b)pc+=o-1;else pc+=2;break;}
            case 0xa1:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t b=lo(),a=lo();if(a<b)pc+=o-1;else pc+=2;break;}
            case 0xa2:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t b=lo(),a=lo();if(a>=b)pc+=o-1;else pc+=2;break;}
            case 0xa3:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t b=lo(),a=lo();if(a>b)pc+=o-1;else pc+=2;break;}
            case 0xa4:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t b=lo(),a=lo();if(a<=b)pc+=o-1;else pc+=2;break;}
            case 0xa5:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t b=lo(),a=lo();if(a==b)pc+=o-1;else pc+=2;break;}
            case 0xa6:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t b=lo(),a=lo();if(a!=b)pc+=o-1;else pc+=2;break;}
            case 0xa7:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);pc+=o-1;break;}
            case 0xc6:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t a=lo();if(a==0)pc+=o-1;else pc+=2;break;}
            case 0xc7:{int16_t o=(int16_t)((code[pc]<<8)|code[pc+1]);int32_t a=lo();if(a!=0)pc+=o-1;else pc+=2;break;}
            case 0xb1:call_depth--;return 0;
            case 0xac:{int32_t r=lo();call_depth--;return r;}
            case 0xb0:{int32_t r=lo();call_depth--;return r;}
            case 0xc0:case 0xc1:pc+=2;break;
            default:
                printf("[JVM] OP no soportado: 0x%02X pc=%d (%s)\n",op,pc-1,m->nombre);
                return 0;
        }
    }
    #undef lp
    #undef lo
    return 0;
}

void cargar_metodos(FILE *f){
    uint8_t b[4];
    fread(b,1,2,f);uint16_t fc=(b[0]<<8)|b[1];
    for(int i=0;i<fc;i++){
        fseek(f,6,SEEK_CUR);
        fread(b,1,2,f);uint16_t ac=(b[0]<<8)|b[1];
        for(int j=0;j<ac;j++){fseek(f,2,SEEK_CUR);fread(b,1,4,f);uint32_t l=(b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3];fseek(f,l,SEEK_CUR);}
    }
    fread(b,1,2,f);uint16_t mc=(b[0]<<8)|b[1];
    for(int m=0;m<mc;m++){
        fseek(f,2,SEEK_CUR);
        fread(b,1,2,f);uint16_t ni=(b[0]<<8)|b[1];
        fread(b,1,2,f);uint16_t di=(b[0]<<8)|b[1];
        if(num_metodos<MAX_METHODS){
            strncpy(metodos[num_metodos].nombre,cp[ni].str,63);
            strncpy(metodos[num_metodos].descriptor,cp[di].str,63);
        }
        fread(b,1,2,f);uint16_t ac=(b[0]<<8)|b[1];
        for(int a=0;a<ac;a++){
            fseek(f,2,SEEK_CUR);
            fread(b,1,4,f);uint32_t al=(b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3];
            if(al>8&&num_metodos<MAX_METHODS){
                fread(b,1,2,f);fread(b,1,2,f);
                metodos[num_metodos].max_locals=(b[0]<<8)|b[1];
                fread(b,1,4,f);uint32_t cl=(b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3];
                metodos[num_metodos].code=malloc(cl);
                fread(metodos[num_metodos].code,1,cl,f);
                metodos[num_metodos].code_len=cl;
                fseek(f,al-8-cl,SEEK_CUR);
                num_metodos++;
            }else fseek(f,al,SEEK_CUR);
        }
    }
}

int jvm_main_unused(int argc,char *argv[]){return 0;}
