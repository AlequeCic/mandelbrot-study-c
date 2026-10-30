# Como reproduzir

1. Instale o runtime e o SDK MS-MPI compatíveis.
2. Compile em um terminal de desenvolvedor do Visual Studio x64:

```powershell
cmake -S mpi/code -B build/mpi -DMSMPI_ROOT="C:/caminho/MSMPI-SDK" -DCMAKE_BUILD_TYPE=Release
cmake --build build/mpi --config Release
```

3. Execute a suíte:

```powershell
python mpi/code/run_mpi_benchmarks.py <pasta-resultados> `
  --mpi-exe build/mpi/Release/mandelbrot_mpi.exe `
  --serial-exe build/mpi/Release/serial_reference.exe `
  --mpiexec "C:/Program Files/Microsoft MPI/Bin/mpiexec.exe" `
  --openmp-results openmp/resultados_openmp_2026-10-29
```

4. Gere o relatório e os gráficos com um Python que possua Pillow:

```powershell
python mpi/code/generate_mpi_report.py <pasta-resultados>
```

A suíte usa três repetições, processos 1/2/4/8/16, forte e fraca, duas estratégias, comparação binária de corretude e exclusão da escrita em disco dos tempos oficiais.
