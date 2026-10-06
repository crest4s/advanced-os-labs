# advanced-os-labs

C labs for the *Sistemas Operativos Avanzados* (Advanced Operating Systems) course at the University of Alcalá (UAH), 2025–26 academic year. They cover dynamic memory, virtual memory paging and the FAT32 file system.

## Labs

### PL1 — Dynamic memory (`PL1/L1.ArchivosFuenteUD1/`)

Small experiment with a dynamic memory module (`mem_dinamica.c/.h`) and a test program (`experimento_mem.c`).

### PL2 — Paging simulator (`PL2/p02/`)

Virtual memory simulator that replays a memory access trace and reports page faults for different page replacement policies:

| Program | Policy |
|---------|--------|
| `sim_pag_aleatorio` | Random |
| `sim_pag_fifo` | FIFO |
| `sim_pag_fifo2op` | FIFO with second chance |
| `sim_pag_lru` | Least Recently Used |
| `sim_pag_nfu` | Not Frequently Used |

All of them share `sim_pag_main.c` / `sim_paginacion.h`. `gen_traza` generates access traces by running a sorting algorithm (`ordenar.c`), and `contar_ops` / `calcular_cdt` are helper tools. The `salida*.txt` files are sample runs.

```bash
cd PL2/p02
make
# page size, number of frames, sorting algorithm, data order, number of elements, output mode
./sim_pag_lru 1 3 HEA DES 4 D
```

### PL3 — FAT32 volume (`PL3/`)

Reads and interprets a FAT32 volume image from a small shell (`fatsoa`): mount the image, show volume information, list directories, `stat` files and extract files following the cluster chain in the FAT (`fs_get()`). A second program, `recuperar_pdf`, recovers a deleted PDF from the volume by locating its directory entry (first byte `0xE5`) and reading its clusters.

```bash
cd PL3/P3-CODE
make
./fatsoa
FATFS:/ open fatsoa.fs
FATFS:/ volumen
FATFS:/ ls
FATFS:/ stat
```

See [`PL3/README.md`](PL3/README.md) (Spanish) for the full write-up: answers to the lab questions, the `fs_get()` algorithm and the forensic recovery steps.

## Requirements

- A C compiler and `make` (developed on macOS/arm64; the code uses POSIX system calls).
