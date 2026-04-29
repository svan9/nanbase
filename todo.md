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

native C++ puts       : 40 мкс
nanvm v1              : 317 мкс [* 7,925]
nanvm v2(30.04.26)    : 147 мкс [* 3,675]
nanvm v3(30.04.26)    : 43 мкс  [- 3]


Для 2–3x от нативного нужно три вещи в порядке приоритета:


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



Computed goto доступен в GCC/Clang и даёт ~30% прирост на интерпретаторах за счёт лучшего branch prediction.

---

## Порядок внедрения

| Шаг | Сложность | Ожидаемый прирост |
|-----|-----------|-------------------|
| Пул для `VM_REG_INFO` | низкая | 2–3x |
| Линейные блоки в JIT | средняя | 3–5x |
| Computed goto dispatch | средняя | 1.3x |

Начни с пула — это самое быстрое изменение и даст наибольший эффект именно для твоего теста с `puts`, так как там `VM_GetArg` вызывается на каждую инструкцию.