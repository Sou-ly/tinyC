#include "codegen.h"
#include "../common/ice.h"
#include <stdio.h>
#include <string.h>

static x86_Operand codegen_val(IrVal val) {
    switch (val.kind) {
        case IR_CONSTANT:
            return x86_operand_imm(val.as.int_val);
        case IR_VARIABLE:
            return x86_operand_id(strdup(val.as.identifier));
    }
    ICE("codegen: unsupported IR value kind");
}

static x86_Unop codegen_unop(IrUnopType op) {
    switch (op) {
        case IR_NEG:	return x86_NEG;
        case IR_COMP: 	return x86_COMP;
		default:		break;
    }
    ICE("codegen: unsupported unary op");
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
    ICE("codegen: unsupported unary op");
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
    ICE("codegen: unsupported unary relational operator");
}

static const x86_Reg x86_arg_registers[6] = {x86_DI, x86_SI, x86_DX, x86_CX, x86_R8, x86_R9};

static void codegen_instr(IrInstruction* ir_instr, x86_InstrList* list) {
    switch (ir_instr->kind) {
        case IR_RETURN: {
            x86_Operand src = codegen_val(ir_instr->as.ret.val);
            x86_instr_list_append(list, x86_instr_mov(x86_operand_reg(x86_AX), src));
            x86_instr_list_append(list, x86_instr_ret());
            return;
        }
        case IR_UNOP: {
            x86_Operand src = codegen_val(ir_instr->as.unary.src);
            x86_Operand dst = codegen_val(ir_instr->as.unary.dst);
            if (ir_instr->as.unary.op == IR_NOT) {
                // !src == 1 iff src == 0: compare src to 0, zero the result,
                // then set its low byte when the compare was equal.
                x86_instr_list_append(list, x86_instr_cmp(src, x86_operand_imm(0)));
                x86_instr_list_append(list, x86_instr_mov(dst, x86_operand_imm(0)));
                x86_instr_list_append(list, x86_instr_setcc(x86_E, dst));
            } else {
                x86_instr_list_append(list, x86_instr_mov(dst, src));
                x86_Operand dst2 = codegen_val(ir_instr->as.unary.dst);
                x86_instr_list_append(list, x86_instr_unary(codegen_unop(ir_instr->as.unary.op), dst2));
            }
            return;
		}
		case IR_BINOP: {
			x86_Operand lhs = codegen_val(ir_instr->as.binop.lhs);
			x86_Operand rhs = codegen_val(ir_instr->as.binop.rhs);
			x86_Operand dst = codegen_val(ir_instr->as.binop.dst);
			switch (ir_instr->as.binop.op) {
				case IR_DIV: {
					// lhs / rhs: load the dividend (lhs) into eax, sign-extend
					// into edx:eax with cdq, then divide by the divisor (rhs).
					// The quotient ends up in eax.
					x86_Operand eax = x86_operand_reg(x86_AX);
					x86_instr_list_append(list, x86_instr_mov(eax, lhs));
					x86_instr_list_append(list, x86_instr_cdq());
					x86_instr_list_append(list, x86_instr_idiv(rhs));
					x86_instr_list_append(list, x86_instr_mov(dst, eax));
					return;
                }
				case IR_MOD: {
					// lhs % rhs: same setup as division, but the remainder is
					// left in edx.
					x86_Operand eax = x86_operand_reg(x86_AX);
					x86_Operand edx = x86_operand_reg(x86_DX);
					x86_instr_list_append(list, x86_instr_mov(eax, lhs));
					x86_instr_list_append(list, x86_instr_cdq());
					x86_instr_list_append(list, x86_instr_idiv(rhs));
					x86_instr_list_append(list, x86_instr_mov(dst, edx));
					return;
                }
                case IR_EQ:
                case IR_NEQ:
                case IR_LESS:
                case IR_GREATER:
                case IR_LEQ:
                case IR_GEQ:
                    // emit renders x86_Cmp{a, b} as `cmpl a, b`, and AT&T cmp sets
                    // flags for (second - first) = (b - a). We want flags for
                    // (lhs - rhs) so the condition codes read naturally (setl ==
                    // lhs < rhs), so build the operands as (rhs, lhs).
                    x86_instr_list_append(list, x86_instr_cmp(rhs, lhs));
                    x86_instr_list_append(list, x86_instr_mov(dst, x86_operand_imm(0)));
                    x86_instr_list_append(list, x86_instr_setcc(codegen_cond(ir_instr->as.binop.op), dst));
                    return;
				default:  {
					x86_instr_list_append(list, x86_instr_mov(dst, lhs));
					x86_instr_list_append(list, x86_instr_binary(codegen_binop(ir_instr->as.binop.op), rhs, dst));
					return;
                }
			}
            ICE("codegen: unsupported IR binary operation");
		}
        case IR_JUMP:
            x86_instr_list_append(list, x86_instr_jmp(ir_instr->as.jump.target));
            return;
        case IR_JUMP_ZERO:
            x86_instr_list_append(list, x86_instr_cmp(
                x86_operand_imm(0),
                codegen_val(ir_instr->as.jump_zero.cond)));
            x86_instr_list_append(list, x86_instr_jmpcc(x86_E, ir_instr->as.jump_zero.target));
            return;
        case IR_JUMP_NOT_ZERO:
            x86_instr_list_append(list, x86_instr_cmp(
                x86_operand_imm(0),
                codegen_val(ir_instr->as.jump_not_zero.cond)));
            x86_instr_list_append(list, x86_instr_jmpcc(x86_NE, ir_instr->as.jump_not_zero.target));
            return;
        case IR_COPY:
            x86_instr_list_append(list, x86_instr_mov(
                codegen_val(ir_instr->as.copy.dst),
                codegen_val(ir_instr->as.copy.src)));
            return;
        case IR_LABEL:
            x86_instr_list_append(list, x86_instr_label(ir_instr->as.label.identifier));
            return;
		case IR_FUNCALL:
			IrFunctionCall funcall = ir_instr->as.funcall;
			int reg_args = (6 < funcall.args.count)? 6 : funcall.args.count;
			int padding = (reg_args % 2 == 0)? 0 : 8;
			if (padding != 0) x86_instr_list_append(list, x86_instr_alloc(padding));
			for (int i = 0; i < reg_args; i++) {
				x86_Operand src = codegen_val(funcall.args.items[i]);
				x86_Operand dst = x86_operand_reg(x86_arg_registers[i]);
				x86_instr_list_append(list, x86_instr_mov(dst, src));
			}
			for (int i = reg_args - 1; i >= 6; i++) {
				x86_Operand op = codegen_val(funcall.args.items[i]);
				if (op.kind == x86_REG || op.kind == x86_IMM) {
					x86_instr_list_append(list, x86_instr_push(codegen_val(funcall.args.items[i])));
				} else {
					x86_instr_list_append(list, x86_instr_mov(x86_operand_reg(x86_AX), op));
					x86_instr_list_append(list, x86_instr_push(x86_operand_reg(x86_AX)));
				}
			}
			x86_instr_list_append(list, x86_instr_call(funcall.identifier));
			int stack_args = (reg_args < 6)? 0 : funcall.args.count - reg_args;
			int bytes_to_remove = 8 * stack_args + padding;
			if (bytes_to_remove) x86_instr_list_append(list, x86_instr_deallocate(bytes_to_remove));
			// The callee left its result in %eax; copy it out into the
			// destination, not the other way round.
			x86_instr_list_append(list, x86_instr_mov(codegen_val(funcall.dst), x86_operand_reg(x86_AX)));
			return;
		// no default case so that compiler warning catches unsupported ops
    }
    ICE("codegen: unsupported IR instruction type");
}

static x86_Function codegen_function(IrFunction* ir_fn) {
    x86_InstrList list = x86_instr_list_create();

	size_t reg_params = (6 < ir_fn->params.count)? 6 : ir_fn->params.count;
	for (size_t i = 0; i < reg_params; i++) {
		x86_instr_list_append(&list, x86_instr_mov(
			x86_operand_id(strdup(ir_fn->params.items[i])),
			x86_operand_reg(x86_arg_registers[i])));
	}

	for (size_t i = reg_params; i < ir_fn->params.count; i++) {
		x86_instr_list_append(&list, x86_instr_mov(
			x86_operand_id(strdup(ir_fn->params.items[i])),
			x86_operand_stack(16 + 8 * (int)(i - reg_params))));
	}

    for (size_t i = 0; i < ir_fn->instructions.count; i++) {
        codegen_instr(&ir_fn->instructions.items[i], &list);
    }
    return x86_function_create(ir_fn->identifier, list);
}

x86_Program codegen(IrProgram* program) {
    x86_Function* functions = malloc(sizeof(x86_Function) * program->count);
    for (size_t i = 0; i < program->count; i++) {
        functions[i] = codegen_function(&program->items[i]);
    }
    return x86_program_create(functions, program->count);
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
    x86_Operand* existing = operand_map_get(map, op.as.identifier);
    if (existing) return *existing;

    if (map->size == map->capacity) {
        map->capacity *= 2;
        map->entries = realloc(map->entries, sizeof(OperandEntry) * map->capacity);
    }

    map->stack_offset -= 4;
    x86_Operand val = (x86_Operand){.kind = x86_STACK, .as.stack = map->stack_offset};
    map->entries[map->size] = (OperandEntry){.key = strdup(op.as.identifier), .val = val};
    map->size++;
    return val;
}

int rename_registers(x86_Function* function) {
    OperandMap opmap = operand_map_create(128); // TODO: find a better value?
    for (x86_Instr* instr = function->instrs.head; instr; instr = instr->next) {
        switch (instr->kind) {
            case x86_MOV:
                instr->as.mov.dst = operand_map_put(&opmap, instr->as.mov.dst);
                instr->as.mov.src = operand_map_put(&opmap, instr->as.mov.src);
                break;
            case x86_UNOP:
                instr->as.unop.operand = operand_map_put(&opmap, instr->as.unop.operand);
                break;
            case x86_BINOP:
                instr->as.binop.rhs = operand_map_put(&opmap, instr->as.binop.rhs);
                instr->as.binop.dst = operand_map_put(&opmap, instr->as.binop.dst);
                break;
            case x86_IDIV:
                instr->as.idiv.operand = operand_map_put(&opmap, instr->as.idiv.operand);
                break;
            case x86_CMP:
                instr->as.cmp.lhs = operand_map_put(&opmap, instr->as.cmp.lhs);
                instr->as.cmp.rhs = operand_map_put(&opmap, instr->as.cmp.rhs);
                break;
            case x86_SETCC:
                instr->as.setcc.op = operand_map_put(&opmap, instr->as.setcc.op);
                break;
			case x86_DEALLOCATE:
				break;
			case x86_PUSH:
				instr->as.push.operand = operand_map_put(&opmap, instr->as.push.operand);
				break;
			case x86_CALL:
				break;
			// no default to catch missing ones with the compiler
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
				if (instr->as.mov.src.kind == x86_STACK && instr->as.mov.dst.kind == x86_STACK) {
					x86_Instr* next_instr = malloc(sizeof(x86_Instr));
        	    	next_instr->kind = x86_MOV;
        	    	next_instr->as.mov.dst = instr->as.mov.dst;
        	    	next_instr->as.mov.src = (x86_Operand){.kind=x86_REG, .as.reg=x86_R10};
        	    	next_instr->next = instr->next;
        	    	instr->as.mov.dst = (x86_Operand){.kind=x86_REG, .as.reg=x86_R10}; 
        	    	instr->next = next_instr;
				}
				break;
			case x86_BINOP:
				if (instr->as.binop.dst.kind == x86_STACK && instr->as.binop.rhs.kind == x86_STACK) {
					// x86 forbids mem,mem: load the rhs into %r10d, then apply the
					// op with %r10d as the source so the result stays in dst.
					x86_Instr* next_instr = malloc(sizeof(x86_Instr));
        	    	next_instr->kind = instr->kind;
					next_instr->as.binop.optype = instr->as.binop.optype;
        	    	next_instr->as.binop.rhs = (x86_Operand){.kind=x86_REG, .as.reg=x86_R10};
        	    	next_instr->as.binop.dst = instr->as.binop.dst;
        	    	next_instr->next = instr->next;
					x86_Operand src = instr->as.binop.rhs;
        	    	instr->kind = x86_MOV;
        	    	instr->as.mov.dst = (x86_Operand){.kind=x86_REG, .as.reg=x86_R10};
        	    	instr->as.mov.src = src;
        	    	instr->next = next_instr;
				}
				break;
			case x86_CMP:
				if (instr->as.cmp.lhs.kind == x86_STACK && instr->as.cmp.rhs.kind == x86_STACK) {
					x86_Instr* next_instr = malloc(sizeof(x86_Instr));
        	    	next_instr->kind = instr->kind;
        	    	next_instr->as.cmp.rhs = instr->as.cmp.rhs;
        	    	next_instr->as.cmp.lhs = (x86_Operand){.kind=x86_REG, .as.reg=x86_R10};
        	    	next_instr->next = instr->next;
					x86_Operand src = instr->as.cmp.lhs;
        	    	instr->kind = x86_MOV; 
        	    	instr->as.mov.dst = (x86_Operand){.kind=x86_REG, .as.reg=x86_R10}; 
        	    	instr->as.mov.src = src; 
        	    	instr->next = next_instr;
				} else if (instr->as.cmp.rhs.kind == x86_IMM) {
					x86_Instr* next_instr = malloc(sizeof(x86_Instr));
        	    	next_instr->kind = instr->kind;
        	    	next_instr->as.cmp.rhs = (x86_Operand){.kind=x86_REG, .as.reg=x86_R11};
        	    	next_instr->as.cmp.lhs = instr->as.cmp.lhs;
        	    	next_instr->next = instr->next;
					x86_Operand src = instr->as.cmp.rhs;
        	    	instr->kind = x86_MOV; 
        	    	instr->as.mov.dst = (x86_Operand){.kind=x86_REG, .as.reg=x86_R11}; 
        	    	instr->as.mov.src = src; 
        	    	instr->next = next_instr;
				}
				break;
			default:
				break;
		}
		instr = instr->next;
    }
    x86_instr_list_prepend(&function->instrs, x86_instr_alloc(-stack_offset));
    return 0;
}
