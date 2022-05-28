#include "lc3.h"

int running = 1;
uint16_t reg[R_COUNT];
uint16_t memory[MEMORY_MAX]; 

void printc(char c){
    printf("%c",c);
}

uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}

void read_image_file(FILE* file)
{
    /* the origin tells us where in memory to place the image */
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    /* we know the maximum file size so we only need one fread */
    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    /* swap to little endian */
    while (read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }
}

int lc3_read_image(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if (!file) { return 0; };
    read_image_file(file);
    fclose(file);
    return 1;
}

void mem_write(uint16_t address, uint16_t val)
{
    memory[address] = val;
}

uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR)
    {
        memory[MR_KBSR] = 0;
    }
    return memory[address];
}

void op_add(uint16_t instr){
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;

    if (imm_flag)
    {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[r0] = reg[r1] + imm5;
    }
    else
    {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] + reg[r2];
    }

    update_flags(r0);
}

void op_and(uint16_t instr){
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;

    if (imm_flag)
    {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[r0] = reg[r1] & imm5;
    }
    else
    {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] & reg[r2];
    }
    update_flags(r0);
}

void op_not(uint16_t instr){
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;

    reg[r0] = ~reg[r1];
    update_flags(r0);
}

void op_br(uint16_t instr){
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    uint16_t cond_flag = (instr >> 9) & 0x7;
    if (cond_flag & reg[R_COND])
    {
        reg[R_PC] += pc_offset;
    }
}

void op_jmp(uint16_t instr){
    uint16_t r1 = (instr >> 6) & 0x7;
    reg[R_PC] = reg[r1];
}

void op_jsr(uint16_t instr){
    uint16_t long_flag = (instr >> 11) & 1;
    reg[R_R7] = reg[R_PC];
    if (long_flag)
    {
        uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
        reg[R_PC] += long_pc_offset;  /* JSR */
    }
    else
    {
        uint16_t r1 = (instr >> 6) & 0x7;
        reg[R_PC] = reg[r1]; /* JSRR */
    }
}

void op_ld(uint16_t instr){
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = mem_read(reg[R_PC] + pc_offset);
    update_flags(r0);
}

void op_ldi(uint16_t instr){
    /* destination register (DR) */
    uint16_t r0 = (instr >> 9) & 0x7;
    /* PCoffset 9*/
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    /* add pc_offset to the current PC, look at that memory location to get the final address */
    reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
    update_flags(r0);
}

void op_ldr(uint16_t instr){
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    reg[r0] = mem_read(reg[r1] + offset);
    update_flags(r0);
}

void op_lea(uint16_t instr){
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = reg[R_PC] + pc_offset;
    update_flags(r0);
}

void op_st(uint16_t instr){
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(reg[R_PC] + pc_offset, reg[r0]);
}

void op_sti(uint16_t instr){
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);

}

void op_str(uint16_t instr){
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    mem_write(reg[r1] + offset, reg[r0]);
}

void trap_puts(uint16_t instr){
    uint16_t* c = memory + reg[R_R0];
    while (*c)
    {
        printc((char)*c);
        ++c;
    }
    fflush(stdout);
}

void trap_halt(uint16_t instr){
    //puts("HALT");
    //fflush(stdout);
    running = 0;
}

void op_trap(uint16_t instr){
    switch (instr & 0xFF)
    {
        case TRAP_GETC:
            //trap_getc(instr);
            break;
        case TRAP_OUT:
            //trap_out(instr);
            break;
        case TRAP_PUTS:
            trap_puts(instr);
            break;
        case TRAP_IN:
            //trap_in(instr);
            break;
        case TRAP_PUTSP:
            //trap_putsp(instr);
            break;
        case TRAP_HALT:
            trap_halt(instr);
            break;
    }
}

void lc3_init(){
    /* MAIN */
    /* since exactly one condition flag should be set at any given time, set the Z flag */
    reg[R_COND] = FL_ZRO;

    /* set the PC to starting position */
    /* 0x3000 is the default */
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;
}

void lc3_run_instruction(){
    if(running) {
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;
        switch (op) {
            case OP_ADD:
                op_add(instr);
                break;
            case OP_AND:
                op_and(instr);
                break;
            case OP_NOT:
                op_not(instr);
                break;
            case OP_BR:
                op_br(instr);
                break;
            case OP_JMP:
                op_jmp(instr);
                break;
            case OP_JSR:
                op_jsr(instr);
                break;
            case OP_LD:
                op_ld(instr);
                break;
            case OP_LDI:
                op_ldi(instr);
                break;
            case OP_LDR:
                op_ldr(instr);
                break;
            case OP_LEA:
                op_lea(instr);
                break;
            case OP_ST:
                op_st(instr);
                break;
            case OP_STI:
                op_sti(instr);
                break;
            case OP_STR:
                op_str(instr);
                break;
            case OP_TRAP:
                op_trap(instr);
                break;
            case OP_RES:
            case OP_RTI:
            default:
                break;
        }
    }
}