# Como reproduzir

1. Abra um terminal de desenvolvedor do Visual Studio x64.
2. Na pasta `code`, compile com:

```powershell
cl /nologo /std:c11 /O2 /W4 /WX /openmp benchmark.c serial.c paralel.c settings.c /Fe:benchmark.exe
cl /nologo /std:c11 /O2 /W4 /WX /openmp main.c serial.c paralel.c settings.c /Fe:code.exe
cl /nologo /std:c11 /O2 /W4 /WX image.c settings.c /Fe:image.exe
```

3. Crie uma pasta vazia e execute:

```powershell
.\code.exe
.\image.exe
.\benchmark.exe <pasta-de-resultados>
python .\generate_benchmark_report.py <pasta-de-resultados>
```

O benchmark realiza três repetições, mede apenas o cálculo de escape-time e limita o experimento a 16 threads quando a máquina possuir mais processadores lógicos. A saída canônica do programa `code.exe` é uma matriz row-major de `int32_t` em arquivos `.bin`; `image.exe` gera as imagens PPM correspondentes.

Para refazer somente a validação, crie outra pasta vazia e execute `./benchmark.exe --correctness-only <pasta-de-corretude>`. Esse modo recalcula as duas matrizes seriais, testa as mesmas configurações OpenMP e grava `correctness.csv` e `seahorse_load_balance.csv`. O processo retorna erro se qualquer comparação não for exata ou se o número de threads efetivamente usado for diferente do solicitado. Com 16 processadores lógicos, são 29 configurações.
