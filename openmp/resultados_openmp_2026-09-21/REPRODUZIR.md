# Como reproduzir

1. Abra um terminal de desenvolvedor do Visual Studio x64.
2. Na pasta `code`, compile com:

```powershell
cl /nologo /std:c11 /O2 /W4 /WX /openmp benchmark.c serial.c paralel.c settings.c /Fe:benchmark.exe
```

3. Crie uma pasta vazia e execute:

```powershell
.\benchmark.exe <pasta-de-resultados>
python .\generate_benchmark_report.py <pasta-de-resultados>
```

O benchmark realiza três repetições, mede apenas o cálculo de escape-time e limita o experimento a 16 threads quando a máquina possuir mais processadores lógicos.
