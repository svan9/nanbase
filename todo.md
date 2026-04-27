[ ] compact mewlib
[ ] virtual tests
[ ] virtual debug code
[ ] code lib packer
[ ] virtual `include` code with manifest file
[ ] test isolate
[ ] isolate flags for vm
[ ] minify lib
~ [ ] compat to exe
[ ] create readme for vm & asm
~ [ ] better vscode nan-asm support 
~ [ ] minify result nan-bin file
[ ] global optimization
[ ] std-pipe libs:


Для 2–3x от нативного нужно три вещи в порядке приоритета:

## 1. Убрать `new` в `VM_GetArg` (самое важное)

```cpp
// сейчас — каждый вызов аллоцирует:
VM_REG_INFO* ri = new VM_REG_INFO();

// исправить — пул или стек:
VM_REG_INFO ri;  // на стеке
arg.data2 = (byte*)&ri;  // но ri умрёт — нужен пул
```

Сделай пул на 32 записи внутри `VirtualMachine`:

```cpp
struct VirtualMachine {
  // ...
  VM_REG_INFO _reg_info_pool[32];
  int         _reg_info_idx = 0;

  VM_REG_INFO* alloc_reg_info() {
    auto* p = &_reg_info_pool[_reg_info_idx % 32];
    ++_reg_info_idx;
    return p;
  }
};
```

Это уберёт тысячи `new`/`delete` за время выполнения горячего цикла.

---

## 2. Компилировать целые линейные блоки в JIT

Сейчас JIT компилирует одну инструкцию и возвращается. Нужно компилировать весь блок до первого `CALL`/`JMP`/`RET`:

```
INC rx0    ─┐
INC rx1     ├── один нативный блок, один вызов
DEC rx2     │   вместо трёх отдельных
RET        ─┘
```

Это уберёт overhead на `vm.begin` update между инструкциями.

---

## 3. Убрать `switch` dispatch — computed goto

```cpp
// сейчас:
switch (head_byte) { case INC: VM_Inc(vm); break; ... }

// быстрее — таблица указателей:
static void* dispatch_table[] = {
  &&op_none, &&op_ldll, &&op_call, &&op_push, ...
};
goto *dispatch_table[*vm.begin++];

op_inc:
  VM_Inc(vm);
  goto *dispatch_table[*vm.begin++];
```

Computed goto доступен в GCC/Clang и даёт ~30% прирост на интерпретаторах за счёт лучшего branch prediction.

---

## Порядок внедрения

| Шаг | Сложность | Ожидаемый прирост |
|-----|-----------|-------------------|
| Пул для `VM_REG_INFO` | низкая | 2–3x |
| Линейные блоки в JIT | средняя | 3–5x |
| Computed goto dispatch | средняя | 1.3x |

Начни с пула — это самое быстрое изменение и даст наибольший эффект именно для твоего теста с `puts`, так как там `VM_GetArg` вызывается на каждую инструкцию.