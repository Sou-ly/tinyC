#include "codegen.h"
#include <stdio.h>
#include <string.h>


static x86_Operand codegen_val(IrVal val) {
    switch (val.kind) {
        case IR_CONSTANT:
            return (x86_Operand){.kind = x86_IMM, .imm = val.int_val};
        case IR_VARIABLE:
            return (x86_Operand){.kind = x86_ID, .identifier = strdup(val.name)};
    }
    fprintf(stderr, "codegen: unsupported IR value kind\n");
    exit(1);
}

static x86_Unop codegen_unop(IrUnopType op) {
    switch (op) {
        case IR_NEG:	return x86_NEG;
        case IR_COMP: 	return x86_NOT;
		default:		break;
    }
    fprintf(stderr, "codegen: unsupported unary op\n");
    exit(1);
}

static x86_Binop codegen_binop(IrBinopType op) {
    switch (op) {
        case IR_ADD:	return x86_ADD;
        case IR_SUB: 	return x86_SUB;
		case IR_MUL: 	return x86_MUL;
		default:		break;
    }
    fprintf(stderr, "codegen: unsupported unary op\n");
    exit(1);
}

static void codegen_instr(IrInstruction* ir_instr, x86_InstrList* list) {
    switch (ir_instr->type) {
        case IR_RETURN: {
            x86_Operand src = codegen_val(ir_instr->ret.val);
            x86_instr_list_append(list, (x86_Instr){.kind = x86_MOV, .mov = {
                .dst = (x86_Operand){.kind = x86_REG, .reg = x86_AX},
                .src = src
            }});
            x86_instr_list_append(list, (x86_Instr){.kind = x86_RET});
            return;
        }
        case IR_UNOP: {
            x86_Operand src = codegen_val(ir_instr->unary.src);
            x86_Operand dst = codegen_val(ir_instr->unary.dst);
            x86_instr_list_append(list, (x86_Instr){.kind = x86_MOV, .mov = {
                .dst = dst,
                .src = src
            }});
            x86_Operand dst2 = codegen_val(ir_instr->unary.dst);
            x86_instr_list_append(list, (x86_Instr){.kind = x86_UNOP, .unop = {
                .unop = codegen_unop(ir_instr->unary.op),
                .operand = dst2
            }});
            return;
		}
		case IR_BINOP: {
			switch (ir_instr->binop.op) {
				case IR_DIV: {
					// lhs / rhs: load the dividend (lhs) into eax, sign-extend
					// into edx:eax with cdq, then divide by the divisor (rhs).
					// The quotient ends up in eax.
					x86_Operand dividend = codegen_val(ir_instr->binop.lhs);
					x86_Operand divisor = codegen_val(ir_instr->binop.rhs);
					x86_Operand dst = codegen_val(ir_instr->binop.dst);
					x86_Operand eax = { x86_REG, .reg = x86_AX };
					x86_instr_list_append(list, (x86_Instr) {.kind = x86_MOV, .mov = {
						.dst = eax,
						.src = dividend
					}});
					x86_instr_list_append(list, (x86_Instr) {.kind = x86_CDQ });
					x86_instr_list_append(list, (x86_Instr) {.kind = x86_IDIV, .idiv = {
						.operand = divisor
					}});
					x86_instr_list_append(list, (x86_Instr) {.kind = x86_MOV, .mov = {
						.dst = dst,
						.src = eax
					}});
					return;
				}
				case IR_MOD: {
					// lhs % rhs: same setup as division, but the remainder is
					// left in edx.
					x86_Operand dividend = codegen_val(ir_instr->binop.lhs);
					x86_Operand divisor = codegen_val(ir_instr->binop.rhs);
					x86_Operand dst = codegen_val(ir_instr->binop.dst);
					x86_Operand eax = { x86_REG, .reg = x86_AX };
					x86_Operand edx = { x86_REG, .reg = x86_DX };
					x86_instr_list_append(list, (x86_Instr) {.kind = x86_MOV, .mov = {
						.dst = eax,
						.src = dividend
					}});
					x86_instr_list_append(list, (x86_Instr) {.kind = x86_CDQ });
					x86_instr_list_append(list, (x86_Instr) {.kind = x86_IDIV, .idiv = {
						.operand = divisor
					}});
					x86_instr_list_append(list, (x86_Instr) {.kind = x86_MOV, .mov = {
						.dst = dst,
						.src = edx
					}});
					return;
				}
				default: {
					x86_Operand lhs = codegen_val(ir_instr->binop.lhs);
					x86_Operand rhs = codegen_val(ir_instr->binop.rhs);
					x86_Operand dst = codegen_val(ir_instr->binop.dst);
					x86_instr_list_append(list, (x86_Instr) {.kind = x86_MOV, .mov = {
						.dst = dst,
						.src = lhs
					}});
					x86_instr_list_append(list, (x86_Instr) {.kind = x86_BINOP, .binop = {
						.optype = codegen_binop(ir_instr->binop.op),
						.rhs	= rhs,
						.dst	= dst
					}});
					return;	
				}
			}
		}
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
        if (instr->kind == x86_MOV 
            && instr->mov.src.kind == x86_STACK 
            && instr->mov.dst.kind == x86_STACK) {
            x86_Instr* next_instr = malloc(sizeof(x86_Instr));
            next_instr->kind = x86_MOV;
            next_instr->mov.dst = instr->mov.dst;
            next_instr->mov.src = (x86_Operand){.kind=x86_REG, .reg=x86_R10};
            next_instr->next = instr->next;
            instr->mov.dst = (x86_Operand){.kind=x86_REG, .reg=x86_R10}; 
            instr->next = next_instr;
        }
        instr = instr->next;
    }
    x86_instr_list_prepend(&function->instrs,
        (x86_Instr){.kind = x86_ALLOC, .alloc_stack = {.size = -stack_offset}});
    return 0;
}
