#include "codegen.h"
#include <stdio.h>
#include <string.h>

static x86_Operand codegen_val(IrVal val) {
    switch (val.kind) {
        case IR_CONSTANT:
            return x86_operand_imm(val.int_val);
        case IR_VARIABLE:
            return x86_operand_id(strdup(val.name));
    }
    fprintf(stderr, "codegen: unsupported IR value kind\n");
    exit(1);
}

static x86_Unop codegen_unop(IrUnopType op) {
    switch (op) {
        case IR_NEG:	return x86_NEG;
        case IR_COMP: 	return x86_COMP;
		default:		break;
    }
    fprintf(stderr, "codegen: unsupported unary op\n");
    exit(1);
}

static x86_Binop codegen_binop(IrBinopType op) {
    switch (op) {
        case IR_ADD:		return x86_ADD;
        case IR_SUB: 		return x86_SUB;
		case IR_MUL: 		return x86_MUL;
        case IR_AND:		return x86_AND;
        case IR_OR: 		return x86_OR;
		case IR_XOR: 		return x86_XOR;
		case IR_RSHIFT: 	return x86_RSHIFT;
		case IR_LSHIFT: 	return x86_LSHIFT;
		default:			break;
    }
    fprintf(stderr, "codegen: unsupported unary op\n");
    exit(1);
}

static x86_ConditionCode codegen_cond(IrBinopType op) {
    switch (op) {
        case IR_EQ:         return x86_E;
        case IR_NEQ:        return x86_NE;
        case IR_LESS:       return x86_L;
        case IR_GREATER:    return x86_G;
        case IR_LEQ:        return x86_LE;
        case IR_GEQ:        return x86_GE;
        default:            break;
    }
    fprintf(stderr, "codegen: unsupported unary relational operator\n");
    exit(1);
}

static void codegen_instr(IrInstruction* ir_instr, x86_InstrList* list) {
    switch (ir_instr->type) {
        case IR_RETURN: {
            x86_Operand src = codegen_val(ir_instr->ret.val);
            x86_instr_list_append(list, x86_mov(x86_operand_reg(x86_AX), src));
            x86_instr_list_append(list, x86_ret());
            return;
        }
        case IR_UNOP: {
            x86_Operand src = codegen_val(ir_instr->unary.src);
            x86_Operand dst = codegen_val(ir_instr->unary.dst);
            if (ir_instr->unary.op == IR_NOT) {
                // !src == 1 iff src == 0: compare src to 0, zero the result,
                // then set its low byte when the compare was equal.
                x86_instr_list_append(list, x86_cmp_instr(src, x86_operand_imm(0)));
                x86_instr_list_append(list, x86_mov(dst, x86_operand_imm(0)));
                x86_instr_list_append(list, x86_setcc_instr(x86_E, dst));
            } else {
                x86_instr_list_append(list, x86_mov(dst, src));
                x86_Operand dst2 = codegen_val(ir_instr->unary.dst);
                x86_instr_list_append(list, x86_unary(codegen_unop(ir_instr->unary.op), dst2));
            }
            return;
		}
		case IR_BINOP: {
			x86_Operand lhs = codegen_val(ir_instr->binop.lhs);
			x86_Operand rhs = codegen_val(ir_instr->binop.rhs);
			x86_Operand dst = codegen_val(ir_instr->binop.dst);
			switch (ir_instr->binop.op) {
				case IR_DIV: {
					// lhs / rhs: load the dividend (lhs) into eax, sign-extend
					// into edx:eax with cdq, then divide by the divisor (rhs).
					// The quotient ends up in eax.
					x86_Operand eax = x86_operand_reg(x86_AX);
					x86_instr_list_append(list, x86_mov(eax, lhs));
					x86_instr_list_append(list, x86_cdq_instr());
					x86_instr_list_append(list, x86_idiv_instr(rhs));
					x86_instr_list_append(list, x86_mov(dst, eax));
					return;
                }
				case IR_MOD: {
					// lhs % rhs: same setup as division, but the remainder is
					// left in edx.
					x86_Operand eax = x86_operand_reg(x86_AX);
					x86_Operand edx = x86_operand_reg(x86_DX);
					x86_instr_list_append(list, x86_mov(eax, lhs));
					x86_instr_list_append(list, x86_cdq_instr());
					x86_instr_list_append(list, x86_idiv_instr(rhs));
					x86_instr_list_append(list, x86_mov(dst, edx));
					return;
                }
                case IR_EQ:
                case IR_NEQ:
                case IR_LESS:
                case IR_GREATER:
                case IR_LEQ:
                case IR_GEQ:
                    x86_instr_list_append(list, x86_cmp_instr(lhs, rhs));
                    x86_instr_list_append(list, x86_mov(dst, x86_operand_imm(0))); 
                    x86_instr_list_append(list, x86_setcc_instr(codegen_cond(ir_instr->binop.op), dst));
                    return;
				default:  {
					x86_instr_list_append(list, x86_mov(dst, lhs));
					x86_instr_list_append(list, x86_binary(codegen_binop(ir_instr->binop.op), rhs, dst));
					return;
                }
			}
            fprintf(stderr, "codegen: unsupported IR binary operation\n");
            exit(1);
		}
        case IR_JUMP:
            x86_instr_list_append(list, x86_jmp_instr(ir_instr->jump.target));
            return;
        case IR_JUMP_ZERO:
            x86_instr_list_append(list, x86_cmp_instr(
                x86_operand_imm(0),
                codegen_val(ir_instr->jump_zero.cond)));
            x86_instr_list_append(list, x86_jmpcc_instr(x86_E, ir_instr->jump_zero.target));
            return;
        case IR_JUMP_NOT_ZERO:
            x86_instr_list_append(list, x86_cmp_instr(
                x86_operand_imm(0),
                codegen_val(ir_instr->jump_not_zero.cond)));
            x86_instr_list_append(list, x86_jmpcc_instr(x86_NE, ir_instr->jump_not_zero.target));
            return;
        case IR_COPY:
            x86_instr_list_append(list, x86_mov(
                codegen_val(ir_instr->copy.dst),
                codegen_val(ir_instr->copy.src)));
            return;
        case IR_LABEL:
            x86_instr_list_append(list, x86_label_instr(ir_instr->label.identifier));
            return;
		default:
			break;
    }
    fprintf(stderr, "codegen: unsupported IR instruction type\n");
    exit(1);
}

static x86_Function codegen_function(IrFunction* ir_fn) {
    x86_InstrList instrs = x86_instr_list_new();
    for (int i = 0; i < ir_fn->size; i++) {
        codegen_instr(&ir_fn->instructions[i], &instrs);
    }
    return make_x86_function(ir_fn->name, instrs);
}

x86_Program codegen(IrProgram* program) {
    x86_Function* functions = malloc(sizeof(x86_Function) * program->size);
    for (int i = 0; i < program->size; i++) {
        functions[i] = codegen_function(&program->functions[i]);
    }
    return make_x86_program(functions, program->size);
}

typedef struct {
    char* key;
    x86_Operand val;
} OperandEntry;

typedef struct {
    OperandEntry* entries;
    int size;
    int capacity;
    int stack_offset;
} OperandMap;

static OperandMap operand_map_create(int capacity) {
    return (OperandMap){
        .entries = malloc(sizeof(OperandEntry) * capacity),
        .size = 0,
        .capacity = capacity,
        .stack_offset = 0,
    };
}

static void operand_map_destroy(OperandMap* map) {
    for (int i = 0; i < map->size; i++) {
        free(map->entries[i].key);
    }
    free(map->entries);
}

static x86_Operand* operand_map_get(OperandMap* map, const char* key) {
    for (int i = 0; i < map->size; i++) {
        if (strcmp(map->entries[i].key, key) == 0) {
            return &map->entries[i].val;
        }
    }
    return NULL;
}

static x86_Operand operand_map_put(OperandMap* map, x86_Operand op) {
    if (op.kind != x86_ID) return op;
    x86_Operand* existing = operand_map_get(map, op.identifier);
    if (existing) return *existing;

    if (map->size == map->capacity) {
        map->capacity *= 2;
        map->entries = realloc(map->entries, sizeof(OperandEntry) * map->capacity);
    }

    map->stack_offset -= 4;
    x86_Operand val = (x86_Operand){.kind = x86_STACK, .stack = map->stack_offset};
    map->entries[map->size] = (OperandEntry){.key = strdup(op.identifier), .val = val};
    map->size++;
    return val;
}

int rename_registers(x86_Function* function) {
    OperandMap opmap = operand_map_create(128); // TODO: find a better value?
    for (x86_Instr* instr = function->instrs.head; instr; instr = instr->next) {
        switch (instr->kind) {
            case x86_MOV:
                instr->mov.dst = operand_map_put(&opmap, instr->mov.dst);
                instr->mov.src = operand_map_put(&opmap, instr->mov.src);
                break;
            case x86_UNOP:
                instr->unop.operand = operand_map_put(&opmap, instr->unop.operand);
                break;
            case x86_BINOP:
                instr->binop.rhs = operand_map_put(&opmap, instr->binop.rhs);
                instr->binop.dst = operand_map_put(&opmap, instr->binop.dst);
                break;
            case x86_IDIV:
                instr->idiv.operand = operand_map_put(&opmap, instr->idiv.operand);
                break;
            case x86_CMP:
                instr->cmp.lhs = operand_map_put(&opmap, instr->cmp.lhs);
                instr->cmp.rhs = operand_map_put(&opmap, instr->cmp.rhs);
                break;
            case x86_SETCC:
                instr->setcc.op = operand_map_put(&opmap, instr->setcc.op);
                break;
            default:
                break;
        }
    }
    int stack_offset = opmap.stack_offset;
    operand_map_destroy(&opmap);
    return stack_offset;
}

// stack_offset can be negative
int allocate_stack(x86_Function* function, int stack_offset) {
    x86_Instr* instr = function->instrs.head;
    while (instr != NULL){
		switch (instr->kind){
			case x86_MOV:
				if (instr->mov.src.kind == x86_STACK && instr->mov.dst.kind == x86_STACK) {
					x86_Instr* next_instr = malloc(sizeof(x86_Instr));
        	    	next_instr->kind = x86_MOV;
        	    	next_instr->mov.dst = instr->mov.dst;
        	    	next_instr->mov.src = (x86_Operand){.kind=x86_REG, .reg=x86_R10};
        	    	next_instr->next = instr->next;
        	    	instr->mov.dst = (x86_Operand){.kind=x86_REG, .reg=x86_R10}; 
        	    	instr->next = next_instr;
				}
				break;
			case x86_BINOP:
				if (instr->binop.dst.kind == x86_STACK && instr->binop.rhs.kind == x86_STACK) {
					// x86 forbids mem,mem: load the rhs into %r10d, then apply the
					// op with %r10d as the source so the result stays in dst.
					x86_Instr* next_instr = malloc(sizeof(x86_Instr));
        	    	next_instr->kind = instr->kind;
					next_instr->binop.optype = instr->binop.optype;
        	    	next_instr->binop.rhs = (x86_Operand){.kind=x86_REG, .reg=x86_R10};
        	    	next_instr->binop.dst = instr->binop.dst;
        	    	next_instr->next = instr->next;
					x86_Operand src = instr->binop.rhs;
        	    	instr->kind = x86_MOV;
        	    	instr->mov.dst = (x86_Operand){.kind=x86_REG, .reg=x86_R10};
        	    	instr->mov.src = src;
        	    	instr->next = next_instr;
				}
				break;
			case x86_CMP:
				if (instr->cmp.lhs.kind == x86_STACK && instr->cmp.rhs.kind == x86_STACK) {
					x86_Instr* next_instr = malloc(sizeof(x86_Instr));
        	    	next_instr->kind = instr->kind;
        	    	next_instr->cmp.rhs = instr->cmp.rhs;
        	    	next_instr->cmp.lhs = (x86_Operand){.kind=x86_REG, .reg=x86_R10};
        	    	next_instr->next = instr->next;
					x86_Operand src = instr->cmp.lhs;
        	    	instr->kind = x86_MOV; 
        	    	instr->mov.dst = (x86_Operand){.kind=x86_REG, .reg=x86_R10}; 
        	    	instr->mov.src = src; 
        	    	instr->next = next_instr;
				} else if (instr->cmp.rhs.kind == x86_IMM) {
					x86_Instr* next_instr = malloc(sizeof(x86_Instr));
        	    	next_instr->kind = instr->kind;
        	    	next_instr->cmp.rhs = (x86_Operand){.kind=x86_REG, .reg=x86_R11};
        	    	next_instr->cmp.lhs = instr->cmp.lhs;
        	    	next_instr->next = instr->next;
					x86_Operand src = instr->cmp.rhs;
        	    	instr->kind = x86_MOV; 
        	    	instr->mov.dst = (x86_Operand){.kind=x86_REG, .reg=x86_R11}; 
        	    	instr->mov.src = src; 
        	    	instr->next = next_instr;
				}
				break;
			default:
				break;
		}
		instr = instr->next;
    }
    x86_instr_list_prepend(&function->instrs, x86_alloc(-stack_offset));
    return 0;
}
