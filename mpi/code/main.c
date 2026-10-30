#include "settings.h"
#include "../../common/load_balance.h"

#include <limits.h>

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

    // Validando entrada.
    if (!(is_cyclic || is_static) || !(is_horse || is_standard) || size <= 0 || size % nprocs != 0) {
        if (rank == 0) {
            printf("Use: mpiexec -n P %s <static|cyclic> <standard|seahorse> [tamanho=4096] [saida.bin|-]\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    int n = size / nprocs;
    size_t local_count_size = (size_t)n * (size_t)size;
    if (local_count_size > INT_MAX) {
        if (rank == 0) {
            fprintf(stderr, "A contagem local excede o limite suportado por MPI_Gather.\n");
        }
        MPI_Finalize();
        return 1;
    }
    int local_count = (int)local_count_size;

    // Alocando a parte local da matriz.
    int32_t* local = NULL;
    if (!array_alloc(&local, n, size)) {
        fprintf(stderr, "Falha de alocacao no rank %d\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    double* times = NULL;
    int32_t* gathered = NULL;
    int32_t* ordered = NULL;
    if (rank == 0) {
        times = malloc((size_t)nprocs * sizeof(*times));
        if (times == NULL || !array_alloc(&gathered, size, size) ||
            (is_cyclic && !array_alloc(&ordered, size, size))) {
            fprintf(stderr, "Falha de alocacao das matrizes no processo raiz.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    // Sincroniza o inicio da fase de computacao.
    MPI_Barrier(MPI_COMM_WORLD);

    double t_start = MPI_Wtime();

    for (int k = 0; k < n; k++) {
        int py = is_cyclic ? (rank + k * nprocs) : (rank * n + k);
        calc_row(local + (size_t)k * size, py, size, is_horse);
    }

    double my_time = MPI_Wtime() - t_start;

    // Mede somente a coleta da imagem; a reordenacao ciclica fica separada.
    MPI_Barrier(MPI_COMM_WORLD);
    double gather_start = MPI_Wtime();

    MPI_Gather(local, local_count, MPI_INT32_T, gathered, local_count,
               MPI_INT32_T, 0, MPI_COMM_WORLD);

    double my_gather_time = MPI_Wtime() - gather_start;
    double master_overhead = 0.0;
    int32_t* image = gathered;
    if (rank == 0 && is_cyclic) {
        double reorder_start = MPI_Wtime();
        reorder_cyclic(gathered, ordered, size, nprocs);
        master_overhead = MPI_Wtime() - reorder_start;
        image = ordered;
    }
    double* comm_times = NULL;
    if (rank == 0) {
        comm_times = malloc((size_t)nprocs * sizeof(*comm_times));
        if (comm_times == NULL) {
            fprintf(stderr, "Falha de alocacao dos tempos de comunicacao.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    // Coleta os tempos individuais fora do tempo da coleta da imagem.
    MPI_Gather(&my_time, 1, MPI_DOUBLE, times, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(&my_gather_time, 1, MPI_DOUBLE, comm_times, 1, MPI_DOUBLE,
               0, MPI_COMM_WORLD);

    if (rank == 0) {
        double max_time = times[0], min_time = times[0];
        double max_comm = comm_times[0], min_comm = comm_times[0];
        int slowest_rank = 0, fastest_rank = 0;
        int slowest_comm_rank = 0, fastest_comm_rank = 0;

        for (int r = 0; r < nprocs; r++) {
            printf("rank_metric strategy=%s case=%s size=%d procs=%d rank=%d compute_time=%.9f gather_time=%.9f\n",
                   argv[1], argv[2], size, nprocs, r, times[r], comm_times[r]);
            if (times[r] > max_time) {
                max_time = times[r];
                slowest_rank = r;
            }
            if (times[r] < min_time) {
                min_time = times[r];
                fastest_rank = r;
            }
            if (comm_times[r] > max_comm) {
                max_comm = comm_times[r];
                slowest_comm_rank = r;
            }
            if (comm_times[r] < min_comm) {
                min_comm = comm_times[r];
                fastest_comm_rank = r;
            }
        }

        double load_balance_factor =
            calculate_load_balance_factor(min_time, max_time);
        double gather_time = comm_times[0];
        double total_time = max_time + gather_time + master_overhead;
        double write_time = 0.0;
        int output_written = 0;
        if (strcmp(output_file, "-") != 0) {
            double write_start = MPI_Wtime();
            output_written = write_output(image, (size_t)size * size,
                                          output_file);
            write_time = MPI_Wtime() - write_start;
        }

        printf("summary strategy=%s case=%s size=%d procs=%d compute_max=%.9f compute_min=%.9f gather=%.9f master_overhead=%.9f total=%.9f fbc=%.9f write=%.9f output_written=%d\n",
               argv[1], argv[2], size, nprocs, max_time, min_time,
               gather_time, master_overhead, total_time, load_balance_factor,
               write_time, output_written);
        printf("extrema compute_slowest_rank=%d compute_fastest_rank=%d gather_slowest_rank=%d gather_fastest_rank=%d gather_max=%.9f gather_min=%.9f\n",
               slowest_rank, fastest_rank, slowest_comm_rank,
               fastest_comm_rank, max_comm, min_comm);
        fflush(stdout);

        free(ordered);
        free(gathered);
        free(times);
        free(comm_times);
    }

    free(local);
    MPI_Finalize();
    return 0;
}
