module vm.dislib;

import std.array;
import std.conv;
import std.algorithm;

class Exit: Exception {
  this() {
    super(null);
  }
}

enum Opcode: int {
  exit = 0,
  reg = 1,
  bb = 2,
  num = 3,
  jump = 4,
  func = 5,
  add = 6,
  sub = 7,
  mul = 8,
  div = 9,
  mod = 10,
  blte = 11,
  call = 12,
  ret = 13,
  putchar = 14,
  string = 15,
  length = 16,
  get = 17,
  set = 18,
  dump = 19,
  read = 20,
  write = 21,
  array = 22,
  cat = 23,
  beq = 24,
  blt = 25,
}

class Arg {}

class Num : Arg {
  int num;
  this(int num_) {
    num = num_;
  }
  override string toString() {
    return num.to!string;
  }
  static Num read(ref int index, const(int)[] ops) {
    Num ret = new Num(ops[index++]);
    return ret;
  }
}

class Reg : Arg {
  int reg;
  this(int reg_) {
    reg = reg_;
  }
  override string toString() {
    return 'r' ~ reg.to!string;
  }
  static Reg read(ref int index, const(int)[] ops) {
    Reg ret = new Reg(ops[index++]);
    return ret;
  }
}

class Loc : Arg {
  int loc;
  this(int loc_) {
    loc = loc_;
  }
  override string toString() {
    return 'l' ~ loc.to!string;
  }
  static Loc read(ref int index, const(int)[] ops) {
    Loc ret = new Loc(ops[index++]);
    ops = ops[1..$];
    return ret;
  }
}

class Nums : Arg {
  Num[] nums;
  this(Num[] nums_) {
    nums = nums_;
  }
  override string toString() {
    return '[' ~ nums.map!(to!string).join(' ') ~ ']';
  }
  static Nums read(ref int index, const(int)[] ops) {
    int n = ops[index++];
    Num[] nums;
    foreach (i; 0..n) {
      nums ~= Num.read(index, ops);
    }
    return new Nums(nums);
  }
}

class Regs : Arg {
  Reg[] regs;
  this(Reg[] regs_) {
    regs = regs_;
  }
  override string toString() {
    return '(' ~ regs.map!(to!string).join(' ') ~ ')';
  }
  static Regs read(ref int index, const(int)[] ops) {
    int n = ops[index++];
    Reg[] regs;
    foreach (i; 0..n) {
      regs ~= Reg.read(index, ops);
    }
    return new Regs(regs);
  }
}

class Func : Arg {
  int loc;
  int nregs;
  string name;
  Instr[] instrs;
  this(int nregs_, Instr[] instrs_=null) {
    nregs = nregs_;
    instrs = instrs_;
  }
  override string toString() {
    string ret = "{\n";
    foreach (instr; instrs) {
      ret ~= "  ";
      ret ~= instr.to!string;
      ret ~= "\n";
    }
    ret ~= "}\n";
    return ret;
  }

  static Func read(ref int index, const(int)[] ops, int max, int nregs, string name) {
    Func ret = new Func(nregs);
    ret.loc = index;
    ret.name = name;
    while (index < max) {
      ret.instrs ~= Instr.read(index, ops);
    }
    return ret;
  }
}

class Instr {
  int loc;
  Opcode op;
  Arg[] data;
  this() {}
  override string toString() {
    return op.to!string ~ data.map!(x => ' ' ~ x.to!string).join;
  }
  static Instr read(ref int index, const(int)[] ops) {
    Instr ret = new Instr;
    ret.loc = index;
    ret.op = ops[index++].to!Opcode;
    final switch (ret.op) {
      case Opcode.exit:
        break;
      case Opcode.reg:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.bb:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Loc.read(index, ops);
        ret.data ~= Loc.read(index, ops);
        break;
      case Opcode.num:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Num.read(index, ops);
        break;
      case Opcode.jump:
        ret.data ~= Loc.read(index, ops);
        break;
      case Opcode.func:
        Loc end = Loc.read(index, ops);
        Nums name = Nums.read(index, ops);
        Num nregs = Num.read(index, ops);
        ret.data ~= Func.read(index, ops, end.loc, nregs.num, name.nums.map!(x => cast(char) x.num).array.idup);
        break;
      case Opcode.add:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.sub:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.mul:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.div:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.mod:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.blte:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Loc.read(index, ops);
        ret.data ~= Loc.read(index, ops);
        break;
      case Opcode.call:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Loc.read(index, ops);
        ret.data ~= Regs.read(index, ops);
        break;
      case Opcode.ret:
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.putchar:
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.string:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Nums.read(index, ops);
        break;
      case Opcode.length:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.get:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.set:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.dump:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.read:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.write:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.array:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Regs.read(index, ops);
        break;
      case Opcode.cat:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        break;
      case Opcode.beq:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Loc.read(index, ops);
        ret.data ~= Loc.read(index, ops);
        break;
      case Opcode.blt:
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Reg.read(index, ops);
        ret.data ~= Loc.read(index, ops);
        ret.data ~= Loc.read(index, ops);
        break;
    }
    return ret;
  }
}

Instr[] parse(const(int)[] ops) {
  int index = 0;
  Instr[] ret;
  while (index < ops.length) {
    ret ~= Instr.read(index, ops);
  }
  return ret;
}
