#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint8_t  read_u8 (FILE *f) { uint8_t b; fread(&b,1,1,f); return b; }
uint16_t read_u16(FILE *f) { uint8_t b[2]; fread(b,1,2,f); return (b[0]<<8)|b[1]; }
uint32_t read_u32(FILE *f) { uint8_t b[4]; fread(b,1,4,f); return (b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3]; }

// Nombres de opcodes básicos
const char* opcode_name(uint8_t op) {
    switch(op) {
        case 0x00: return "nop";
        case 0x02: return "iconst_m1";
        case 0x03: return "iconst_0";
        case 0x04: return "iconst_1";
        case 0x05: return "iconst_2";
        case 0x06: return "iconst_3";
        case 0x07: return "iconst_4";
        case 0x08: return "iconst_5";
        case 0x10: return "bipush";
        case 0x15: return "iload";
        case 0x1a: return "iload_0";
        case 0x1b: return "iload_1";
        case 0x1c: return "iload_2";
        case 0x1d: return "iload_3";
        case 0x36: return "istore";
        case 0x3b: return "istore_0";
        case 0x3c: return "istore_1";
        case 0x3d: return "istore_2";
        case 0x3e: return "istore_3";
        case 0x60: return "iadd";
        case 0x64: return "isub";
        case 0x68: return "imul";
        case 0x6c: return "idiv";
        case 0xb1: return "return";
        case 0xb2: return "getstatic";
        case 0xb6: return "invokevirtual";
        case 0xb8: return "invokestatic";
        case 0x57: return "pop";
        case 0x59: return "dup";
        default:   return "???";
    }
}

void skip_cp_entry(FILE *f, uint8_t tag) {
    switch(tag) {
        case 1:  { uint16_t l=read_u16(f); fseek(f,l,SEEK_CUR); break; }
        case 3: case 4: case 9: case 10: case 11: case 12: fseek(f,4,SEEK_CUR); break;
        case 5: case 6: fseek(f,8,SEEK_CUR); break;
        case 7: case 8: case 16: fseek(f,2,SEEK_CUR); break;
        case 15: fseek(f,3,SEEK_CUR); break;
        case 18: fseek(f,4,SEEK_CUR); break;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Uso: reader <archivo.class>\n"); return 1; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { printf("No se pudo abrir\n"); return 1; }

    // Saltar header
    read_u32(f); // magic
    read_u16(f); // minor
    read_u16(f); // major
    uint16_t cp_count = read_u16(f);

    // Saltar constant pool
    for (int i = 1; i < cp_count; i++) {
        uint8_t tag = read_u8(f);
        skip_cp_entry(f, tag);
        if (tag == 5 || tag == 6) i++; // long/double ocupan 2 slots
    }

    // Leer flags, this, super, interfaces
    read_u16(f); // access flags
    read_u16(f); // this class
    read_u16(f); // super class
    uint16_t iface_count = read_u16(f);
    fseek(f, iface_count * 2, SEEK_CUR);

    // Saltar fields
    uint16_t field_count = read_u16(f);
    for (int i = 0; i < field_count; i++) {
        fseek(f, 6, SEEK_CUR);
        uint16_t attr_count = read_u16(f);
        for (int j = 0; j < attr_count; j++) {
            fseek(f, 2, SEEK_CUR);
            uint32_t len = read_u32(f);
            fseek(f, len, SEEK_CUR);
        }
    }

    // Leer métodos
    uint16_t method_count = read_u16(f);
    printf("=== Metodos encontrados: %d ===\n\n", method_count);

    for (int m = 0; m < method_count; m++) {
        read_u16(f); // access flags
        read_u16(f); // name index
        read_u16(f); // descriptor index
        uint16_t attr_count = read_u16(f);

        for (int a = 0; a < attr_count; a++) {
            read_u16(f); // attr name index
            uint32_t attr_len = read_u32(f);

            // El atributo Code contiene el bytecode
            // Lo identificamos por su tamaño (simplificado)
            if (attr_len > 8) {
                uint16_t max_stack  = read_u16(f);
                uint16_t max_locals = read_u16(f);
                uint32_t code_len   = read_u32(f);

                printf("  max_stack=%d  max_locals=%d  bytes=%d\n",
                       max_stack, max_locals, code_len);
                printf("  --- Bytecode ---\n");

                for (uint32_t pc = 0; pc < code_len; ) {
                    uint8_t op = read_u8(f);
                    printf("  [%3d] 0x%02X %s", pc, op, opcode_name(op));
                    pc++;

                    // Opcodes con argumentos
                    if (op == 0xb2 || op == 0xb6 || op == 0xb8) {
                        uint16_t idx = read_u16(f);
                        printf(" #%d", idx);
                        pc += 2;
                    } else if (op == 0x10) { // bipush
                        int8_t val = (int8_t)read_u8(f);
                        printf(" %d", val);
                        pc++;
                    } else if (op == 0x15 || op == 0x36) { // iload/istore
                        uint8_t idx = read_u8(f);
                        printf(" %d", idx);
                        pc++;
                    }
                    printf("\n");
                }

                // Saltar el resto del atributo Code
                uint32_t remaining = attr_len - 8 - code_len;
                fseek(f, remaining, SEEK_CUR);
                printf("\n");
            } else {
                fseek(f, attr_len, SEEK_CUR);
            }
        }
    }

    fclose(f);
    return 0;
}
