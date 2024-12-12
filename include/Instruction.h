#ifndef _PALLADIUM_INSTRUCTION_H
#define _PALLADIUM_INSTRUCTION_H

#include "Util.h"
#include "VMType.h"
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

// class VM;

using InstructionResult = ResultOr<bool>;

template <class VM> struct Instruction {
  Instruction() = default;
  virtual ~Instruction() = default;
  virtual InstructionResult execute(VM *vm) = 0;
};

// c(0) = c(i)
template <class VM> struct Load : public Instruction<VM> {
  Load(std::size_t i) : _i(i) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &registers = vm->registers();
    registers[0] = registers[_i];
    vm->inc_pc();
    return true;
  }

private:
  std::size_t _i;
};

// c(0) = i
template <class VM> struct CLoad : public Instruction<VM> {
  CLoad(const VMType &value) : _value(value) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &registers = vm->registers();
    registers[0] = _value;
    vm->inc_pc();
    return true;
  }

private:
  VMType _value;
};

// c(0) = c(c(i))
template <class VM> struct INDLoad : public Instruction<VM> {
  INDLoad(std::size_t i) : _i(i) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &registers = vm->registers();
    if (is_vm_type<int>(registers[_i])) {
      int index = vm_type_get<int>(registers[_i]);
      registers[0] = registers[index];
    }
    vm->inc_pc();
    return true;
  }

private:
  std::size_t _i;
};

// Load top of the stack to register i
// c(i) = top(stack)
template <class VM> struct SLoad : public Instruction<VM> {
  SLoad(std::size_t i) : _i(i) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &registers = vm->registers();
    registers[0] = vm->stack_top();
    vm->stack_pop();
    vm->inc_pc();

    return true;
  }

private:
  std::size_t _i;
};

// c(i) = c(0)
template <class VM> struct Store : public Instruction<VM> {
  Store(std::size_t i) : _i(i) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &registers = vm->registers();
    registers[_i] = registers[0];
    vm->inc_pc();
    return true;
  }

private:
  std::size_t _i;
};

// c(c(i)) = c(0)
template <class VM> struct INDStore : public Instruction<VM> {
  INDStore(std::size_t i) : _i(i) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &registers = vm->registers();
    if (is_vm_type<int>(registers[_i])) {
      int index = vm_type_get<int>(registers[_i]);
      registers[index] = registers[0];
      vm->inc_pc();
    }
    return err("expected int in register reg(" + std::to_string(_i) + ")");
  }

private:
  std::size_t _i;
};

// c(0) = c(0) +c(i)
template <class VM> struct Add : public Instruction<VM> {
  Add(std::size_t i) : _i(i) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &registers = vm->registers();

    auto res = add(registers[0], registers[_i]);
    if (res.ok()) {
      registers[0] = res.result();
      vm->inc_pc();
      return true;
    }
    return res.error_value();
  }

private:
  std::size_t _i;
};

// c(0) = c(0) + i
template <class VM> struct CAdd : public Instruction<VM> {
  CAdd(const VMType &i) : _i(i) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &registers = vm->registers();
    auto res = add(registers[0], _i);
    if (res.ok()) {
      registers[0] = res.result();
      vm->inc_pc();
      return true;
    }
    return res.error_value();
  }

private:
  VMType _i;
};

// c(0) = c(0) + c(c(i))
template <class VM> struct INDAdd : public Instruction<VM> {
  INDAdd(std::size_t i) : _i(i) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &registers = vm->registers();
    if (is_vm_type<int>(registers[_i])) {
      int index = vm_type_get<int>(registers[_i]);
      auto res = add(registers[0], registers[index]);
      if (res.ok()) {
        registers[0] = res.result();
        vm->inc_pc();
        return true;
      }
      return res.error_value();
    }
    return err("expected int in register reg(" + std::to_string(_i) + ")");
  }

private:
  std::size_t _i;
};

// c(0) op i
// in (0 is <, 1 is >,2 is ==,3 is !=,4 is <=, 5 is >=)
template <class VM> struct If : public Instruction<VM> {
  If(std::size_t cond, const VMType &value, std::size_t target)
      : _cond(cond), _value(value), _target(target) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &registers = vm->registers();
    auto register_value = std::get<VMPrimitive>(registers[0]);
    auto value = std::get<VMPrimitive>(_value);
    bool condition = false;

    switch (_cond) {
    case 0:
      condition = (register_value < value);
      break;
    case 1:
      condition = (register_value > value);
      break;
    case 2:
      condition = (register_value == value);
      break;
    case 3:
      condition = (register_value != value);
      break;
    case 4:
      condition = (register_value <= value);
      break;
    case 5:
      condition = (register_value >= value);
      break;
    }
    if (condition) {
      vm->set_pc(_target);
    } else {
      vm->inc_pc();
    }
    return true;
  }

private:
  int _cond;
  VMType _value;
  std::size_t _target;
};

template <class VM> struct Goto : public Instruction<VM> {
  Goto(std::size_t i) : _i(i) {}

  auto execute(VM *vm) -> InstructionResult override {
    vm->set_pc(_i);
    return true;
  }

private:
  std::size_t _i;
};

template <class VM> struct Halt : public Instruction<VM> {
  Halt() {}
  auto execute(VM *vm) -> InstructionResult override {
    UNUSED(vm);
    return true;
  }
};

template <class VM> struct Push : public Instruction<VM> {
  Push(const VMType &value) : _value(value) {}

  auto execute(VM *vm) -> InstructionResult override {
    vm->stack_push(_value);
    vm->inc_pc();
    return true;
  }

private:
  VMType _value;
};

template <class VM> struct Pop : public Instruction<VM> {
  Pop() {}
  auto execute(VM *vm) -> InstructionResult override {
    vm->stack_pop();
    vm->inc_pc();
    return true;
  }
};

// print top of stack
template <class VM> struct Print : public Instruction<VM> {
  Print() {}
  auto execute(VM *vm) -> InstructionResult override {
    auto v = vm->stack_top();
    vm->stack_pop();
    auto res = to_string(v);
    if (res.ok()) {
      std::cout << res.result();
      std::flush(std::cout);
      vm->inc_pc();
      return true;
    }
    return res.error_value();
  }
};

template <class VM> struct PrintRegStructField : public Instruction<VM> {
  PrintRegStructField(std::size_t i, std::size_t adr) : _i(i), _adr(adr) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &v = std::get<VMStruct>(vm->registers()[_i]).get_field(_adr);
    VMType field_value = std::get<VMPrimitive>(v);
    auto res = to_string(field_value);
    if (res.ok()) {
      std::cout << res.result();
      std::flush(std::cout);
      vm->inc_pc();
      return true;
    }
    return res.error_value();
  }

private:
  std::size_t _i;
  std::size_t _adr;
};

template <class VM> struct Call : public Instruction<VM> {
  Call(const VMType &fname) : _fname(fname) {}

  auto execute(VM *vm) -> InstructionResult override {
    std::string fname = vm_type_get<std::string>(_fname).result_or("");

    const auto &entry = vm->function_entry(fname);
    vm->make_stack_frame();
    if (vm->registers().size() <= entry.argument_count()) {
      return err("Function " + fname +
                 " not enough registers to store arguments");
    }
    for (uint8_t i = 0; i < entry.argument_count(); ++i) {
      auto value = vm->stack_top();
      vm->registers()[i] = value;
      vm->stack_pop();
    }
    vm->set_pc(entry.address());
    return true;
  }

private:
  VMType _fname;
};

template <class VM> struct CallNative : public Instruction<VM> {
  CallNative(const VMType &fname) : _fname(fname) {}

  auto execute(VM *vm) -> InstructionResult override {
    std::string fname = vm_type_get<std::string>(_fname).result_or("");

    const auto &entry = vm->native_function_entry(fname);
    std::vector<VMType> args;
    for (uint8_t i = 0; i < entry.argument_count(); ++i) {
      auto value = vm->stack_top();
      args.push_back(value);
      vm->stack_pop();
    }
    auto res = entry(vm, args);
    if (!res) {
      return res;
    }
    vm->inc_pc();
    return true;
  }

private:
  VMType _fname;
};

template <class VM> struct RetVoid : public Instruction<VM> {
  RetVoid() {}
  auto execute(VM *vm) -> InstructionResult override {
    vm->restore_from_call_stack();
    return true;
  }
};

template <class VM> struct Return : public Instruction<VM> {
  Return(std::size_t i) : _i(i) {}

  auto execute(VM *vm) -> InstructionResult override {
    const VMType ret_value = vm->registers()[_i];
    vm->restore_from_call_stack();
    vm->stack_push(ret_value);
    return true;
  }

public:
  std::size_t _i;
};

template <class VM> struct StructCreate : public Instruction<VM> {
  StructCreate(std::size_t i, std::size_t sz) : _i(i), _sz(sz) {}

  auto execute(VM *vm) -> InstructionResult override {
    vm->registers()[_i] = VMStruct(_sz);
    vm->inc_pc();
    return true;
  }

private:
  std::size_t _i;
  std::size_t _sz;
};

template <class VM> struct AddField : public Instruction<VM> {
  AddField(std::size_t i, const VMStructTypes &type) : _i(i), _type(type) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &s = std::get<VMStruct>(vm->registers()[_i]);
    s.add_field(_type);
    vm->inc_pc();
    return true;
  }

private:
  std::size_t _i;
  VMStructTypes _type;
};

template <class VM> struct SetField : public Instruction<VM> {
  SetField(std::size_t i, std::size_t field_adr, const VMStructTypes &type)
      : _i(i), _field_adr(field_adr), _type(type) {}

  auto execute(VM *vm) -> InstructionResult override {
    auto &s = std::get<VMStruct>(vm->registers()[_i]);
    s.set_field(_field_adr, _type);
    vm->inc_pc();
    return true;
  }

private:
  std::size_t _i;
  std::size_t _field_adr;
  VMStructTypes _type;
};

template <class VM>
using InstructionType =
    std::variant<Load<VM>, CLoad<VM>, INDLoad<VM>, SLoad<VM>, Store<VM>,
                 INDStore<VM>, Add<VM>, CAdd<VM>, INDAdd<VM>, If<VM>, Goto<VM>,
                 Halt<VM>, Push<VM>, Pop<VM>, Print<VM>,
                 PrintRegStructField<VM>, Call<VM>, CallNative<VM>, RetVoid<VM>,
                 Return<VM>, StructCreate<VM>, AddField<VM>, SetField<VM>>;
#endif
