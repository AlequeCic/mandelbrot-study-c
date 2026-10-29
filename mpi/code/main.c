#include "settings.h"

int main(int argc, char** argv){
    
    MPI_Init(&argc,&argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    
    int ok = (argc >= 3);
    int is_cyclic = ok && strcmp(argv[1], "cyclic") == 0;
    int is_static = ok && strcmp(argv[1], "static") == 0;
    int is_horse = ok && strcmp(argv[2], "seahorse") == 0;
    int is_standard = ok && strcmp(argv[2], "standard") == 0;
    int size = (argc > 3) ? atoi(argv[3]) : MAX_ROWS;

    const char* default_name = is_horse ? HORSE_MPI_ESCAPE_FILE_NAME : MPI_ESCAPE_FILE_NAME;
    const char* output_file  = (argc > 4) ? argv[4] : default_name;

    //validando entrada
    if (!(is_cyclic || is_static) || !(is_horse || is_standard) || size <= 0 || size % nprocs != 0) {
        if (rank == 0) {
            printf("Use: mpirun -np P %s <static|cyclic> <standard|seahorse> [tamanho=4096] [saida.bin]\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    //alocando matriz
    int n = size / nprocs;
    int32_t* local = NULL;
    if (!array_alloc(&local, n, size)) {
        printf("Falha de alocacao no rank %d\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    //parando todo mundo para começar do mesmo lugar
    MPI_Barrier(MPI_COMM_WORLD);

    double t_start = MPI_Wtime(); //inicio

    for (int k = 0; k < n; k++) {
        int py = is_cyclic ? (rank + k * nprocs) : (rank * n + k);
        calc_row(local + (size_t)k * size, py, size, is_horse);
    }

    double my_time = MPI_Wtime() - t_start; //fim 

    //alocando matriz para coletar o tempo de calculo
    double* times = NULL;
    int32_t* gathered = NULL;
    if (rank == 0) { //so funciona no rank 0
        times = malloc((size_t)nprocs * sizeof(double));
        array_alloc(&gathered, size, size);
    }

    // alinhando todo mundo para calcular o tempo de gather (juntar a matriz) 
    MPI_Barrier(MPI_COMM_WORLD);
    double t_comm_start = MPI_Wtime(); //inicio

    MPI_Gather(local, n * size, MPI_INT32_T, gathered, n * size, MPI_INT32_T, 0, MPI_COMM_WORLD);

    double my_comm_time = MPI_Wtime() - t_comm_start; // fim

    int32_t* image = gathered;
    if (rank == 0 && is_cyclic) {
        array_alloc(&image, size, size);
        reorder_cyclic(gathered, image, size, nprocs);
    }
    
    double* comm_times = NULL;
    if (rank == 0) comm_times = malloc((size_t)nprocs * sizeof(double));
    //coletando os tempos de calculo de cada processo
    MPI_Gather(&my_time, 1, MPI_DOUBLE, times, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(&my_comm_time, 1, MPI_DOUBLE, comm_times, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    

    //imprimindo os dados
    if (rank == 0) {
        double max_time = times[0], min_time = times[0];
        double max_comm = comm_times[0], min_comm = comm_times[0];
        int slowest_rank = 0, fastest_rank = 0;
        int slowest_comm_rank = 0, fastest_comm_rank = 0;

        printf("strategy=%s case=%s size=%d procs=%d\n", argv[1], argv[2], size, nprocs);

    for (int r = 0; r < nprocs; r++) {
        printf("rank %d: compute_time=%.6f s | comm_time=%.6f s\n",
               r, times[r], comm_times[r]);

        if (times[r] > max_time) { max_time = times[r]; slowest_rank = r; }
        if (times[r] < min_time) { min_time = times[r]; fastest_rank = r; }

        if (comm_times[r] > max_comm) { max_comm = comm_times[r]; slowest_comm_rank = r; }
        if (comm_times[r] < min_comm) { min_comm = comm_times[r]; fastest_comm_rank = r; }
    }

    double fbc = (max_time - min_time) / max_time;
    double gather_time = comm_times[0];   // custo de coleta visto pela raiz

    printf("\n--- computacao ---\n");
    printf("rank mais lento  : %d (%.6f s)\n", slowest_rank, max_time);
    printf("rank mais rapido : %d (%.6f s)\n", fastest_rank, min_time);
    printf("Balanceamento de carga = %.6f\n", fbc);

    printf("\n--- comunicacao ---\n");
    printf("tempo de coleta (rank 0)     = %.6f s\n", gather_time);
    printf("rank mais lento na comunicacao  : %d (%.6f s)\n", slowest_comm_rank, max_comm);
    printf("rank mais rapido na comunicacao : %d (%.6f s)\n", fastest_comm_rank, min_comm);

    printf("\n--- totais ---\n");
    printf("tempo_total (T_max + coleta) = %.6f s\n",
           max_time + gather_time);

    double t_write = MPI_Wtime();
    write_output(image, (size_t)size * size, output_file);
    double write_time = MPI_Wtime() - t_write;
    printf("\nTempo de escrita em disco = %.6f s\n", write_time);

    if (image != gathered) free(image);
    free(gathered);
    free(times);
    free(comm_times);
    }

    free(local);
    MPI_Finalize();


    return 0;
}